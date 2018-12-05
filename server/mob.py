from constants import *

g_mob_classes = {}
def register_mob_class(name, mob_class):
  if name in g_mob_classes:
    raise Exception("register_mob_class duplicate: %s" % name)

  g_mob_classes[name] = mob_class

class Mob(object):
  def __init__(self):
    # Initial values are for a non-existing mob.
    self.id = MOB_NON_EXISTING_ID  # Must be set when adding to the world.
    self.gfx_id = ""
    self.name = ""
    self.type = ""  # Optional type field.
    self.hp = 100
    self.hp_max = 100
    self.mana = 100
    self.mana_max = 100
    self.visible = True

    self.pos_x, self.pos_y = None, None  # Position if on map.

  def update(self, player, world):
    # Do something?
    pass

  def wakeup(self, data):
    for k, v in data.mobs():
      setattr(self, k, v)
    if not self.gfx_id:
      self.gfx_id = str(self.type)

register_mob_class("mob", Mob)
