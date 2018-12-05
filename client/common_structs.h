#pragma once
#include <string>
#include <vector>

#define ITEM_NON_EXISTING_ID (0xffffffffffffffffULL)

struct SimpleItem {
  uint64_t id = ITEM_NON_EXISTING_ID;
  bool movable = false;
  std::string gfx_id = "";
  std::string name = "";
};

struct SimpleMob {
  uint8_t type{};
  bool visible{};
  uint64_t id{};
  uint16_t pos_x{};
  uint16_t pos_y{};
  std::string gfx_id{};
  std::string name{};
};

struct SimpleItemList {
  uint16_t pos_x{}, pos_y{};
  std::vector<SimpleItem> items;
};
