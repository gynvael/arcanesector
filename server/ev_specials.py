import os
import Queue
import sys
import time
import types
from random import randint

from packets import *
from world import *
from ev_common import *
from content import *

class SpecialEvents(GetHandlersMixin):
  def __init__(self, world):
    self.world = world

  def handle_basic_info_request(self, ev, player):
    # Send basic info to the player (usually after the client just connected, or
    # teleported somewhere).
    world = self.world

    player.send_stats(full=True)
    player.send_inventory()
    player.send_position()
    if player.holding.id != ITEM_NON_EXISTING_ID:
      world.post_packet(
          player.id,
          PacketSC_HLDI(player.holding)
      )

    msg = '\n'.join([
        "Welcome %s to \x12" "Arcane Sector" "\x0f!" % player.name,
    ])

    player.show_text(msg)

  def handle_spawners(self, ev, player):
    world = self.world

    while True:
      now = time.time()

      for spawn in world.map.spawners:
        if 'last' in spawn:
          when = spawn["last"] + spawn["cooldown"]
          if now < when:
            continue

        if world.map.get_items(spawn["pos_x"], spawn["pos_y"]):
          # Something already there, ignore.
          continue

        # Spawn.
        item = None
        if spawn["item_class"] == "herb":
          item = herbs.ItemHerb(random.randint(0, 13))
        elif spawn["item_class"] == "flask":
          item = flasks.ItemFlask()
        elif spawn["item_class"] == "teleport_ring":
          item = spells.ItemTeleportRing()

        spawn["last"] = now

        if item:
          item.pos_x = spawn["pos_x"]
          item.pos_y = spawn["pos_y"]
          world.map.add_item(item)
          world.register_item(item)

      for _ in world.YIELDING_sleep(30.0):  # yield from
        yield _


