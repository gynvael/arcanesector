#pragma once
#include <vector>
#include <stdint.h>
#include <string>

#define WORLD_W 512
#define WORLD_H 768
#define WORLD_SZ (WORLD_W * WORLD_H)

// A container for the world map.
class WorldMap {
 public:
  struct Tile {
    uint8_t type;  // E.g. water, sand, forest, etc.
    uint8_t variant;  // Specific to a tile.
  } __attribute__((packed));

  // Load the world.
  bool load(const std::string& fname);

  std::vector<Tile> tiles;
};

