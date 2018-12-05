#pragma once
#include <string>

class PacketsSC;
class PacketsCS;
class Canvas;

namespace mouse_button {
  enum mouse_button_t : int {
    UNSET = 0,
    LEFT = 1,
    RIGHT = 2
  };
};

namespace key_code {
  enum key_code_t : int {
    UNSET = 0,

    // Special keys not directly mapped to ASCII table.
    ARROW_UP    = 0x1000,
    ARROW_DOWN  = 0x1001,
    ARROW_LEFT  = 0x1002,
    ARROW_RIGHT = 0x1003,
    SHIFT_LEFT  = 0x1010,
    SHIFT_RIGHT = 0x1011,
    CTRL_LEFT   = 0x1020,
    CTRL_RIGHT  = 0x1021,
    ALT_RIGHT   = 0x1030,
    ALT_LEFT    = 0x1031,
    CAPSLOCK    = 0x1040,

    KEYPAD_0     = 0x2000,
    KEYPAD_1     = 0x2001,
    KEYPAD_2     = 0x2002,
    KEYPAD_3     = 0x2003,
    KEYPAD_4     = 0x2004,
    KEYPAD_5     = 0x2005,
    KEYPAD_6     = 0x2006,
    KEYPAD_7     = 0x2007,
    KEYPAD_8     = 0x2008,
    KEYPAD_9     = 0x2009,
    KEYPAD_ENTER = 0x200a,

    // Mimic ASCII codes from here.
    NUM_0 = 0x30,  // Not keypad! Just normal numbers.
    NUM_1 = 0x31,
    NUM_2 = 0x32,
    NUM_3 = 0x33,
    NUM_4 = 0x34,
    NUM_5 = 0x35,
    NUM_6 = 0x36,
    NUM_7 = 0x37,
    NUM_8 = 0x38,
    NUM_9 = 0x39,
    A = 0x41,
    B = 0x42,
    C = 0x43,
    D = 0x44,
    E = 0x45,
    F = 0x46,
    G = 0x47,
    H = 0x48,
    I = 0x49,
    J = 0x4a,
    K = 0x4b,
    L = 0x4c,
    M = 0x4d,
    N = 0x4e,
    O = 0x4f,
    P = 0x50,
    Q = 0x51,
    R = 0x52,
    S = 0x53,
    T = 0x54,
    U = 0x55,
    V = 0x56,
    W = 0x57,
    X = 0x58,
    Y = 0x59,
    Z = 0x5a,

    QUOTE        = 0x27,
    COMMA        = 0x2C,
    MINUS        = 0x2D,
    PERIOD       = 0x2E,
    SLASH        = 0x2F,
    SEMICOLON    = 0x3B,
    EQUALS       = 0x3D,
    SQUARE_LEFT  = 0x5B,
    SQUARE_RIGHT = 0x5D,
    BACKSLASH    = 0x5C,
    BACKTICK     = 0x60,

    ENTER = 0x0a,  // There is also KEYPAD_ENTER.
    SPACE = 0x20,
    BACKSPACE = 0x08,
    TAB = 0x09,
    ESCAPE = 0x1b
  };
}

// These might be copied around a lot.

// Events shared from the networking thread to the game thread.
struct EventNetGame {
  enum {
    PACKET,        // A network packet was received and needs to be processed
                   // by the game engine.
    DISCONNECT     // Connection was lost.
  } type;

  // Set if PACKET type (receiver owns the packet).
  PacketsSC *packet = nullptr;
};

// Events shared from the game thread to the networking thread.
struct EventGameNet {
  enum {
    PACKET         // A request to send the packet.
  } type;

  // Set if Packet type (receiver owns the packet).
  PacketsCS *packet = nullptr;
};

// Events shared from the UI thread to the game thread.
struct EventUIGame {
  enum {
    REQUEST_FRAME,
    KEY_DOWN,
    KEY_UP,
    MOUSE_DOWN,
    MOUSE_UP,
    MOUSE_MOVE,
    EXIT
  } type;

  // Set for KEY_DOWN, KEY_UP.
  key_code::key_code_t key_code = key_code::UNSET;

  // Set for MOUSE_DOWN, MOUSE_UP.
  mouse_button::mouse_button_t mouse_button = mouse_button::UNSET;

  // Set for MOUSE_DOWN, MOUSE_UP, MOUSE_MOVE.
  int mx = -1, my = -1;
};

// Events shared from the game thread to the UI thread.
struct EventGameUI {
  enum {
    FRAME,      // A certain magical frame.
    CURSOR,     // Mouse cursor (can be nullptr for default).
    MUSIC,      // Change music to this track.
    SFX         // Play given sound FX.
  } type;

  // Set for FRAME or CURSOR (receiver DOES NOT own the canvas).
  Canvas *frame = nullptr;

  // Set for MUSIC and SFX types (receiver owns the string).
  std::string *id = nullptr;

  // Set for CURSOR.
  int hot_x = 0, hot_y = 0;
};
