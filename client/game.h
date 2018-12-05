#pragma once
#include <string>
#include <stdint.h>
#include "synced_queue.h"
#include "events.h"

class Engine;

struct Config {
  std::string ui_type;
  std::string passwd;
  std::string host_address;
  uint16_t    host_port;
  uint8_t     player_id;
};

struct NetworkingThreadContext {
  const Config *config = nullptr;

  // The networking threads are alive as long as the end flag is not set. It
  // can be used both as a way to make threads exit, or to check if they already
  // did exit (in such case the return_value is set).
  volatile bool end = false;
  volatile bool return_value;

  // Controls whether the sender thread (when true) or receiver thread (when
  // false) are able to send the packets. Only used
  volatile bool enable_sending = false;

  // Communication between threads.
  SyncedQueue<EventGameNet> *queue_game_from = nullptr;
  SyncedQueue<EventNetGame> *queue_game_to = nullptr;
};

struct GameThreadContext {
  const Config *config = nullptr;
  Engine *e = nullptr;

  // The Game thread is alive as long as the end flag is not set. It can be used
  // both as a way to make threads exit, or to check if they already did exit.
  volatile bool end = false;

  // Communication between threads.
  SyncedQueue<EventGameNet> *queue_net_to = nullptr;
  SyncedQueue<EventNetGame> *queue_net_from = nullptr;
  SyncedQueue<EventGameUI> *queue_ui_to = nullptr;
  SyncedQueue<EventUIGame> *queue_ui_from = nullptr;
};

struct UIThreadContext {
  // The UI thread is alive as long as the end flag is not set. It can be used
  // both as a way to make threads exit, or to check if they already did exit.
  volatile bool end = false;

  // Communication between threads.
  SyncedQueue<EventGameUI> *queue_game_from = nullptr;
  SyncedQueue<EventUIGame> *queue_game_to = nullptr;
};

