import sys
import random
sys.path.append('..')

from constants import *
from item import *

class ItemHerb(Item):
  def __init__(self, index=0):
    Item.__init__(self)

    names = [
      "Rele",
      "Drond",
      "Bell Rose",
      "Lady's Oaplow",
      "Magic Mushroom",
      "Skull Vine",
      "Fria",
      "Blue Frog Fruit",
      "Spoglow Mint",
      "Treaffond",
      "Blue Weasel Button",
      "Black Skull",
      "Moon Ephess",
      "Crown Berry"
    ]

    # Dirty hack.
    if index % 2 == 0:
      index = 6
    else:
      index = 8

    self.movable = True
    self.gfx_id = ("herb_%d" % index)
    self.name = names[index]
    self.type = "herb"
    self.index = index

register_item_class("herb", ItemHerb)
