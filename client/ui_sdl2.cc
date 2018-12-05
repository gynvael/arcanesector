#include <SDL2/SDL.h>
#include <algorithm>
#include "ui_common.h"
#include "engine.h"


UI_SDL2::~UI_SDL2() {
  if (this->win != nullptr) {
    SDL_DestroyWindow(this->win);
  }

  SDL_Quit();
}

bool UI_SDL2::ok_to_yield() {
  return false; // Interactive UI, no sleeping.
}

bool UI_SDL2::initialize() {
  if (SDL_Init(SDL_INIT_VIDEO |
               SDL_INIT_AUDIO |
               SDL_INIT_TIMER |
               SDL_INIT_EVENTS) != 0) {
    fprintf(stderr, "error: SDL_Init failed\n");
    return false;
  }

  this->win = SDL_CreateWindow(
    "Arcane Sector", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    WIDTH_UI * 2, HEIGHT_UI * 2,
    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (this->win == nullptr) {
    fprintf(stderr, "error: SDL_CreateWindow failed\n");
    return false;
  }

  this->win_surface = SDL_GetWindowSurface(this->win);
  if (this->win_surface == nullptr) {
    fprintf(stderr, "error: SDL_GetWindowSurface failed\n");
    return false;
  }

  // Alright, at this point we can request the first frame!
  ctx->queue_game_to->push(EventUIGame{EventUIGame::REQUEST_FRAME});

  return true;
}

bool UI_SDL2::process_events() {
  bool ret = true;
  ret &= process_SDL2_events();
  ret &= process_game_events();
  ret &= process_SDL2_events();  // SDL2 has to turns.
  return ret;
}

key_code::key_code_t UI_SDL2::convert_keysym(SDL_Keycode sym) {
  if (sym >= SDLK_a && sym <= SDLK_z) {
    return key_code::key_code_t((int)sym - (int)SDLK_a + (int)key_code::A);
  }

  if (sym >= SDLK_0 && sym <= SDLK_9) {
    return key_code::key_code_t(sym);  // Direct mapping.
  }

  switch (sym) {
    case SDLK_UP: return key_code::ARROW_UP;
    case SDLK_DOWN: return key_code::ARROW_DOWN;
    case SDLK_LEFT: return key_code::ARROW_LEFT;
    case SDLK_RIGHT: return key_code::ARROW_RIGHT;

    case SDLK_LSHIFT: return key_code::SHIFT_LEFT;
    case SDLK_RSHIFT: return key_code::SHIFT_RIGHT;

    case SDLK_LCTRL: return key_code::CTRL_LEFT;
    case SDLK_RCTRL: return key_code::CTRL_RIGHT;

    case SDLK_LALT: return key_code::ALT_LEFT;
    case SDLK_RALT: return key_code::ALT_RIGHT;

    case SDLK_CAPSLOCK: return key_code::CAPSLOCK;

    case SDLK_KP_0: return key_code::KEYPAD_0;
    case SDLK_KP_1: return key_code::KEYPAD_1;
    case SDLK_KP_2: return key_code::KEYPAD_2;
    case SDLK_KP_3: return key_code::KEYPAD_3;
    case SDLK_KP_4: return key_code::KEYPAD_4;
    case SDLK_KP_5: return key_code::KEYPAD_5;
    case SDLK_KP_6: return key_code::KEYPAD_6;
    case SDLK_KP_7: return key_code::KEYPAD_7;
    case SDLK_KP_8: return key_code::KEYPAD_8;
    case SDLK_KP_9: return key_code::KEYPAD_9;
    case SDLK_KP_ENTER: return key_code::KEYPAD_ENTER;

    case SDLK_QUOTE: return key_code::QUOTE;
    case SDLK_COMMA: return key_code::COMMA;
    case SDLK_MINUS: return key_code::MINUS;
    case SDLK_PERIOD: return key_code::PERIOD;
    case SDLK_SLASH: return key_code::SLASH;
    case SDLK_SEMICOLON: return key_code::SEMICOLON;
    case SDLK_EQUALS: return key_code::EQUALS;
    case SDLK_LEFTBRACKET: return key_code::SQUARE_LEFT;
    case SDLK_RIGHTBRACKET: return key_code::SQUARE_RIGHT;
    case SDLK_BACKSLASH: return key_code::BACKSLASH;
    case SDLK_BACKQUOTE: return key_code::BACKTICK;

    case SDLK_RETURN: return key_code::ENTER;
    case SDLK_SPACE: return key_code::SPACE;
    case SDLK_BACKSPACE: return key_code::BACKSPACE;
    case SDLK_TAB: return key_code::TAB;
    case SDLK_ESCAPE: return key_code::ESCAPE;
  }

  if (sym == SDLK_UNKNOWN) {
    return key_code::key_code_t(-0xffff);
  }

  return key_code::key_code_t(-sym);
}

std::pair<int, int> UI_SDL2::convert_mouse_coords(int x, int y) {
  if (win_surface == nullptr) {
    return {0, 0};
  }

  return {
      std::clamp((x * 428) / win_surface->w, 0, 427),
      std::clamp((y * 240) / win_surface->h, 0, 239)
  };
}

bool UI_SDL2::process_SDL2_events() {
  SDL_PumpEvents();

  SDL_Event ev;
  while (!ctx->end && SDL_PeepEvents(
      &ev, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) > 0) {

    if (ev.type == SDL_QUIT) {
      ctx->queue_game_to->push(EventUIGame{EventUIGame::EXIT});
      ctx->end = true;
      return false;
    }

    if (ev.type == SDL_WINDOWEVENT) {
      if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        this->win_surface = SDL_GetWindowSurface(this->win);
        if (this->win_surface == nullptr) {
          fprintf(stderr, "error: SDL_GetWindowSurface failed\n");
          return false;
        }
        continue;
      }

      if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
        SDL_Event quit_ev;
        quit_ev.type = SDL_QUIT;
        SDL_PushEvent(&quit_ev);
        continue;
      }
    }

    if (ev.type == SDL_KEYDOWN ||
        ev.type == SDL_KEYUP) {
      key_code::key_code_t engine_key_code =
         this->convert_keysym(ev.key.keysym.sym);
      if (engine_key_code >= 0) {
        ctx->queue_game_to->push(EventUIGame{
            ev.type == SDL_KEYDOWN ? EventUIGame::KEY_DOWN : EventUIGame::KEY_UP,
            engine_key_code});
      } else {
        // The engine would not care about this key.
      }

    }

    if (ev.type == SDL_MOUSEBUTTONDOWN) {
      mx = ev.button.x;
      my = ev.button.y;
      auto [x, y] = convert_mouse_coords(ev.button.x, ev.button.y);
      if (ev.button.button == SDL_BUTTON_LEFT) {
        ctx->queue_game_to->push(EventUIGame{
            EventUIGame::MOUSE_DOWN, key_code::UNSET, mouse_button::LEFT, x, y});
      } else if (ev.button.button == SDL_BUTTON_RIGHT) {
        ctx->queue_game_to->push(EventUIGame{
            EventUIGame::MOUSE_DOWN, key_code::UNSET, mouse_button::RIGHT, x, y});
      }  // Ignore other buttons.
    }

    if (ev.type == SDL_MOUSEBUTTONUP) {
      mx = ev.button.x;
      my = ev.button.y;
      auto [x, y] = convert_mouse_coords(ev.button.x, ev.button.y);
      if (ev.button.button == SDL_BUTTON_LEFT) {
        ctx->queue_game_to->push(EventUIGame{
            EventUIGame::MOUSE_UP, key_code::UNSET, mouse_button::LEFT, x, y});
      } else if (ev.button.button == SDL_BUTTON_RIGHT) {
        ctx->queue_game_to->push(EventUIGame{
            EventUIGame::MOUSE_UP, key_code::UNSET, mouse_button::RIGHT, x, y});
      }  // Ignore other buttons.
    }

    if (ev.type == SDL_MOUSEMOTION) {
      mx = ev.motion.x;
      my = ev.motion.y;
      auto [x, y] = convert_mouse_coords(ev.motion.x, ev.motion.y);
      ctx->queue_game_to->push(EventUIGame{
          EventUIGame::MOUSE_MOVE, key_code::UNSET, mouse_button::UNSET, x, y});
    }
  }

  return true;
}

bool UI_SDL2::process_game_events() {
  // Process Game events.
  EventGameUI ev;
  while (!ctx->end && ctx->queue_game_from->pop(&ev)) {
    switch (ev.type) {
      case EventGameUI::FRAME: {
        // Convert the native-to-engine canvas to an SDL surface.
        // TODO: This probably can be optimized a little if surface formats
        // match (especially if we'll need to do.
        SDL_Surface *frame = ev.frame->to_surface();

        // Continue with copying the frame.
        SDL_BlitScaled(frame, NULL, this->win_surface, NULL);

        // Blit-in the cursor as well.
        // TODO: Redraw the cursor each time the mouse moves, and not only on
        // new frames (there is a lag in such case which doesn't have to be
        // there).
        if (cursor != nullptr) {
          const float scale_x = float(this->win_surface->w) / 428.0f;
          const float scale_y = float(this->win_surface->h) / 240.0f;
          SDL_Rect dstrect = {
            this->mx - int(float(this->cursor_hot_x) * scale_x),
            this->my - int(float(this->cursor_hot_y) * scale_y),
            int(float(this->cursor->w) * scale_x),
            int(float(this->cursor->h) * scale_y)
          };

          SDL_BlitScaled(cursor, NULL, this->win_surface, &dstrect);
        }


        SDL_UpdateWindowSurface(this->win);
        SDL_FreeSurface(frame);

        // At this point a new frame can be requested (this will invalidate
        // the canvas, but not the converted surface; the canvas must not be
        // freed).
        ctx->queue_game_to->push(EventUIGame{EventUIGame::REQUEST_FRAME});
      }
      break;

      case EventGameUI::CURSOR: {
        if (cursor != nullptr) {
          SDL_FreeSurface(cursor);
        }

        if (ev.frame == nullptr) {
          cursor = nullptr;
        } else {
          cursor = ev.frame->to_surface();
          cursor_hot_x = ev.hot_x;
          cursor_hot_y = ev.hot_y;
        }
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

