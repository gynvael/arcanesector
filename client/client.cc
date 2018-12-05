#define __USE_MINGW_ANSI_STDIO 1
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdint.h>
#include <memory>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include <list>
#include <cassert>

using namespace std::string_literals;

#include "synced_queue.h"
#include "packets.h"  // Technically a .cc file. Don't ask.
#include "engine.h"
#include "events.h"
#include "game.h"
#include "ui_common.h"
#include "logic.h"

bool networking_sender_main_worker(NetworkingThreadContext *ctx, NetSock *s) {
  if (!PacketsCS_ENTR::make(ctx->config->passwd,
                            ctx->config->player_id)->send(s)) {
    return false;
  }

  while (!ctx->end) {
    EventGameNet ev;

    if (!ctx->queue_game_from->pop(&ev)) {
      // Nothing to send yet. Sleep a while (a 5ms wake-up lag doesn't matter).
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }

    assert(ev.type == EventGameNet::PACKET);  // Only supported type.
    std::unique_ptr<PacketsCS> p(ev.packet);

    if (!ev.packet->send(s)) {
      return false;
    }
  }

  // Last packet we send is a GoodBYE to the server.
  if (!PacketsCS_GBYE::make()->send(s)) {
    return false;
  }

  return true;
}

void networking_sender_main(NetworkingThreadContext *ctx, NetSock *s) {
  ctx->return_value &= networking_sender_main_worker(ctx, s);
  ctx->end = true;
}

bool networking_main_worker(NetworkingThreadContext *ctx, NetSock *s) {
  bool first_packet = true;

  while (!ctx->end) {
    auto p = PacketsSC::recv(s);
    if (p == nullptr) {
      return false;
    }

    if (p->get_chunk_id() == "NOPC"s) {
      if (!first_packet) {
        return false;  // NOPC can only be sent as first packet.
      }
    }
    first_packet = false;

    ctx->queue_game_to->push(
        EventNetGame{EventNetGame::PACKET, p.release()});
  }

  return true;
}

void networking_main(NetworkingThreadContext *ctx) {
  NetSock s;
  if (!s.Connect(ctx->config->host_address.c_str(), ctx->config->host_port)) {
    fprintf(stderr, "error: could not connect to game server\n");
    ctx->return_value = false;
    ctx->end = true;
    return;
  }

  ctx->return_value = true;  // Default, will be changed by either threads.

  std::thread sender(networking_sender_main, ctx, &s);
  ctx->return_value &= networking_main_worker(ctx, &s);
  ctx->end = true;

  ctx->queue_game_to->push(EventNetGame{EventNetGame::DISCONNECT});

  sender.join();
}

void game_main(GameThreadContext *ctx) {
  GameLogic logic;
  logic.main(ctx);
}

bool game(const Config *config) {
  // Initialize the game engine before starting the threads.
  Engine e;
  if (!e.initialize()) {
    puts("error: engine initialization failed");
    return false;
  }

  // Queues for cross-thread communication.
  SyncedQueue<EventGameNet> queue_game_net;
  SyncedQueue<EventNetGame> queue_net_game;
  SyncedQueue<EventGameUI> queue_game_ui;
  SyncedQueue<EventUIGame> queue_ui_game;

  // Prepare the UI context (it might run in this thread synchronously, or in
  // another thread, depending got the UI class).
  UIThreadContext ui_ctx;
  ui_ctx.queue_game_from = &queue_game_ui;
  ui_ctx.queue_game_to = &queue_ui_game;

  std::unique_ptr<UI> ui;
  if (config->ui_type == "SDL2") {
    ui.reset(new UI_SDL2(&ui_ctx));
  }

  if (config->ui_type == "WEBSOCKET") {
    ui.reset(new UI_WS(&ui_ctx));
  }

  if (ui == nullptr) {
    fprintf(stderr, "error: unknown UI type %s\n", config->ui_type.c_str());
    return false;
  }

  if (!ui->initialize()) {
    fprintf(stderr, "error: failed to initialize UI\n");
    return false;
  }

  // Prepare and start networking thread.
  NetworkingThreadContext net_ctx;
  net_ctx.config = config;
  net_ctx.queue_game_from = &queue_game_net;
  net_ctx.queue_game_to = &queue_net_game;
  std::thread net(networking_main, &net_ctx);

  // Prepare and start game thread.
  GameThreadContext game_ctx;
  game_ctx.config = config;
  game_ctx.e = &e;
  game_ctx.queue_net_to = &queue_game_net;
  game_ctx.queue_net_from = &queue_net_game;
  game_ctx.queue_ui_to = &queue_game_ui;
  game_ctx.queue_ui_from = &queue_ui_game;
  std::thread game(game_main, &game_ctx);

  // Run the game until the networking thread or the UI finish it.
  bool ret = true;
  while (true) {
    // TODO: call a "handle ui" function with a list of events
    // that are to be translated to game events.

    // Check if the networking thread exited.
    if (net_ctx.end) {
      ret = net_ctx.return_value;
      break;
    }

    if (!ui->process_events()) {
      break;
    }

    if (game_ctx.end) {
      ret = true;
      break;
    }

    if (ui->ok_to_yield()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  // Finish all the threads.
  puts("Finishing..."); fflush(stdout);

  net_ctx.end = true;
  game_ctx.end = true;
  ui_ctx.end = true;
  ui->join();
  game.join();
  net.join();

  return ret;
}

#undef main
int main(int argc, char **argv) {
  // TODO: Move the connection (& reconnecting) to the networking thread.
  const char *passwd = getenv("ARCANE_PASSWD");
  if (passwd == nullptr) {
    fprintf(stderr, "error: ARCANE_PASSWD environment variable not set.\n");
    return 1;
  }

  const char *host = getenv("ARCANE_HOST");
  if (host == nullptr) {
    fprintf(stderr, "error: ARCANE_HOST needs to be set to host:port.\n");
    return 2;
  }

  const char *ui_type = getenv("ARCANE_UI_TYPE");
  if (ui_type == nullptr) {
    ui_type = "SDL2";
  }

  char host_address[256]{};
  uint16_t host_port;
  if (sscanf(host, "%255[^:]:%hu", host_address, &host_port) != 2) {
    fprintf(stderr,
            "error: ARCANE_HOST has incorrect format (use host:port).\n");
    return 2;
  }

  if (argc != 2) {
    fprintf(stderr, "usage: client <player_id>\n"
                    "note : player_id has to be from 0 to 255\n");
    return 3;
  }

  uint8_t player_id;
  if (sscanf(argv[1], "%hhu", &player_id) != 1) {
    fprintf(stderr, "error: player_id out of range (0-255)\n");
    return 4;
  }

  NetSock::InitNetworking();

  Config config{
      ui_type,
      passwd,
      host_address, host_port,
      player_id
  };

  // TODO: reconnect on disconnect
  if (!game(&config)) {
    fprintf(stderr, "error: server disconnected\n");
  }  // Otherwise it was a planned disconnect.


  return 0;
}
