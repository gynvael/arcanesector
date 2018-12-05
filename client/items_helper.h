#pragma once
#include <string>
#include <list>

// TODO: A reasonable thing to do is to have this in a class or at least a
// namespace.

struct item_sprite_t {
  std::string img;
  float w3d, h3d;  // Size in meters.
  bool has_position = false;  // Preferred position is set.
  float x3d = 0.0f, y3d = 0.0f, z3d = 0.0f;
  bool is_sprite = false;  // If false, x, y, w, h are unset.
  int x = 0, y = 0;  // In pixels.
  int w = 0, h = 0;  // In pixels.
};

void init_sprite_map();
std::list<std::string> item_sprite_map_keys();
item_sprite_t item_to_sprite_info(const std::string& name);

