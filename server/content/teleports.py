import sys
sys.path.append('..')

from constants import *
from item import *

class ItemDoor(Item):
  def __init__(self, teleport_group=None):
    Item.__init__(self)
    self.movable = False
    self.gfx_id = "door"
    self.name = "Door"
    self.type = "door"

    self.teleport_group = teleport_group
    self.teleport_group_items = None

  def find_teleports(self, world):
    self.teleport_group_items = []

    for k, item in world.items.items():
      if item.type != "door":
        continue

      if item.teleport_group == self.teleport_group:
        self.teleport_group_items.append(item)

  def teleport(self, mob, world):
    if self.teleport_group_items is None:
      self.find_teleports(world)

    dst = None
    for item in self.teleport_group_items:
      diff_x = abs(item.pos_x - mob.pos_x)
      diff_y = abs(item.pos_y - mob.pos_y)
      if diff_x + diff_y == 1:
        continue
      dst = item

    if dst is None:
      return

    for d, x, y in [
        ( DIR_NORTH, 0, -1 ),
        ( DIR_SOUTH, 0,  1 ),
        ( DIR_WEST, -1, 0 ),
        ( DIR_EAST, 1, 0 ),
    ]:
      cx = dst.pos_x + x
      cy = dst.pos_y + y
      idx = cx + cy * 512
      candidate_tile = world.map.terrain[idx]
      if candidate_tile in BLOCKING_TILES:
        continue

      old_x, old_y = mob.pos_x, mob.pos_y
      world.map.move_mob(mob, cx, cy)
      if mob.type == "player":
        mob.direction = d
        mob.send_position()
        world.broadcast_mobs(cx, cy, ignore=mob)
      else:
        world.broadcast_mobs(cx, cy)
      world.broadcast_mobs(old_x, old_y)


register_item_class("door", ItemDoor)

