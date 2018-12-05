import sys
sys.path.append('..')

from constants import *
from item import *

SWITCH_NUMBERS = [
    0xa8c567df,0x57af7154,0xf46c169b,0x403eec2f,0x374fb5c2,0xc61a4238,
    0x5a502b1a,0x590aa048,0x39ad2bc0,0x25ae89c9,0xe1dc2db2,0x9049374f,
    0xbc3344d7,0x89bbf1dc,0x143af3de,0xfcbd8dcb,0x1a8b3bbd,0xfe3d3c98,
    0xa2dd010f,0xcdc803a6,0x204d3099,0x289ad759,0xd26a9ad7,0x6f03feef,
    0xa6a53639,0xe8d669d3,0xbb2eae34,0xa375268e,0xf74e4875,0x417c175d,
]

class ItemBlocker(Item):
  def __init__(self):
    Item.__init__(self)
    self.movable = False
    self.blocking = True
    self.gfx_id = "blocker"
    self.name = "Blocked secret passage"
    self.type = "blocker"

class ItemSwitch(Item):
  def __init__(self, switch_group=""):
    Item.__init__(self)
    self.movable = False
    self.gfx_id = "switch"
    self.name = "Switch (off)"
    self.type = "switch"

    self.switch_group = str(switch_group)

    self.switch_group_items = None
    self.switch_value = None
    self.switch_on = False

  def init_group(self, world):
    self.switch_group_items = []

    i = 0
    for k, item in world.items.items():
      if item.type != "switch":
        continue

      if item.switch_group == self.switch_group:
        self.switch_group_items.append(item)
        item.switch_group_items = self.switch_group_items
        item.switch_value = SWITCH_NUMBERS[i]
        i += 1

  def check_blockers(self, world):
    v = 0
    cnt = 0
    for switch in self.switch_group_items:
      if switch.switch_on:
        v ^= switch.switch_value
        cnt += 1

    op = (cnt > 0 and v == 0)

    for k, item in world.items.items():
      if item.type != "blocker":
        continue

      if op:
        item.blocking = False
        item.gfx_id = "blocker_open"
      else:
        item.blocking = True
        item.gfx_id = "blocker"


  def use(self, player, world):
    if self.switch_group_items is None:
      self.init_group(world)

    if self.switch_on:
      self.switch_on = False
      self.gfx_id = "switch"
      player.show_text("Switch off: %i" % 0)
      self.name = "Switch (off)"
    else:
      self.switch_on = True
      self.gfx_id = "switch_on"
      player.show_text("Switch on: %i" % self.switch_value)
      self.name = "Switch (on)"

    self.check_blockers(world)
    player.send_ground()


register_item_class("switch", ItemSwitch)
register_item_class("blocker", ItemBlocker)
