#include <unordered_map>
#include <iterator>
#include <algorithm>
#include <list>
#include "items_helper.h"

using sprite_map_t = std::unordered_map<std::string, item_sprite_t>;
static sprite_map_t sprite_map;

static item_sprite_t normal_sprite(const std::string& img, int tile_x, int tile_y) {
  item_sprite_t item;
  item.img = img;
  item.w3d = 0.25f;
  item.h3d = 0.25f;
  item.is_sprite = true;
  item.x = tile_x * 16;
  item.y = tile_y * 16;
  item.w = 16;
  item.h = 16;
  return item;
}

static item_sprite_t world_item(
    const std::string& img, float w3d, float h3d, float x, float y, float z) {
  item_sprite_t item;
  item.img = img;
  item.w3d = w3d;
  item.h3d = h3d;
  item.has_position = true;
  item.x3d = x;
  item.y3d = y;
  item.z3d = z;
  return item;
}

void init_sprite_map() {
  // TODO: In an ideal world this would be loaded from a config file.
  sprite_map["__default"]      = normal_sprite("items_1", 11, 11);
  sprite_map["scroll"]         = normal_sprite("items_1", 9, 3);
  sprite_map["gold_key"]       = normal_sprite("items_1", 11, 3);
  sprite_map["teleport_ring"]  = normal_sprite("items_1", 1, 1);
  sprite_map["dagger"]         = normal_sprite("items_1", 0, 7);
  sprite_map["empty_flask"]    = normal_sprite("items_1", 11, 5);
  sprite_map["unknown_potion"] = normal_sprite("items_1", 9, 5);
  sprite_map["health_potion"]  = normal_sprite("items_1", 11, 4);
  sprite_map["mana_potion"]    = normal_sprite("items_1", 12, 4);
  sprite_map["herb_0"]         = normal_sprite("items_1", 0, 12);
  sprite_map["herb_1"]         = normal_sprite("items_1", 1, 12);
  sprite_map["herb_2"]         = normal_sprite("items_1", 2, 12);
  sprite_map["herb_3"]         = normal_sprite("items_1", 3, 12);
  sprite_map["herb_4"]         = normal_sprite("items_1", 4, 12);
  sprite_map["herb_5"]         = normal_sprite("items_1", 5, 12);
  sprite_map["herb_6"]         = normal_sprite("items_1", 6, 12);
  sprite_map["herb_7"]         = normal_sprite("items_1", 0, 13);
  sprite_map["herb_8"]         = normal_sprite("items_1", 1, 13);
  sprite_map["herb_9"]         = normal_sprite("items_1", 2, 13);
  sprite_map["herb_10"]        = normal_sprite("items_1", 3, 13);
  sprite_map["herb_11"]        = normal_sprite("items_1", 4, 13);
  sprite_map["herb_12"]        = normal_sprite("items_1", 5, 13);
  sprite_map["herb_13"]        = normal_sprite("items_1", 6, 13);
  sprite_map["sign"]           = world_item("sign", 1.0f, 1.8f, 0.0f, 0.0f, 0.0f);
  sprite_map["lamppost"]       = world_item("lamppost", 0.5f, 1.5f, 0.0f, 0.0f, 0.0f);
  sprite_map["switch"]         = world_item("switch", 0.5f, 1.5f, 0.0f, 0.0f, 0.0f);
  sprite_map["switch_on"]      = world_item("switch_on", 0.5f, 1.5f, 0.0f, 0.0f, 0.0f);
  sprite_map["blocker"]        = world_item("blocker", 5.0f, 3.0f, 0.0f, 0.0f, 0.0f);
  sprite_map["blocker_open"]        = world_item("blocker_open", 5.0f, 3.0f, 0.0f, 0.0f, 0.0f);
}

std::list<std::string> item_sprite_map_keys() {
  std::list<std::string> keys;
  std::transform(
      sprite_map.begin(), sprite_map.end(),
      std::back_inserter(keys),
      [](const sprite_map_t::value_type &item) -> std::string {
        return item.first;
      }
  );

  return keys;
}

item_sprite_t item_to_sprite_info(const std::string& name) {
  auto ret = sprite_map.find(name);
  if (ret == sprite_map.end()) {
    return sprite_map["__default"];
  }

  return ret->second;
}
