import sys
sys.path.append('..')

from constants import *
from item import *

class ItemSign(Item):
  def __init__(self, text=""):
    Item.__init__(self)
    self.movable = False
    self.gfx_id = "sign"
    self.name = "Sign"

    self.text = str(text)

  def use(self, player, world):
    player.show_text("The sign reads: %s" % str(self.text))

class ItemFlagSign(Item):
  def __init__(self, text=""):
    Item.__init__(self)
    self.movable = False
    self.gfx_id = "sign"
    self.name = "Flag Sign"

    self.flag_file = None

  def use(self, player, world):
    if self.flag_file == None:
      player.show_text("The sign is unreadable")
      return

    try:
      with open("flags/%s" % self.flag_file) as f:
        text = f.read()
      player.show_text("The sign reads: %s" % text)
    except IOError:
      player.show_text("ERROR: Contact CTF admin, this should not happen.")

register_item_class("sign", ItemSign)
register_item_class("flag_sign", ItemFlagSign)

