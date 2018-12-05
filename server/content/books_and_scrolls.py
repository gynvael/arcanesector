import sys
sys.path.append('..')

from constants import *
from item import *

class ItemScroll(Item):
  def __init__(self, name="Scroll of Unnamed", text="???"):
    Item.__init__(self)
    self.movable = True
    self.gfx_id = "scroll"

    self.name = name
    self.text = text

  def use(self, player, world):
    # TODO: make scroll addable to book
    player.show_text("The scroll says: %s" % self.text)

register_item_class("scroll", ItemScroll)

