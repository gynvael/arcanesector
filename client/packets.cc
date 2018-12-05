#include "packets.h"

// ------------------------------------------------------------------
// Packets received by the client.
// ------------------------------------------------------------------

bool PacketsSC::recv_header(NetSock *s) {
  int ret = s->ReadAll(&this->h, sizeof(this->h));
  if (ret != sizeof(this->h)) {
    return false;
  }

  if (this->h.sz > 1024 * 1024) {
    return false;
  }

  return true;
}

bool PacketsSC::recv_packet(NetSock *s) {
  int payload_sz = (int)this->h.sz;

  this->payload.resize(payload_sz);
  int ret = s->ReadAll(this->payload.data(), payload_sz);
  if (ret != payload_sz) {
    return false;
  }

  Parser p(payload);
  return this->parse(&p);
}

std::unique_ptr<PacketsSC> PacketsSC::recv(NetSock *s) {
  auto tmp_packet = std::make_unique<PacketsSC>();
  if (!tmp_packet->recv_header(s)) {
    return nullptr;
  }

  // There are some methods to automatize this, but whatever.
  // TODO: How about we do an unordered_map of possible packets with a set of
  // lambda functions that would create a class of given type?
  std::unique_ptr<PacketsSC> p;
  #define IF_CHUNK(name) \
    if (memcmp(tmp_packet->h.chunk_id, #name, 4) == 0) { \
      p.reset(new PacketsSC_ ## name{}); \
    }

  IF_CHUNK(POSI)
  else IF_CHUNK(GRND)
  else IF_CHUNK(MOBS)
  else IF_CHUNK(INFO)
  else IF_CHUNK(INVT)
  else IF_CHUNK(GAME)
  else IF_CHUNK(TEXT)
  else IF_CHUNK(PONG)
  else IF_CHUNK(SLCT)
  else IF_CHUNK(HLDI)
  else IF_CHUNK(NOPC)


  #undef IF_CHUNK

  if (p == nullptr) {
    fprintf(stderr, "error: unknown chunk %c%c%c%c\n",
      tmp_packet->h.chunk_id[0],
      tmp_packet->h.chunk_id[1],
      tmp_packet->h.chunk_id[2],
      tmp_packet->h.chunk_id[3]);
    return nullptr;
  }

  p->h = tmp_packet->h;
  if (!p->recv_packet(s)) {
    return nullptr;
  }

  return p;
}

std::string PacketsSC::get_chunk_id() const {
  abort();
}

bool PacketsSC::parse(Parser*) {
  abort();
}

std::string PacketsSC_NOPC::get_chunk_id() const {
  return "NOPC";
}

bool PacketsSC_NOPC::parse(Parser*) {
  return true;
}

std::string PacketsSC_GAME::get_chunk_id() const {
  return "GAME";
}

bool PacketsSC_GAME::parse(Parser*) {
  return true;
}

std::string PacketsSC_INFO::get_chunk_id() const {
  return "INFO";
}

bool PacketsSC_INFO::parse(Parser *p) {
  hp       = p->read_uint16();
  hp_max   = p->read_uint16();
  mana     = p->read_uint16();
  mana_max = p->read_uint16();
  name = p->read_string();  // Can be an empty string.
  return p->ok();
}

std::string PacketsSC_INVT::get_chunk_id() const {
  return "INVT";
}

bool PacketsSC_INVT::parse(Parser *p) {
  for (int i = 0; i < 8; i++) {
    inventory[i] = p->read_item();
  }

  for (int i = 0; i < 2; i++) {
    equipment[i] = p->read_item();
  }

  return p->ok();
}

std::string PacketsSC_POSI::get_chunk_id() const {
  return "POSI";
}

bool PacketsSC_POSI::parse(Parser *p) {
  pos_x = p->read_uint16();
  pos_y = p->read_uint16();
  direction = p->read_uint8();
  return p->ok();
}

std::string PacketsSC_TEXT::get_chunk_id() const {
  return "TEXT";
}

bool PacketsSC_TEXT::parse(Parser*) {
  // This is a weird one.
  text.assign((const char*)&payload[0], payload.size());
  return true;
}

std::string PacketsSC_PONG::get_chunk_id() const {
  return "PONG";
}

bool PacketsSC_PONG::parse(Parser*) {
  return true;
}

std::string PacketsSC_HLDI::get_chunk_id() const {
  return "HLDI";
}

bool PacketsSC_HLDI::parse(Parser *p) {
  item = p->read_item();  // Can be empty item.
  return p->ok();
}

std::string PacketsSC_GRND::get_chunk_id() const {
  return "GRND";
}

bool PacketsSC_GRND::parse(Parser *p) {
  uint8_t count = p->read_uint8();
  if (count != 0) {
    lists.resize(count);
    for (int i = 0; i < count && p->ok(); i++) {
      lists[i] = p->read_itemlist();
    }
  }
  return p->ok();
}

std::string PacketsSC_SLCT::get_chunk_id() const {
  return "SLCT";
}

bool PacketsSC_SLCT::parse(Parser*) {
  return true;
}

std::string PacketsSC_MOBS::get_chunk_id() const {
  return "MOBS";
}

bool PacketsSC_MOBS::parse(Parser *p) {
  uint8_t count = p->read_uint16();
  if (count != 0) {
    moblist.resize(count);
    for (int i = 0; i < count && p->ok(); i++) {
      moblist[i] = p->read_mob();
    }
  }
  return p->ok();
}

// ------------------------------------------------------------------
// Packets sent by the client.
// ------------------------------------------------------------------

bool PacketsCS::send(NetSock *s) {
  // NOTE: I'm not checking whether payload is within size limits. That's
  // handled server side.

  // Make sure payload is filled.
  this->build();

  // Init common header.
  packet_header_st h{};
  h.sz = payload.size();
  memcpy(h.chunk_id, this->get_chunk_id().data(), 4);
  h.packet_id = this->packet_id;

  int ret = s->WriteAll(&h, sizeof(h));
  if (ret != sizeof(h)) {
    return false;
  }

  int payload_sz = (int)this->payload.size();
  ret = s->WriteAll(this->payload.data(), payload_sz);
  if (ret != payload_sz) {
    return false;
  }

  return true;
}



std::string PacketsCS_ENTR::get_chunk_id() const {
  return "ENTR";
}

void PacketsCS_ENTR::build() {
  payload.resize(sizeof(data));
  memcpy(payload.data(), &data, sizeof(data));
}

std::unique_ptr<PacketsCS_ENTR>
    PacketsCS_ENTR::make(std::string passwd, uint8_t player_id) {
  auto p = std::make_unique<PacketsCS_ENTR>();

  // Copy at most 32 bytes.
  size_t passwd_sz = std::min<size_t>(32, passwd.size());
  memcpy(p->data.passwd, passwd.data(), passwd_sz);

  p->data.player_id = player_id;

  return p;
}


std::string PacketsCS_MYPC::get_chunk_id() const {
  return "MYPC";
}

void PacketsCS_MYPC::build() {
  payload.resize(sizeof(data));
  memcpy(payload.data(), &data, sizeof(data));
}

std::unique_ptr<PacketsCS_MYPC>
    PacketsCS_MYPC::make(std::string pc_name, uint8_t portrait) {
  auto p = std::make_unique<PacketsCS_MYPC>();

  // Copy at most 32 bytes.
  size_t pc_name_sz = std::min<size_t>(32, pc_name.size());
  memcpy(p->data.pc_name, pc_name.data(), pc_name_sz);

  p->data.portrait = portrait;

  return p;
}

std::string PacketsCS_MOVE::get_chunk_id() const {
  return "MOVE";
}

void PacketsCS_MOVE::build() {
  payload.resize(sizeof(data));
  memcpy(payload.data(), &data, sizeof(data));
}

std::unique_ptr<PacketsCS_MOVE>
    PacketsCS_MOVE::make(uint8_t direction) {
  auto p = std::make_unique<PacketsCS_MOVE>();

  p->data.direction = direction;

  return p;
}

std::string PacketsCS_DIRE::get_chunk_id() const {
  return "DIRE";
}

void PacketsCS_DIRE::build() {
  payload.resize(sizeof(data));
  memcpy(payload.data(), &data, sizeof(data));
}

std::unique_ptr<PacketsCS_DIRE>
    PacketsCS_DIRE::make(uint8_t direction) {
  auto p = std::make_unique<PacketsCS_DIRE>();

  p->data.direction = direction;

  return p;
}

std::string PacketsCS_GBYE::get_chunk_id() const {
  return "GBYE";
}

std::unique_ptr<PacketsCS_GBYE>
    PacketsCS_GBYE::make() {
  return std::make_unique<PacketsCS_GBYE>();
}

std::string PacketsCS_PING::get_chunk_id() const {
  return "PING";
}

std::unique_ptr<PacketsCS_PING>
    PacketsCS_PING::make() {
  return std::make_unique<PacketsCS_PING>();
}

std::string PacketsCS_SAYS::get_chunk_id() const {
  return "SAYS";
}

void PacketsCS_SAYS::build() {
  payload.resize(text.size());
  memcpy(payload.data(), text.data(), text.size());
}

std::unique_ptr<PacketsCS_SAYS>
    PacketsCS_SAYS::make(const std::string& text) {
  auto p = std::make_unique<PacketsCS_SAYS>();

  p->text = text;

  return p;
}

std::string PacketsCS_USEI::get_chunk_id() const {
  return "USEI";
}

void PacketsCS_USEI::build() {
  payload.resize(sizeof(item));
  memcpy(payload.data(), &item, sizeof(item));
}

std::unique_ptr<PacketsCS_USEI>
    PacketsCS_USEI::make(uint64_t item) {
  auto p = std::make_unique<PacketsCS_USEI>();

  p->item = item;

  return p;
}

std::string PacketsCS_HOLD::get_chunk_id() const {
  return "HOLD";
}

void PacketsCS_HOLD::build() {
  payload.resize(sizeof(item));
  memcpy(payload.data(), &item, sizeof(item));
}

std::unique_ptr<PacketsCS_HOLD>
    PacketsCS_HOLD::make(uint64_t item) {
  auto p = std::make_unique<PacketsCS_HOLD>();

  p->item = item;

  return p;
}

std::string PacketsCS_DROP::get_chunk_id() const {
  return "DROP";
}

void PacketsCS_DROP::build() {
  payload.resize(sizeof(dst));
  memcpy(payload.data(), &dst, sizeof(dst));
}

std::unique_ptr<PacketsCS_DROP>
    PacketsCS_DROP::make(uint8_t dst) {
  auto p = std::make_unique<PacketsCS_DROP>();

  p->dst = dst;

  return p;
}

std::string PacketsCS_CAST::get_chunk_id() const {
  return "CAST";
}

void PacketsCS_CAST::build() {
  payload.resize(sizeof(spell));
  memcpy(payload.data(), spell, sizeof(spell));
}

std::unique_ptr<PacketsCS_CAST>
    PacketsCS_CAST::make(uint8_t spell[8]) {
  auto p = std::make_unique<PacketsCS_CAST>();

  memcpy(p->spell, spell, 8);

  return p;
}

std::string PacketsCS_THIS::get_chunk_id() const {
  return "THIS";
}

void PacketsCS_THIS::build() {
  payload.resize(sizeof(type) + sizeof(id));
  memcpy(payload.data(), &type, sizeof(type));
  memcpy(payload.data() + 1, &id, sizeof(id));
}

std::unique_ptr<PacketsCS_THIS>
    PacketsCS_THIS::make(uint64_t packet_id, uint8_t type, uint64_t id) {
  auto p = std::make_unique<PacketsCS_THIS>();
  p->packet_id = packet_id;
  p->type = type;
  p->id = id;
  return p;
}

