from struct import pack, unpack
import sys

from data_common import *
from constants import *

PACKET_LENGTH_LIMIT = 1024 ** 2

g_debug = sys.modules["__main__"].g_debug

g_cs_packets = {}  # Receivable packets (auto-initialized later).
g_sc_packets = {}  # Sendable packets (auto-initialized later).


class PacketException(Exception):
  def __init__(self, err):
    self.err = err

# Do you know of RAII (Resource Acquisition Is Initialization)? Well, this here
# is RAIDNS (Resource Acquisition Is Doing Network Stuff), at lease in case of
# receiving packets.

# Note: All packet class names must match the chunk_id fields - it's actually
# checked in the code and used in the code for packet routing.

# -------------------------------------------------------------------
# Packets received by the server.
# -------------------------------------------------------------------
class PacketCS(object):
  def __init__(self, s):
    self.s = s

    # Receive common part.
    self.sz, self.chunk_id, self.packet_id = unpack("<I4sQ", s.recvall(0x10))

    if self.chunk_id not in g_cs_packets:
      raise PacketException(
          "Declared packet chunk ID is not known (%s)" % self.chunk_id)

    if self.__class__.__name__ != "PacketCS":
      # It's a subclass. Verify that the chunk_id matches the subclass' name.
      assert self.__class__.__name__.startswith("PacketCS_")
      class_chunk_id = self.__class__.__name__[len("PacketCS_"):]
      if class_chunk_id != self.chunk_id:
        raise PacketException(
            "Unexpected packet (%s vs %s)" % (class_chunk_id, self.chunk_id))

    if self.sz > PACKET_LENGTH_LIMIT:
      raise PacketException(
          "Declared packet size too large (%u vs limit %u)" % (
              self.sz, PACKET_LENGTH_LIMIT))

    # Receive the packet.
    self.data = s.recvall(self.sz)

    # Call the handler in the underlying class.
    self.parse(self.data)

  def parse(self, d):
    pass  # Do nothing by default.


class PacketCS_ENTR(PacketCS):
  def parse(self, d):
    passwd, self.player_id = unpack("<32sB", d)
    self.passwd = passwd.rstrip('\0')


class PacketCS_MYPC(PacketCS):
  def parse(self, d):
    pc_name, self.portrait = unpack("<32sB", d)
    self.pc_name = pc_name.rstrip('\0')


class PacketCS_DIRE(PacketCS):
  def parse(self, d):
    self.direction, = unpack("<B", d)


class PacketCS_MOVE(PacketCS):
  def parse(self, d):
    self.direction, = unpack("<B", d)


class PacketCS_CAST(PacketCS):
  def parse(self, d):
    self.spell, = unpack("<8s", d)
    self.spell = bytearray(self.spell)


class PacketCS_THIS(PacketCS):
  def parse(self, d):
    self.type, self.id = unpack("<BQ", d)


class PacketCS_USEI(PacketCS):
  def parse(self, d):
    self.item_id, = unpack("<Q", d)


class PacketCS_HOLD(PacketCS):
  def parse(self, d):
    self.item_id, = unpack("<Q", d)


class PacketCS_DROP(PacketCS):
  def parse(self, d):
    self.dst, = unpack("<B", d)


class PacketCS_SAYS(PacketCS):
  def parse(self, d):
    self.text = d


class PacketCS_GBYE(PacketCS):
  pass


class PacketCS_PING(PacketCS):
  pass


# The hackiest of hacks - an object-promoting packet receiver.
def PacketReceiver(s):
  p = PacketCS(s)  # Receive common part and the payload.
  p.__class__ = g_cs_packets[p.chunk_id]
  p.parse(p.data)
  return p


# -------------------------------------------------------------------
# Packets sent by the server.
# -------------------------------------------------------------------
class PacketSC(object):
  # Child classes:
  # 1. Can set self.payload to a string.
  # 2. Can set self.packet_id to an int (unsigned 64-bit).
  # Note: Building of payload must be done in __init__.

  def send(self, s):
    # For empty packets self.payload doesn't have to be set.
    if hasattr(self, 'payload'):
      assert type(self.payload) is str
      sz = len(self.payload)
      payload = self.payload
    else:
      sz = 0

    # A lovely hack to set the chunk_id automatically.
    assert self.__class__.__name__.startswith("PacketSC_")
    chunk_id = self.__class__.__name__[len("PacketSC_"):]

    # Field packet_id is optional.
    packet_id = self.packet_id if hasattr(self, 'packet_id') else 0

    # Send header.
    s.sendall(pack("<I4sQ", sz, chunk_id, packet_id))

    # Send payload (if any).
    if sz != 0:
      s.sendall(payload)


class PacketSC_NOPC(PacketSC):
  pass  # No additional data.


class PacketSC_GAME(PacketSC):
  pass  # No additional data.


class PacketSC_SLCT(PacketSC):
  pass  # No additional data.


class PacketSC_POSI(PacketSC):
  def __init__(self, pos_x, pos_y, direction):
    self.payload = pack("<HHB", pos_x, pos_y, direction)


class PacketSC_TEXT(PacketSC):
  def __init__(self, text):
    self.payload = str(text)


class PacketSC_HLDI(PacketSC):
  def __init__(self, item):
    self.payload = pack_item(item)


class PacketSC_INFO(PacketSC):
  def __init__(self, hp, hp_max, mana, mana_max, name):
    # name can be empty.
    self.payload = ''.join([
        pack("<HHHH", hp, hp_max, mana, mana_max),
        pack_str(name)
    ])


class PacketSC_INVT(PacketSC):
  def __init__(self, inventory, equipment):
    assert len(inventory) == 8
    assert len(equipment) == 2
    items = []

    for item in inventory + equipment:
      items.append(pack_item(item))

    self.payload = ''.join(items)


class PacketSC_ANIM(PacketSC):
  def __init__(self, type, id, anim):  # Shadowing: type, id
    self.payload = pack("<BQB", type, id, anim)


class PacketSC_MOBS(PacketSC):
  def __init__(self, moblist):
    count = min(len(moblist), 0xffff)
    packet = [
        pack("<H", count)
    ]

    for i in xrange(count):
      packet.append(pack_mob(moblist[i]))

    self.payload = ''.join(packet)


class PacketSC_GRND(PacketSC):
  def __init__(self, lists):
    count = min(len(lists), 0xff)
    packet = [
        pack("<B", count)
    ]

    for i in xrange(count):
      packet.append(self.pack_itemlist(lists[i]))

    self.payload = ''.join(packet)

  def pack_itemlist(self, itemlist):
    count = min(len(itemlist.items), 0xff)

    packet = [
        pack("<BHH", count, itemlist.pos_x, itemlist.pos_y)
    ]

    for i in xrange(count):
      packet.append(pack_item(itemlist.items[i]))
    return ''.join(packet)


class PacketSC_PONG(PacketSC):
  pass  # No additional data.


# Subpackets (serve only as containers).

class Subpacket_ItemList(object):
  def __init__(self, count, pos_x, pos_y, items):
    self.count = count
    self.pos_x = pos_x
    self.pos_y = pos_y
    self.items = items


class Subpacket_Item(object):
  def __init__(self, id, movable, gfx_id, name):  # Shadowing: id
    self.id = id
    self.movable = movable
    self.gfx_id = gfx_id
    self.name = name

# There is no Subpacket_String as a normal str works well enough.

class Subpacket_Mob(object):
  def __init__(self, type, visible, id, pos_x, pos_y, gfx_id, name):
    # Shadowing: type, id
    self.type = type
    self.visible = visible
    self.id = id
    self.pos_x = pos_x
    self.pos_y = pos_y
    self.gfx_id = gfx_id
    self.name = name


# Helper functions.
def pack_str(s):
  return ''.join([
      pack("<H", len(s)),
      s
  ])

def pack_item(item):
  if item.id == ITEM_NON_EXISTING_ID:
    return pack("<Q", item.id)

  return ''.join([
      pack("<QB", item.id, item.movable),
      pack_str(item.gfx_id),
      pack_str(item.name)
  ])

def pack_mob(mob):
  packet = [
      pack("<BBQ", 0, mob.visible, mob.id)
  ]

  if mob.visible:
    packet.extend([
        pack("<HH", mob.pos_x, mob.pos_y),
        pack_str(mob.gfx_id),
        pack_str(mob.name)
    ])

  return ''.join(packet)

# Init lists.
def init_packet_dict(d, prefix):
  m = sys.modules[__name__]
  for name in dir(m):
    if not name.startswith(prefix):
      continue

    chunk_id = name[len(prefix):]
    d[chunk_id] = getattr(m, name)

init_packet_dict(g_sc_packets, "PacketSC_")
init_packet_dict(g_cs_packets, "PacketCS_")

if g_debug:
  print "Receivable packets:"
  for k, v in g_cs_packets.items():
    print "<- %s: %s" % (k, repr(v))

  print "Sendable packets:"
  for k, v in g_sc_packets.items():
    print "-> %s: %s" % (k, repr(v))



