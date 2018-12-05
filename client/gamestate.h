#pragma once
#include <string>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <stdint.h>
#include "common_structs.h"

const int NORTH = 0;
const int SOUTH = 1;
const int WEST = 2;
const int EAST = 3;

const int HAND_LEFT = 0;
const int HAND_RIGHT = 1;

const uint64_t MOB_MASK = 0x8000000000000000ULL;

struct GameState {
  std::chrono::steady_clock::time_point now;

  // Mouse coords.
  int mx = 0;
  int my = 0;

  // Coords where the button was last pushed down.
  int mx_down_left = -1;
  int my_down_left = -1;
  int mx_down_right = -1;
  int my_down_right = -1;

  bool mleft = false;  // Left mouse button down.
  bool mright = false;  // Left mouse button up.

  // Previous states in case of a change (can be used to determine if a click
  // happened, or a drag & drop finished).
  // Updated by the engine.
  bool mleft_previous = false;
  bool mright_previous = false;

  enum game_scene_t {
    SCENE_SPLASHSCREEN,
    SCENE_GAME,
    SCENE_CONSOLE
  } game_scene = SCENE_GAME;

  // SCENE_GAME variables.
  int player_x = 0;
  int player_y = 0;
  int player_dir = NORTH;

  int player_hp_max = 100;
  int player_hp = 0;
  int player_mana_max = 100;
  int player_mana = 0;

  int player_gender = 0;

  std::string player_name;

  SimpleItem equiped[2];
  SimpleItem inventory[8];
  SimpleItem holding;
  bool selecting = false;
  uint64_t selecting_packet_id;

  // World map.
  static inline uint64_t coords_to_key(int x, int y) {
    uint64_t ux = (uint64_t)(uint32_t)x;
    uint64_t uy = (uint64_t)(uint32_t)y;
    return ux | (uy << 32);
  }

  static inline std::pair<int, int> key_to_coords(uint64_t k) {
    uint32_t ux = (uint32_t)k;
    uint32_t uy = (uint32_t)(k >> 32);
    int x, y;
    memcpy(&x, &ux, 4);
    memcpy(&y, &uy, 4);
    return {x, y};
  }

  // Items/Mobs on the ground.
  std::unordered_map<
      uint64_t,
      std::vector<SimpleItem>> ground_items;

  std::unordered_map<
      uint64_t,
      std::vector<SimpleMob>> ground_mobs;

  // Item ID to SimpleItem map.
  std::unordered_map<uint64_t, SimpleItem> item_id_to_item;
  std::unordered_map<uint64_t, SimpleMob> mob_id_to_mob;

  enum game_hud_t {
    HUD_INVENTORY,
    HUD_SPELL
  } game_hud = HUD_INVENTORY;

  enum game_input_t {
    INPUT_GAME,
    INPUT_CHAT
  } game_input = INPUT_GAME;

  // Spell casting.
  uint8_t spell[8]{};
  int spell_length = 0;

  // Signals from engine to logic to be handled after a frame has been rendered.
  // Yeah, I know it's weird for the engine to prepare these, but given the time
  // constrains it's just shorter code.
  bool clicked_spell_cast = false;
  uint64_t wants_to_hold_item = ITEM_NON_EXISTING_ID;
  uint64_t wants_to_use_item = ITEM_NON_EXISTING_ID;
  bool wants_to_drop_item = false;
  uint8_t item_drop_dst = 255;
  bool wants_to_select = false;
  uint8_t selection_target_type = 255;
  uint64_t selection_target = 0;  // Not used for ground.

};

