#pragma once
#include <SDL2/SDL.h>
#include <utility>
#include <chrono>
#include <vector>
#include <stdint.h>
#include "game.h"
#include "NetSock.h"

class Canvas;

const float UI_WS_MAX_FPS = 10;

class UI {
 public:
  UI(UIThreadContext *ctx) { this->ctx = ctx; }
  virtual ~UI() {}

  virtual bool initialize() = 0;
  virtual bool process_events() = 0;
  virtual void join() {};  // Wait for thread to finish (if any).

  // Is the main thread allowed to sleep a few ms?
  virtual bool ok_to_yield() = 0;

 protected:
  UIThreadContext *ctx;
};

// Only implemented on Linux.
class UI_WS : public UI {
 public:
  using UI::UI;  // Inherit constructor.
  ~UI_WS();
  bool initialize() override;
  bool process_events() override;
  bool ok_to_yield() override;

#ifdef __unix__
 private:
  NetSock ws;
  std::vector<uint8_t> ws_data;
  std::vector<uint8_t> frame_data;

  std::vector<uint8_t> last_frame;
  unsigned int frame_counter = 0;

  std::chrono::time_point<std::chrono::steady_clock> last_frame_request;
  bool ok_to_request_frame = false;
  bool packet_processed = false;
  bool frame_acked = true;  // Did the thin client ack the last frame?

  bool process_ws_frame();
  bool process_ws_events();
  bool process_game_events();

  void send_frame_diff(Canvas *c);
  void send_keyframe(Canvas *c);

  void ws_send(int type, const void *data, size_t size);
#endif
};

class UI_SDL2 : public UI {
 public:
  using UI::UI;  // Inherit constructor.
  ~UI_SDL2();
  bool initialize() override;
  bool process_events() override;
  bool ok_to_yield() override;

 private:
  bool process_game_events();
  bool process_SDL2_events();
  key_code::key_code_t convert_keysym(SDL_Keycode sym);
  std::pair<int, int> convert_mouse_coords(int x, int y);

  SDL_Window *win = nullptr;
  SDL_Surface *win_surface = nullptr;
  SDL_Surface *cursor = nullptr;
  int cursor_hot_x = 0;
  int cursor_hot_y = 0;
  int mx = 0;
  int my = 0;
};

