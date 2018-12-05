import sys
import random
sys.path.append('..')

from constants import *
from item import *

class ItemFlask(Item):
  def __init__(self, bitmask=0):
    Item.__init__(self)
    self.movable = True

    if bitmask == 0:
      self.gfx_id = "empty_flask"
      self.name = "Empty flask"
    elif bitmask == FLASK_HEALTH_POTION:
      self.gfx_id = "health_potion"
      self.name = "Health potion"
    elif bitmask == FLASK_MANA_POTION:
      self.gfx_id = "mana_potion"
      self.name = "Mana potion"
    else:
      self.gfx_id = "unknown_potion"
      self.name = ("Unspecified potion (#%d)" % bitmask)

    self.bitmask = bitmask

  def use(self, player, world):
    if self.bitmask == FLASK_HEALTH_POTION or self.bitmask == FLASK_MANA_POTION:
      if self.bitmask == FLASK_HEALTH_POTION:
        if player.hp == player.hp_max:
          player.show_text("Already at maximum HP")
          return

        player.hp = min(player.hp + random.randint(20, 30), player.hp_max)
        player.show_text("Gulp, gulp, gulp... Aaahh! I feel so fresh now.")
      else:
        if player.mana == player.mana_max:
          player.show_text("Already at maximum mana")
          return

        player.mana = min(player.mana + random.randint(25, 40), player.mana_max)
        player.show_text("Mana successfully replenished.")

      self.gfx_id = "empty_flask"
      self.name = "Empty flask"
      self.bitmask = 0

      player.send_stats()
    else:
      player.show_text("Select the herb to put into the flask.")

      for _ in player.YIELDING_select():  # yield from
        if _ is False:
          return
        yield _

      if player.select_result is None:
        player.show_text("Something's not right...")
        return

      target_type, target = player.select_result
      if target_type != "item":
        player.show_text("Something's not right...")
        return

      item, item_location, item_location_info = target

      if item.type != "herb":
        player.show_text("That's not a herb.")
        return

      # Mark that the herb was put inside the flask.
      self.bitmask |= (1 << item.index)

      # Remove the herb.
      item.id = ITEM_NON_EXISTING_ID

      if self.bitmask == FLASK_HEALTH_POTION:
        self.gfx_id = "health_potion"
        self.name = "Health potion"

        player.show_text("Obtained a health potion!")
      elif self.bitmask == FLASK_MANA_POTION:
        self.gfx_id = "mana_potion"
        self.name = "Mana potion"

        player.show_text("Obtained a mana potion!")
      else:
        self.gfx_id = "unknown_potion"
        self.name = ("Unspecified potion (#%d)" % self.bitmask)

    player.send_inventory()
    player.send_ground()

register_item_class("flask", ItemFlask)
