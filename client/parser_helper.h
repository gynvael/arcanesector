#pragma once
#include <vector>
#include <string>
#include <cstdio>
#include <stdint.h>
#include "common_structs.h"

using bytes_t = std::vector<uint8_t>;

class Parser {
 public:
  Parser(const bytes_t& data)
    : data_{data}, sz_{data.size()}, idx_{0}, error_{false} {}

  uint8_t read_uint8() {
    if (error_) {
      return 0;
    }

    if (idx_ < sz_) {
      return data_.at(idx_++);
    }

    error_ = true;  // Allow for lazy error checking.
    return 0;
  }

  uint16_t read_uint16() {
    uint16_t val = read_uint8();
    val |= (uint16_t)read_uint8() << 8;
    return val;
  }

  uint32_t read_uint32() {
    uint32_t val = read_uint16();
    val |= (uint32_t)read_uint16() << 16;
    return val;
  }

  uint64_t read_uint64() {
    uint64_t val = read_uint32();
    val |= (uint64_t)read_uint32() << 32;
    return val;
  }

  std::string read_string() {
    size_t string_sz = read_uint16();
    if (error_) {
      return "";
    }

    if (idx_ + string_sz > sz_) {
      error_ = true;
      return "";
    }

    std::string s((const char*)&data_[idx_], string_sz);
    idx_ += string_sz;
    return s;
  }

  SimpleItem read_item() {
    SimpleItem item;

    item.id = read_uint64();

    if (!error_ && item.id != ITEM_NON_EXISTING_ID) {
      item.movable = (bool)read_uint8();
      item.gfx_id = read_string();
      item.name = read_string();
    }

    return item;
  }

  SimpleItemList read_itemlist() {
    SimpleItemList itemlist;
    uint8_t count = read_uint8();
    itemlist.pos_x = read_uint16();
    itemlist.pos_y = read_uint16();

    if (!error_ && count) {
      itemlist.items.resize(count);
      for (int i = 0; i < count && !error_; i++) {
        itemlist.items[i] = read_item();
      }
    }

    return itemlist;
  }

  SimpleMob read_mob() {
    SimpleMob mob;

    mob.type = read_uint8();
    mob.visible = (bool)read_uint8();
    mob.id = read_uint64();

    if (!error_ && mob.visible) {
      mob.pos_x = read_uint16();
      mob.pos_y = read_uint16();
      mob.gfx_id = read_string();
      mob.name = read_string();
    }

    return mob;
  }

  bool error() const {
    return error_;
  }

  bool ok() const {
    return !error_;
  }

 private:
  const bytes_t& data_;
  size_t sz_;
  size_t idx_;
  bool error_;
};

