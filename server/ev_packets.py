import os
import Queue
import sys
import time
import types
import random

from packets import *
from ev_common import *
from content import spells

# Setting for hearing distance.
MAX_DIST_CLOSE = 7
MAX_DIST_NEARBY = 12
MAX_DIST_AWAY = 18

MAX_DIST_CLOSE_SQ = MAX_DIST_CLOSE * MAX_DIST_CLOSE
MAX_DIST_NEARBY_SQ = MAX_DIST_NEARBY * MAX_DIST_NEARBY
MAX_DIST_AWAY_SQ = MAX_DIST_AWAY * MAX_DIST_AWAY

g_debug = sys.modules["__main__"].g_debug

class PacketEvents(GetHandlersMixin):
  def __init__(self, world):
    self.world = world

  def handle_SAYS(self, p, player):
    text = escape_text(p.text)
    if len(text) > 256:
      text = text[:256]

    # Depending on the distance there will be 3 different variants of text.
    # Close: Text as is.
    # Nearby: Lowercased.
    # A little further away: Random letters will be replaced by dots.
    # Players even further than that will not be able to hear the message at
    # all.
    text_close = text
    text_nearby = text.lower()
    text_away = ''.join([ch if random.randint(0, 1) == 0 else '.' for ch in text])

    msg_close = "\x13%s says: \x17%s" % (player.name, text_close)
    msg_nearby = "\x13%s says: \x17%s" % (player.name, text_nearby)
    msg_away = "\x13%s says: \x17%s" % (player.name, text_away)

    x = player.pos_x
    y = player.pos_y
    players = self.world.get_active_players_near(x, y, MAX_DIST_AWAY)

    for p in players:
      if p is player:
        continue  # Player's message got displayed by the client.

      dx = x - p.pos_x
      dy = y - p.pos_y
      dist_sq = dx * dx + dy * dy

      if dist_sq <= MAX_DIST_CLOSE_SQ:
        p.show_text(msg_close)
      elif dist_sq <= MAX_DIST_NEARBY_SQ:
        p.show_text(msg_nearby)
      else:
        p.show_text(msg_away)

    print "%s says> %s" % (player.name, text)

  def handle_MOVE(self, p, player):
    if p.direction > 3:
      return False

    actual_direction = MOVE_TO_DIR_TRANSLATION[player.direction][p.direction]
    move_vector = DIR_TO_VECTOR[actual_direction]

    pos_candidate = (
        player.pos_x + move_vector[0],
        player.pos_y + move_vector[1]
    )

    if (pos_candidate[0] < 0 or pos_candidate[0] >= 512 or
        pos_candidate[1] < 0 or pos_candidate[1] >= 768):
      player.show_text("Can't go there.")
      return False

    items = self.world.map.get_items(pos_candidate[0], pos_candidate[1])
    if items:
      for item in items:
        if item.blocking:
          player.show_text("Way is blocked.")
          return False
        if item.type == "door":
          return item.teleport(player, self.world)

    idx = pos_candidate[0] + pos_candidate[1] * 512

    candidate_tile = self.world.map.terrain[idx]
    if candidate_tile in BLOCKING_TILES:
      #if candidate_tile == TILE_WATER:
      #  player.show_text("Looks deep, cold and otherwise unpleasant.")
      #else:
      #  player.show_text("Looks pretty solid.")
      return False

    old_x, old_y = player.pos_x, player.pos_y
    self.world.map.move_mob(player, pos_candidate[0], pos_candidate[1])
    player.send_position()
    self.world.broadcast_mobs(old_x, old_y, ignore=player)
    self.world.broadcast_mobs(player.pos_x, player.pos_y, ignore=player)

  def handle_DIRE(self, p, player):
    if p.direction > 3:
      return False

    player.direction = p.direction
    player.send_position()

  def handle_CAST(self, p, player):
    return spells.spellcast(p.spell, player, self.world)

  def handle_HOLD(self, p, player):
    item_id = p.item_id
    if item_id == ITEM_NON_EXISTING_ID:  # What.
      if g_debug:
        print "Player tried to hold non-existing item"
      return False

    item_place = player.can_reach_item(item_id)
    if item_place is False:
      player.show_text("Out of reach.")
      return

    item = item_place[0]

    if not item.movable:
      player.show_text("Unable to take this item.")
      return

    if item_place[1] == "ground":
      self.world.map.remove_item(item)
      player.send_ground()
      self.world.broadcast_ground(player.pos_x, player.pos_y, ignore=player)
    elif item_place[1] == "inventory":
      player.inventory[item_place[2]] = self.world.NON_EXISTING_ITEM
      player.send_inventory()
    else:
      player.equipment[item_place[2]] = self.world.NON_EXISTING_ITEM
      player.send_inventory()

    player.hold_item(item)

  def handle_DROP(self, p, player):
    if player.holding.id == ITEM_NON_EXISTING_ID:
      return

    new_holding = self.world.NON_EXISTING_ITEM

    if 0 <= p.dst < 8:  # Inventory.
      idx = p.dst
      new_holding = player.inventory[idx]  # Might be empty.
      player.inventory[idx] = player.holding
      player.send_inventory()
    elif 8 <= p.dst < 10:  # Equipment.
      idx = p.dst - 8
      new_holding = player.equipment[idx]  # Might be empty.
      player.equipment[idx] = player.holding
      player.send_inventory()
    else:
      # Ground.
      player.holding.pos_x = player.pos_x
      player.holding.pos_y = player.pos_y
      self.world.map.add_item(player.holding)
      player.send_ground()
      self.world.broadcast_ground(player.pos_x, player.pos_y, ignore=player)

    player.hold_item(new_holding)

  def handle_USEI(self, p, player):
    item_id = p.item_id
    if item_id == ITEM_NON_EXISTING_ID:  # What.
      if g_debug:
        print "Player tried to use non-existing item"
      return False

    item_place = player.can_reach_item(item_id)
    if item_place is False:
      player.show_text("Out of reach.")
      return

    item = item_place[0]
    return item.use(player, self.world)

  def handle_THIS(self, p, player):
    # Ignore.
    return


