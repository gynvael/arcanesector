#pragma once
#include <chrono>
#include <vector>
#include <string>
#include <unordered_map>
#include "engine.h"
#include "game.h"
#include "gamestate.h"
#include "events.h"
#include "packets.h"

class GameLogic {
 public:
  void main(GameThreadContext *ctx);

 protected:
  friend class Engine;

  class TextInputSubsystem {
   public:
    void reset(std::string *s);  // Can be nullptr.
    void handle(EventUIGame *ev);
    void handle_repeats();  // Called from main loop TODO
    void emit();

   private:
    const float REPEAT_INITIAL_DELAY = 1.0f;  // Seconds.
    const float REPEAT_COOLDOWN = 0.50f;  // Seconds.

    std::string *text = nullptr;

    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool capslock = false;

    char repeated_char = 0;  // None if 0.
    bool repeated_fast = false;
    std::chrono::time_point<std::chrono::steady_clock> last_emit;

  };

  // These return true if any event was processed.
  bool process_net_event();
  bool process_ui_event();

  bool process_ui_event_splashscreen(EventUIGame *ev);
  bool process_ui_event_console(EventUIGame *ev);
  bool process_ui_event_game_chat(EventUIGame *ev);
  bool process_ui_event_game(EventUIGame *ev);

  void process_console_command(const std::string& t);

  void cast_spell();

  void console_command_help(std::string command, std::vector<std::string> args);
  void console_command_quit(std::string command, std::vector<std::string> args);
  void console_command_cfg_dump(std::string command,
                                std::vector<std::string> args);
  void console_command_cfg_scheme(std::string command,
                                  std::vector<std::string> args);
  void console_command_cfg_passwd(std::string command,
                                  std::vector<std::string> args);
  void console_conf_decrypt(std::vector<uint8_t>& data);
  void console_hexii_dump(uint8_t *data, size_t sz);

  void scene_switch(GameState::game_scene_t scene);
  void game_input_switch(GameState::game_input_t input);

  std::vector<std::string> split_string(const std::string& t);

  static const uint8_t MOVE_FORWARD = 0;
  static const uint8_t MOVE_BACKWARD = 1;
  static const uint8_t MOVE_STRAFE_LEFT = 2;
  static const uint8_t MOVE_STRAFE_RIGHT = 3;
  static const uint8_t NORTH = 0;
  static const uint8_t SOUTH = 1;
  static const uint8_t WEST = 2;
  static const uint8_t EAST = 3;

  GameState state;
  TextInputSubsystem text_input;
  GameThreadContext *ctx = nullptr;

  // Console-related fields.
  typedef void (GameLogic::*console_method_t)(std::string, std::vector<std::string>);
  std::unordered_map<std::string, console_method_t> console_commands;

  std::string console_conf_password;
  enum conf_scheme_t {
    CONF_SCHEME_NONE,
    CONF_SCHEME_MD5_AES_128_EBC  // MD5 for password to key derivation.
  } console_conf_scheme;
};

