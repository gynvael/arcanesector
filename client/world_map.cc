#include "world_map.h"
#include <stdio.h>

bool WorldMap::load(const std::string& fname) {
  this->tiles.resize(WORLD_SZ);

  FILE *f = fopen(fname.c_str(), "rb");
  if (f == nullptr) {
    puts("error: world map file not found or could not be opened");
    return false;
  }

  size_t to_read = WORLD_SZ * sizeof(Tile);
  size_t read = fread(this->tiles.data(), 1, to_read, f);
  fclose(f);

  if (read != to_read) {
    puts("error: failed to read the whole world map");
    return false;
  }

  return true;
}



