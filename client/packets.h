#pragma once
// This is technically a .cc file since I'm including all the source code here.
// Oh well.

#include <vector>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <algorithm>
#include "NetSock.h"
#include "parser_helper.h"

using namespace std::string_literals;

using bytes_t = std::vector<uint8_t>;

struct packet_header_st {
  uint32_t sz;
  char chunk_id[4];
  uint64_t packet_id;
} __attribute__((packed));

// ------------------------------------------------------------------
// Packets received by the client.
// ------------------------------------------------------------------

class PacketsSC {
 public:
  PacketsSC() {}
  virtual ~PacketsSC() {}

  bool recv_header(NetSock *s);
  bool recv_packet(NetSock *s);

  // Override these methods, seriously.
  virtual std::string get_chunk_id() const;
  virtual bool parse(Parser*);

  static std::unique_ptr<PacketsSC> recv(NetSock *s);

  packet_header_st h{};
  bytes_t payload; // parse should look for data here.
};

class PacketsSC_NOPC : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser*) override;
};

class PacketsSC_GAME : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser*) override;
};

class PacketsSC_INFO : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser *p) override;

  uint16_t hp{};
  uint16_t hp_max{};
  uint16_t mana{};
  uint16_t mana_max{};
  std::string name{};  // Can be empty.
};

class PacketsSC_INVT : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser *p) override;

  SimpleItem inventory[8];
  SimpleItem equipment[2];
};

class PacketsSC_POSI : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser *p) override;

  uint16_t pos_x;
  uint16_t pos_y;
  uint8_t direction;
};

class PacketsSC_TEXT : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser*) override;

  std::string text;
};

class PacketsSC_PONG : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser*) override;
};

class PacketsSC_HLDI : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser*) override;

  SimpleItem item;
};

class PacketsSC_GRND : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser*) override;

  std::vector<SimpleItemList> lists;
};

class PacketsSC_SLCT : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser*) override;
};

class PacketsSC_MOBS : public PacketsSC {
 public:
  std::string get_chunk_id() const override;
  bool parse(Parser*) override;

  std::vector<SimpleMob> moblist;
};


// ------------------------------------------------------------------
// Packets sent by the client.
// ------------------------------------------------------------------

class PacketsCS {
 public:
  PacketsCS() : packet_id{0} {}
  virtual ~PacketsCS() {}

  bool send(NetSock *s);

  virtual std::string get_chunk_id() const = 0;
  virtual void build() {};

  bytes_t payload;  // Child class has to fill this.
  uint64_t packet_id;  // User can optionally fill this.
};

class PacketsCS_ENTR : public PacketsCS {
 public:
  struct ENTR_st {
    char passwd[32];
    uint8_t player_id;
  } __attribute__((packed));

  ENTR_st data{};

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_ENTR>
      make(std::string passwd, uint8_t player_id);
};

class PacketsCS_MYPC : public PacketsCS {
 public:
  struct MYPC_st {
    char pc_name[32];
    uint8_t portrait;
  } __attribute__((packed));

  MYPC_st data{};

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_MYPC>
      make(std::string pc_name, uint8_t portrait);
};

class PacketsCS_MOVE : public PacketsCS {
 public:
 struct MOVE_st {
    // 0-3 mapped as Forward, Backward, Strafe Left, Strafe Right.
    uint8_t direction;
  } __attribute__((packed));

  MOVE_st data{};

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_MOVE> make(uint8_t direction);
};

class PacketsCS_DIRE : public PacketsCS {
 public:
 struct DIRE_st {
    // 0-3 mapped as North, South, West, East.
    uint8_t direction;
  } __attribute__((packed));

  DIRE_st data{};

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_DIRE> make(uint8_t direction);
};

class PacketsCS_GBYE : public PacketsCS {
 public:
  std::string get_chunk_id() const override;

  static std::unique_ptr<PacketsCS_GBYE> make();
};

class PacketsCS_PING : public PacketsCS {
 public:
  std::string get_chunk_id() const override;

  static std::unique_ptr<PacketsCS_PING> make();
};

class PacketsCS_SAYS : public PacketsCS {
 public:
  std::string text;

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_SAYS> make(const std::string& text);
};

class PacketsCS_USEI : public PacketsCS {
 public:
  uint64_t item;

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_USEI> make(uint64_t item_id);
};

class PacketsCS_HOLD : public PacketsCS {
 public:
  uint64_t item;

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_HOLD> make(uint64_t item_id);
};

class PacketsCS_DROP : public PacketsCS {
 public:
  uint8_t dst;

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_DROP> make(uint8_t dst);
};

class PacketsCS_CAST : public PacketsCS {
 public:
  uint8_t spell[8];

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_CAST> make(uint8_t spell[8]);
};

class PacketsCS_THIS : public PacketsCS {
 public:
  uint8_t type;
  uint64_t id;

  std::string get_chunk_id() const override;
  void build() override;

  static std::unique_ptr<PacketsCS_THIS> make(
      uint64_t packet_id, uint8_t type, uint64_t id);
};

