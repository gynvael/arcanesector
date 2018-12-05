import os
import Queue
import sys
import time
import types
import json

from threading import Thread, Event, Lock

from packets import *
from uniqueid import get_unique_id
from ev_packets import *
from ev_specials import *
from constants import *
from item import *
from mob import *
from content import *

HERE = os.path.dirname(os.path.abspath(__file__))

g_debug = sys.modules["__main__"].g_debug

# TODO: If an item is held, auto-release it after 5 seconds (or ideally, after
# disconnect).

NON_EXISTING_ITEM = Item()  # Special item used in free slots.

# TODO: to and from json serialization (see json.JSONEncoder and
# https://realpython.com/python-json/#decoding-custom-types)
class Player(Mob):
  def __init__(self, world):
    Mob.__init__(self)

    # Bind player to this world.
    self.world = world

    # Initial values when the player is newly created.
    self.name = None
    self.type = MOB_TYPE_PLAYER
    self.hp = 40
    self.hp_max = 40
    self.mana = 10
    self.mana_max = 60
    self.inventory = [NON_EXISTING_ITEM] * 8
    self.equipment = [NON_EXISTING_ITEM] * 2
    self.holding = NON_EXISTING_ITEM
    self.pos_x = PLAYER_STARTING_POSITION[0]
    self.pos_y = PLAYER_STARTING_POSITION[1]
    self.direction = PLAYER_STARTING_POSITION[2]
    self.selecting = False
    self.gfx_id = "3d_mob_drow_f"

    # This MUST be set on player creation (it's player_id).
    self.id = MOB_NON_EXISTING_ID

    # TEST! TODO: set some initial inventory
    #self.inventory[0] = self.world.register_item(
    #    books_and_scrolls.ItemScroll("Scroll of Flag"))
    #self.inventory[1] = self.world.register_item(
    #    spells.ItemTeleportRing())
    #self.inventory[2] = self.world.register_item(
    #    flasks.ItemFlask(0))
    #self.inventory[0] = self.world.register_item(
    #    flasks.ItemFlask(FLASK_MANA_POTION))
    #self.inventory[4] = self.world.register_item(
    #    herbs.ItemHerb(3))
    #self.inventory[5] = self.world.register_item(
    #    herbs.ItemHerb(10))

  def update(self, player, world):
    # Do nothing.
    pass

  def can_reach_item(self, item_id):
    try:
      item = self.world.items[item_id]
    except KeyError:  # No item in the world.
      return False

    if self.pos_x == item.pos_x and self.pos_y == item.pos_y:
      return (item, "ground", (item.pos_x, item.pos_y))

    for i, candidate_item in enumerate(self.inventory):
      if candidate_item is item:
        return (item, "inventory", i)

    for i, candidate_item in enumerate(self.equipment):
      if candidate_item is item:
        return (item, "equipment", i)

    return False

  def send_stats(self, full=False):
    self.world.post_packet(
        self.id,
        PacketSC_INFO(
            self.hp, self.hp_max,
            self.mana, self.mana_max,
            self.name if full else "")
    )

  def send_inventory(self):  # Inventory and Equipment.
    self.world.post_packet(
        self.id,
        PacketSC_INVT(self.inventory, self.equipment)
    )

  def send_position(self):
    self.world.post_packet(
        self.id,
        PacketSC_POSI(self.pos_x, self.pos_y, self.direction)
    )
    self.send_world_update()


  def send_world_update(self):
    self.send_ground()
    self.send_mobs()
    pass

  def send_ground(self):
    lists = []
    m = self.world.map
    for j in xrange(self.pos_y - 5, self.pos_y + 5 + 1):
      for i in xrange(self.pos_x - 5, self.pos_x + 5 + 1):
        items = m.get_items(i, j)
        if not items:
          continue

        lists.append(Subpacket_ItemList(
            len(items), i, j, items))

    self.world.post_packet(
        self.id,
        PacketSC_GRND(lists)
    )

  def send_mobs(self):
    moblist = self.world.map.get_mobs_near(self.pos_x, self.pos_y, 5, ignore=self)
    self.world.post_packet(
        self.id,
        PacketSC_MOBS(moblist)
    )

  def hold_item(self, item):
    self.holding = item
    if item.id != ITEM_NON_EXISTING_ID:
      item.pos_x = None
      item.pos_y = None

    self.world.post_packet(
        self.id,
        PacketSC_HLDI(item)
    )

  def send_text(self, text):
    # Just an alias.
    self.show_text(text)

  def show_text(self, text):
    self.world.post_packet(
        self.id,
        PacketSC_TEXT(str(text))
    )

  def YIELDING_select(self, timeout=60.0):
    """
    How to use:
    for _ in player.YIELDING_select():
      if _ is False:
        return
      yield _
    """

    # If player is already selecting, bail out.
    if self.selecting:
      self.show_text("Too busy to select another thing.")
      yield False
      return

    # Also bail out if the player is holding something.
    # Note: Alternatively we could just select the held item I guess.
    if self.holding.id != ITEM_NON_EXISTING_ID:
      self.show_text(
          "Can't select when holding an item. Let it go! Let it go!")
      yield False
      return

    self.selecting = True
    self.select_result = None

    select_start = time.time()
    timeout_time = select_start + timeout

    # Tell client to send us something selected.
    slct = PacketSC_SLCT()
    slct.packet_id = get_unique_id()
    self.world.post_packet(self.id, slct)
    net = self.world.register_catch_packet_request(
        self.id, slct.packet_id, "THIS", timeout_time)

    # Await the packet in the net.
    while True:
      # Accept the caught packet even if it's past timeout.
      if net:
        break

      # Check timeout.
      now = time.time()
      if now > timeout_time:
        self.show_text("Not interested in selecting? Got it! Aborting select.")
        self.selecting = False
        yield False
        return

      # Good night.
      yield {
          "event": EV_ASYNC,
          "wake_time": now + 0.5
      }

    # We caught something in the net!
    p = net[0]["p"]
    TARGET_TYPES = [ "item", "mob", "ground", "empty_slot" ]
    if p.type >= len(TARGET_TYPES):  # Uhm, weird.
      self.selecting = False
      yield False
      return

    target_type = TARGET_TYPES[p.type]

    target = None

    if target_type == "item":
      target = self.can_reach_item(p.id)
      if target is False:
        self.show_text("Can't reach this item.")
        self.selecting = False
        yield False
        return
    elif target_type == "mob":
      target = self.world.mobs.get(p.id)
      if target is None:
        self.show_text("Missed the mob.")
        self.selecting = False
        yield False
        return
    elif target_type == "ground":
      target = (self.pos_x, self.pos_y)
    elif target_type == "empty_slot":
      # Is the slot really empty, or is some funny business going on here?
      if 0 <= p.id < 8:
        if self.inventory[p.id].id != ITEM_NON_EXISTING_ID:
          self.show_text("Slot is not empty.")
          self.selecting = False
          yield False
          return
        target = ("inventory", p.id)
      elif 8 <= p.id < 10:
        if self.equipment[p.id - 8] != ITEM_NON_EXISTING_ID:
          self.show_text("Slot is not empty.")
          self.selecting = False
          yield False
          return
        target = ("equipment", p.id - 8)
      else: # Wat.
        self.show_text("What did you select???")
        self.selecting = False
        yield False
        return

    # Done!
    self.select_result = target_type, target
    self.selecting = False


class Map(object):
  def __init__(self, world):
    self.world = world

    self.w = 512
    self.h = 768
    self.terrain = bytearray(self.w * self.h)

    # Each of these uses (x, y) as a key and a list of entities as values.
    self.item_map = {}
    self.mob_map = {}

    # TODO: add a thread that posts item removal events

  @staticmethod
  def decode_map_data(data):
    data = data.decode("base64").decode("zlib")
    return bytearray(data)

  def load(self, fname):
    with open(fname, "rb") as f:
      d = json.load(f)

    self.w = d["w"]
    self.h = d["h"]
    self.terrain = self.decode_map_data(d["terrain"])
    assert self.w * self.h == len(self.terrain)

    for item_data in d["items"]:
      item_class = g_item_classes.get(item_data["type"])
      if item_class is None:
        item_class = Item

      item = item_class()
      item.wakeup(item_data)

      self.world.register_item(item)
      self.add_item(item)

    self.spawners = []

    for sys_obj in d["system_objects"]:
      if sys_obj["type"] == "spawn_item":
        self.spawners.append(sys_obj)

  def add_item(self, item):
    if item.pos_x is None or item.pos_y is None:
      return item

    pos = (item.pos_x, item.pos_y)
    if pos in self.item_map:
      try:  # Is it already there? (desync?)
        self.item_map[pos].index(item)
      except ValueError:
        self.item_map[pos].append(item)
    else:
      self.item_map[pos] = [item]
    return item

  def get_items(self, pos_x, pos_y):
    items = self.item_map.get((pos_x, pos_y))
    if items is None:
      return items

    if not items:
      return items

    # Use this opportunity to remove any non-existing item.
    good_items = []
    for item in items:
      if item.id != ITEM_NON_EXISTING_ID:
        good_items.append(item)

    if len(good_items) == len(items):
      return items

    self.item_map[(pos_x, pos_y)] = good_items

    return good_items

  def remove_item(self, item):
    # Remove item from map.
    if item.pos_x is None or item.pos_y is None:
      return item

    pos = (item.pos_x, item.pos_y)
    items = self.item_map.get(pos)
    if items is None:
      return

    try:
      while True:  # Remove any dups too.
        idx = items.index(item)
        items.pop(idx)

        if not items:
          del self.item_map[pos]
          break
    except ValueError:
      return

  def get_mobs(self, pos_x, pos_y):
    return self.mob_map.get((pos_x, pos_y))

  def get_mobs_near(self, pos_x, pos_y, distance, ignore=None):
    mobs = []
    for j in xrange(pos_y - distance, pos_y + distance + 1):
      for i in xrange(pos_x - distance, pos_x + distance + 1):
        ret = self.mob_map.get((i, j))
        if not ret:
          continue
        if ignore is None:
          mobs.extend(ret)
        else:
          for mob in ret:
            if mob is not ignore:
              mobs.append(mob)

    return mobs

  def add_mob(self, mob):
    if mob.pos_x is None or mob.pos_y is None:
      return mob

    pos = (mob.pos_x, mob.pos_y)
    if pos in self.mob_map:
      # Check if the mob isn't there by any chance.
      try:
        self.mob_map[pos].index(mob)
        # Nothing to do. Weird though.
      except ValueError:
        self.mob_map[pos].append(mob)
    else:
      self.mob_map[pos] = [mob]
    return mob

  def remove_mob(self, mob):
    # Remove item from map.
    if mob.pos_x is None or mob.pos_y is None:
      return mob

    pos = (mob.pos_x, mob.pos_y)
    mobs = self.mob_map.get(pos)
    if mobs is None:
      return

    try:
      while True:  # Fix desync.
        idx = mobs.index(mob)
        mobs.pop(idx)

        if not mobs:
          del self.mob_map[pos]
          break
    except ValueError:
      return

  def move_mob(self, mob, new_pos_x, new_pos_y):
    self.remove_mob(mob)
    mob.pos_x = new_pos_x
    mob.pos_y = new_pos_y
    self.add_mob(mob)
    return mob


class PacketCatchKey(object):
  # Note: Never set deadline to None when adding to a catch-packet dict.
  def __init__(self, player_id, packet_id, chunk_id, deadline):
    self.player_id = player_id
    self.packet_id = packet_id
    self.chunk_id = chunk_id
    self.deadline = deadline

  def __hash__(self):
    return self.player_id ^ self.packet_id ^ hash(self.chunk_id)

  def __eq__(self, obj):
    return (self.player_id == obj.player_id and
            self.packet_id == obj.packet_id and
            self.chunk_id == obj.chunk_id)


class World(object):
  def __init__(self):
    self.the_end = Event()

    self.NON_EXISTING_ITEM = NON_EXISTING_ITEM

    # Initialize world to defaults.
    self.players_mutex = Lock()  # Used only while creating a player.
    self.players = [None] * 256

    self.player_sessions_mutex = Lock()
    self.player_sessions = [None] * 256

    # Players with established session (use player_sessions_mutex).
    # Note: The sole fact that a player is logged in doesn't mean it's in game.
    # The player might still be e.g. creating a character. Cross-check with the
    # players list to verify if the player actually is playing.
    self.players_logged_in = set()

    # Initialize handlers.
    self.special_handlers_obj = SpecialEvents(self)
    self.special_handlers = self.special_handlers_obj.get_handlers()

    self.packet_handlers_obj = PacketEvents(self)
    self.packet_handlers = self.packet_handlers_obj.get_handlers()

    if g_debug:
      print "World handles the following packet events:"
      for k, v in self.packet_handlers.items():
        print "[EV_PACKET] %s" % (k, )

      print "World handles the following special events:"
      for k, v in self.special_handlers.items():
        print "[EV_SPECIAL] %s" % (k, )

    # Ready storage for items and mobs. These are indexed by 64-bit IDs.
    self.items = {}
    self.mobs = {}

    # Load the pre-defined part of the world.
    self.map = Map(self)
    self.map.load(os.path.join(HERE, "data", "map.json"))

    # Load the items/players/mobs from the backup.
    # TODO: this

    # Bind player to this world.
    for player in self.players:
      if player is None:
        continue
      player.world = self

    # Spawn main thread.
    self.event_queue = Queue.Queue()

    main_thread_ready = Event()
    th = Thread(
        target=self.main_thread,
        args=(main_thread_ready,)
    )
    th.daemon = True  # TODO actually make it False
    th.start()
    self.main_thread_handle = th
    main_thread_ready.wait()

  def register_item(self, item):
    item.id = get_unique_id()
    self.items[item.id] = item
    return item

  def register_mob(self, mob):
    mob.id = get_unique_id()
    self.mobs[mob.id] = mob
    return mob

  def drop_item(self, item, pos_x, pos_y):
    item.pos_x = pos_x
    item.pos_y = pos_y
    self.map.add_item(item)
    return item

  def pickup_item(self, item):
    self.map.remove_item(item)
    return item

  def create_player(self, player_id, pc_name, portrait):
    with self.players_mutex:
      if self.players[player_id] is not None:
        return False  # Player already created.

      p = Player(self)
      p.name = pc_name
      p.portrait = portrait
      p.id = player_id
      p.gfx_id = ["3d_mob_drow_f", "3d_mob_drow_m"][p.id % 2]
      self.players[player_id] = p
      self.mobs[player_id] = p
      self.map.add_mob(p)

    self.broadcast_mobs(p.pos_x, p.pos_y, ignore=p)

    return True

  def acquire_player_session(self, player_id, handler_thread):
    assert 0 <= player_id < 256

    with self.player_sessions_mutex:
      # New session always overrides old session.
      old_session = self.player_sessions[player_id]
      if old_session is not None:
        old_session.force_disconnect()

      self.player_sessions[player_id] = handler_thread
      self.players_logged_in.add(player_id)

      player = self.players[player_id]
      broadcast = False
      if (player is not None and
          player.id != MOB_NON_EXISTING_ID and
          player.pos_x is not None and
          player.pos_y is not None):
        broadcast = True
        self.map.add_mob(player)

    if broadcast:
      self.broadcast_mobs(player.pos_x, player.pos_y, ignore=player)


  def release_player_session(self, player_id, handler_thread):
    if player_id is None:
      return

    with self.player_sessions_mutex:
      old_session = self.player_sessions[player_id]
      if handler_thread is not old_session:
        return  # Already another thread claimed the sessions.

      self.player_sessions[player_id] = None
      self.players_logged_in.discard(player_id)

      player = self.players[player_id]
      broadcast = False
      if (player is not None and
          player.id != MOB_NON_EXISTING_ID and
          player.pos_x is not None and
          player.pos_y is not None):
        self.map.remove_mob(player)
        broadcast = True

    if broadcast:
      self.broadcast_mobs(player.pos_x, player.pos_y, ignore=player)

  def broadcast_mobs(self, pos_x, pos_y, ignore=None):
    for player in self.get_active_players_near(pos_x, pos_y, 8):
      if player is not ignore:
        player.send_mobs()

  def broadcast_ground(self, pos_x, pos_y, ignore=None):
    for player in self.get_active_players_near(pos_x, pos_y, 8):
      if player is not ignore:
        player.send_ground()

  def broadcast_all(self, pos_x, pos_y, ignore=None):
    for player in self.get_active_players_near(pos_x, pos_y, 8):
      if player is not ignore:
        player.send_mobs()
        player.send_ground()

  def get_active_players(self):
    with self.player_sessions_mutex:
      active = set(self.players_logged_in)  # Copy.

    return [self.players[player_id] for player_id in active
            if self.players[player_id] is not None]

  def get_active_players_near(self, x, y, max_distance):
    # TODO: In a perfect world there should be some quad tree updated when
    # players move, so that getting the nearby players is actually fast
    # algorithmically. But for now it's and O(n) process.
    active = self.get_active_players()
    near = []

    if max_distance == 0:  # Players need to be exactly at the spot.
      for p in active:
        if p.pos_x == x and p.pos_y == y:
          near.append(p)
    else:
      max_distance_sq = max_distance * max_distance
      for p in active:
        dx = x - p.pos_x
        dy = y - p.pos_y
        if dx * dx + dy * dy <= max_distance_sq:
          near.append(p)

    return near

  def get_active_players_at(self, x, y):
    return self.get_active_players_near(x, y, 0)

  def is_player_active(self, player_id):
    with self.player_sessions_mutex:
      return (player_id in self.players_logged_in and
              self.players[player_id] is not None)

  def YIELDING_sleep(self, delay):
    yield {
        "event": EV_ASYNC,
        "wake_time": time.time() + delay
    }

  def register_catch_packet_request(
      self, player_id, packet_id, chunk_id, deadline):
    net = []  # A butterfly net to catch the packet in.
    key = PacketCatchKey(player_id, packet_id, chunk_id, deadline)
    self.catch_packet_requests[key] = net
    return net

  def post_sleeping_event(self, ev):
    # OK, this isn't fast and the O() is terrible. But unless this is actually
    # a bottleneck I don't really care. Also, bisect module cannot into custom
    # structures.
    self.event_queue_sleeping.append(ev)
    self.event_queue_sleeping.sort(key=lambda el: el["wake_time"])

  def post_event(self, ev):
    if not self.the_end.is_set():
      if "wake_time" in ev:
        self.post_sleeping_event(ev)
      else:
        self.event_queue.put(ev)

  def post_packet(self, player_id, p):
    # This is a best effort function. We don't really care if the client doesn't
    # receive a packet.
    with self.player_sessions_mutex:
      session = self.player_sessions[player_id]
      if session is None:
        return

    # Note: There is a race-condition here - the session might be overridden
    # by a new one, and the old one might get deactivated in the meantime. But
    # there is little that can be done about it anyway, so it's better to
    # release the mutex early and just rely on the mechanisms in the deactivated
    # session for ignoring the packet.
    session.post_packet(p)

  def handle_special(self, ev):
    # Not all special events need this field set.
    player = None
    if "player_id" in ev:
      player = self.players[ev["player_id"]]

    return self.special_handlers[ev["type"]](ev, player)

  def handle_packet(self, ev):
    player = self.players[ev["player_id"]]
    return self.packet_handlers[ev["p"].chunk_id](ev["p"], player)

  def handle_async(self, ev):
    # Just return the generator, it will be handled in the main loop.
    return ev["generator"]

  def main_thread(self, ready):
    print "Main World Thread is now active."
    ready.set()

    # Top-level handlers.
    handlers = {
        EV_PACKET: self.handle_packet,
        EV_ASYNC: self.handle_async,
        EV_SPECIAL: self.handle_special
    }

    # Sleeping events that are yet to be run are put here. Only this thread has
    # access to this list, and the list is always kept sorted (ascending).
    self.event_queue_sleeping = []

    # In some cases a packet doesn't have to be processed, because another
    # handler is already waiting for it (e.g. if the handler requested some
    # interactive input and is waiting for the answer). These requests are
    # put in the dict below with PacketCatchKey as the key and empty list as
    # the value, and are handled by events from the event_queue_sleeping.
    self.catch_packet_requests = {}

    self.post_event({
        "event": EV_SPECIAL,
        "type": "spawners"
    })

    while (not self.the_end.is_set() or
           not self.event_queue.empty() or
           self.event_queue_sleeping):  # Exit when everything is processed.
      now = time.time()

      ev = None
      try:
        # Check main queue.
        ev = self.event_queue.get_nowait()
      except Queue.Empty:
        # Check the sleeper queue.
        if self.event_queue_sleeping:
          candidate_ev = self.event_queue_sleeping[0]
          if now >= candidate_ev["wake_time"]:
            ev = candidate_ev
            self.event_queue_sleeping.pop(0)

      if ev is None:
        time.sleep(0.01)  # Sleep 10ms.
        continue  # Check if the_end wasn't set.

      #if g_debug:
      #  print "Processing ev:", repr(ev)

      # Is this a packet that is waited for?
      if ev["event"] == EV_PACKET:
        p = ev["p"]
        k = PacketCatchKey(ev["player_id"], p.packet_id, p.chunk_id, None)
        # In an ideal world we could just .send() the packet to the waiting
        # fiber, but it turns out it's pretty tricky to get this right in Python
        # 2.7, so, in hopes of avoiding some bugs, I'll settle for another
        # method which has a small lag, but is safer.
        v = self.catch_packet_requests.get(k)
        if v is not None:
          v.append(ev)
          del self.catch_packet_requests[k]
          continue

      # A non-waited-for event.
      potential_generator = handlers[ev["event"]](ev)

      # If the handler returned a generator we need to iterate on it to actually
      # run the code.
      if type(potential_generator) is types.GeneratorType:
        generator = potential_generator
        try:
          new_ev = generator.next()

          # The only reason to yield is returning a new event that would block.
          # In such case we need to add the generator to the event and post it.
          new_ev["generator"] = generator
          assert(new_ev["event"] == EV_ASYNC)

          self.post_event(new_ev)

        except StopIteration:
          pass  # Code finished.

      continue

    print "Main World Thread is exiting."
    # TODO: Save world.


