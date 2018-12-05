#ifdef __unix__
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
#include "engine.h"
#include "NetSock.h"
#include "ui_common.h"

const int WS_BINARY = 2;
const int WS_STIRNG = 1;

UI_WS::~UI_WS() {
}

bool UI_WS::initialize() {
  const char *fd_str = getenv("ARCANE_FD");
  if (fd_str == nullptr) {
    fprintf(stderr,
        "error: Missing ARCANE_FD - it should have the file descriptor "
        "number of an initialized and ready to use websock\n");
    return false;
  }

  int fd = -1;
  if (sscanf(fd_str, "%i", &fd) != 1) {
    fprintf(stderr, "error: ARCANE_FD must be a number\n");
    return false;
  }

  printf("ws_info: using fd %i\n", fd);

  last_frame_request = std::chrono::steady_clock::now();

  // Don't do this at home kids.
  ws.socket = fd;
  ws.mode = NetSock::SYNCHRONIC;
  ws.SetMode(NetSock::ASYNCHRONIC);

  ok_to_request_frame = true;

  return true;
}

void UI_WS::ws_send(int type, const void *data, size_t size) {
  uint8_t header[2] = {
      (uint8_t)((1 << 7) | type),
      (uint8_t)(size > 0xffff ? 127 :
                size >= 126 ? 126 :
                size)
  };

  std::string packet;
  packet.append((const char*)header, 2);

  if (header[1] == 127) {
    uint64_t len_8 = htobe64(size);
    packet.append((const char*)&len_8, 8);
  } else if (header[1] == 126) {
    uint16_t len_2 = htobe16(size);
    packet.append((const char*)&len_2, 2);
  }

  packet.append((const char*)data, size);

  if (ws.WriteAll(packet.data(), packet.size()) == 0) {
    ws.Disconnect();
    puts("ws_error: failed while sending data");
    fflush(stdout);
    _exit(1);  // Fast exit.
  }
}

bool UI_WS::process_ws_events() {
  packet_processed = false;

  // Get some data.
  uint8_t some_data[256];
  int ret = ws.Read(some_data, sizeof(some_data));
  if (ret == 0) {
    // Read error.
    printf("ws_error: read error (%i)", errno);
    return false;
  }

  if (ret != -1 && ret > 0) {
    // There is some more data.
    auto curr_size = ws_data.size();
    ws_data.resize(curr_size + ret);
    memcpy(ws_data.data() + curr_size, some_data, ret);
  }

  // Try to process a packet. This might fail since not all data arrived, but
  // that's OK.

  if (ws_data.size() < 2 /* Minimal frame size */) {
    return true;
  }

  uint64_t processed = 0;

  const uint8_t opcode = ws_data[0] & 0xf;
  const bool fin = (bool)(ws_data[0] >> 7);
  const bool mask = (bool)(ws_data[1] >> 7);

  if (!mask) {
    puts("ws_error: missing mask, disconnecting");
    return false;
  }

  uint64_t payload_len = ws_data[1] & 0x7f;

  processed += 2;

  if (payload_len == 126) {
    if (ws_data.size() < processed + 2) {
      return true;
    }
    payload_len = be16toh(*(uint16_t*)(&ws_data[2]));
    processed += 2;
  } else if (payload_len == 127) {
    if (ws_data.size() < processed + 8) {
      return true;
    }
    payload_len = be64toh(*(uint64_t*)(&ws_data[2]));
    processed += 8;
  }

  if (payload_len > 256) {  // All our frames are around ~20 bytes anyway.
    puts("ws_error: too large frame");
    return false;
  }

  uint8_t mask_data[4]{};
  if (mask) {  // Always true btw.
    if (ws_data.size() < processed + 4) {
      return true;
    }

    mask_data[0] = ws_data[processed];
    mask_data[1] = ws_data[processed+1];
    mask_data[2] = ws_data[processed+2];
    mask_data[3] = ws_data[processed+3];
    processed += 4;
  }

  if (ws_data.size() < processed + payload_len) {
    return true;
  }

  // Packet ready!
  uint8_t *payload = ws_data.data() + processed;
  const uint64_t full_packet_size = processed + payload_len;

  if (mask) {
    for (uint64_t i = 0; i < payload_len; i++) {
      payload[i] ^= mask_data[i % 4];
    }
  }

  /*for (int i = 0; i < ws_data.size(); i++) {
    printf("%.2x ", ws_data[i]);
  }
  puts("");*/

  bool process_frame = false;
  if (opcode == 0x9 /* PING */) {
    ws_send(0xa /* PONG */, payload, payload_len);
  } else if (opcode == 0xa /* PONG */) {
    // Yay.
  } else if (opcode == 0x1 || opcode == 0x2 /* DATA */) {
    // Yay.
    if (!frame_data.empty()) {
      puts("ws_error: frame data unfinished");
      return false;
    }

    frame_data.resize(payload_len);
    memcpy(frame_data.data(), payload, payload_len);
    process_frame = fin;
  } else if (opcode == 0x0 /* CONT */) {
    if (frame_data.empty()) {
      // Weird... but I guess payload could have been 0.
      // Still weird.
      return false;
    }

    auto current_frame_size = frame_data.size();
    frame_data.resize(current_frame_size + payload_len);
    memcpy(frame_data.data() + current_frame_size, payload, payload_len);

    process_frame = fin;
  } else if (opcode == 0x8 /* CLOSE */) {
    puts("ws_info: graceful close");
    return false;
  } else {
    printf("ws_error: weird frame opcode %u\n", opcode);
    return false;
  }

  // Move the buffer.
  uint64_t remaining = ws_data.size() - full_packet_size;
  memmove(ws_data.data(), ws_data.data() + full_packet_size,
          remaining);
  ws_data.resize(remaining);

  // There might be another packet in the buffer, but that's a story for another time.
  packet_processed = true;

  // Anything to process on the frame level?
  if (process_frame) {
    return process_ws_frame();
  }

  return true;
}

bool UI_WS::process_ws_frame() {
  if (frame_data.empty()) {
    return false;
  }

  switch (frame_data[0]) {
    case 0x00: {
      if (frame_data.size() != 1) {
        return false;
      }

      frame_acked = true;
    }
    break;

    case 0x01:   // Mouse button down.
    case 0x02: { // Mouse button up.
      if (frame_data.size() != 6) {
        return false;
      }

      bool down = frame_data[0] == 0x01;

      uint16_t mx = std::clamp(frame_data[1] | (frame_data[2] << 8), 0, 427);
      uint16_t my = std::clamp(frame_data[3] | (frame_data[4] << 8), 0, 239);
      uint8_t button = frame_data[5];

      if (button == 1) {
        ctx->queue_game_to->push(EventUIGame{
            down ? EventUIGame::MOUSE_DOWN : EventUIGame::MOUSE_UP,
            key_code::UNSET, mouse_button::LEFT, mx, my});
      } else if (button == 2) {
        ctx->queue_game_to->push(EventUIGame{
            down ? EventUIGame::MOUSE_DOWN : EventUIGame::MOUSE_UP,
            key_code::UNSET, mouse_button::RIGHT, mx, my});
      } else {
        return false;
      }
    }
    break;

    case 0x03: {  // Mouse move.
      if (frame_data.size() != 5) {
        return false;
      }

      uint16_t mx = std::clamp(frame_data[1] | (frame_data[2] << 8), 0, 427);
      uint16_t my = std::clamp(frame_data[3] | (frame_data[4] << 8), 0, 239);

      ctx->queue_game_to->push(EventUIGame{
            EventUIGame::MOUSE_MOVE,
            key_code::UNSET, mouse_button::UNSET, mx, my});
    }
    break;

    case 0x10:
    case 0x11: {  // Keydown.
      if (frame_data.size() != 3) {
        return false;
      }

      bool down = frame_data[0] == 0x10;

      uint16_t code = frame_data[1] | (frame_data[2] << 8);
      ctx->queue_game_to->push(EventUIGame{
            down ? EventUIGame::KEY_DOWN : EventUIGame::KEY_UP,
            key_code::key_code_t(code)});
    }
    break;
  }

  frame_data.resize(0);
  return true;
}

void UI_WS::send_keyframe(Canvas *c) {
  // Make a copy.
  last_frame.resize(428 * 240 * 4);
  memcpy(last_frame.data(), c->d.data(), last_frame.size());

  // OK, watch this hack! To not have to copy the frame again, I'm using the
  // lowest bit of first pixel to mark a frame. It's 0 for keyframe, and non-0
  // for actual packet ID.
  c->d[0].r &= 0xfe;
  ws_send(WS_BINARY, c->d.data(), c->d.size() * 4);
}

void UI_WS::send_frame_diff(Canvas *c) {
  if (last_frame.empty() || frame_counter == 20) {
    frame_counter = 0;
    send_keyframe(c);
    return;
  }

  frame_counter++;

  std::vector<uint8_t> packet;
  packet.reserve(1 + 240 * (428 * 4 + 2));

  packet.push_back(0x01);  // Diff Frame.
  for (int i = 0; i < 240; i++) {
    if (memcmp(&c->d[i * 428], &last_frame[i * 428 * 4], 428 * 4) == 0) {
      continue;
    }

    packet.push_back((uint8_t)i);

    size_t offset = packet.size();
    packet.resize(offset + 428 * 4);
    memcpy(&packet[offset], &c->d[i * 428], 428 * 4);
  }

  ws_send(WS_BINARY, packet.data(), packet.size());

  // Make a copy.
  last_frame.resize(428 * 240 * 4);
  memcpy(last_frame.data(), c->d.data(), last_frame.size());
}

bool UI_WS::process_game_events() {
  // Frame limiter.
  if (ok_to_request_frame && frame_acked) {
    auto time_now = std::chrono::steady_clock::now();
    std::chrono::duration<float> diff = time_now - last_frame_request;
    if (diff.count() > 1.0f / UI_WS_MAX_FPS) {
      ok_to_request_frame = false;
      frame_acked = false;
      ctx->queue_game_to->push(EventUIGame{EventUIGame::REQUEST_FRAME});
    }
  }

  // Process Game events.
  EventGameUI ev;
  while (!ctx->end && ctx->queue_game_from->pop(&ev)) {
    switch (ev.type) {
      case EventGameUI::FRAME: {
        send_frame_diff(ev.frame);

        // ev.frame
        ok_to_request_frame = true;
        frame_acked = false;
        last_frame_request = std::chrono::steady_clock::now();
      }
      break;

      case EventGameUI::CURSOR: {
        std::vector<uint8_t> packet;
        if (ev.frame == nullptr) {
          packet.resize(1);
          packet[0] = 0x21;
        } else {
          packet.resize(1 + 4 + 4 + ev.frame->d.size() * 4);
          packet[0] = 0x23;

          uint16_t hx = uint16_t(ev.hot_x + 0x8000);
          uint16_t hy = uint16_t(ev.hot_y + 0x8000);

          packet[1] = hx & 0xff;
          packet[2] = (hx >> 8);

          packet[3] = hy & 0xff;
          packet[4] = (hy >> 8);

          uint16_t w = uint16_t(ev.frame->w);
          uint16_t h = uint16_t(ev.frame->h);

          packet[5] = w & 0xff;
          packet[6] = (w >> 8);

          packet[7] = h & 0xff;
          packet[8] = (h >> 8);

          memcpy(&packet[9], ev.frame->d.data(), ev.frame->d.size() * 4);
        }
        ws_send(WS_BINARY, packet.data(), packet.size());
      }
      break;

      case EventGameUI::MUSIC: {
        // TODO
        printf("todo: playing music \"%s\"\n", ev.id->c_str());
      }
      break;

      case EventGameUI::SFX: {
        // TODO
        printf("todo: playing sound effect \"%s\"\n", ev.id->c_str());
      }
      break;
    }

    // Cleanup.
    if (ev.id != nullptr) {
      delete ev.id;
    }
  }

  return true;
}

bool UI_WS::process_events() {
  bool ret = true;
  ret &= process_ws_events();
  ret &= process_game_events();
  return ret;
}

bool UI_WS::ok_to_yield() {
  return !packet_processed;  // Sleep a while.
}


#else
// Not implemented, but should compile.
#include "ui_common.h"

UI_WS::~UI_WS() {
}

bool UI_WS::initialize() {
  puts("Not implemented (no time for cross-platform stuff for above code).");
  return false;
}

bool UI_WS::process_events() {
  return false;
}

bool UI_WS::ok_to_yield() {
  return false;
}


#endif
