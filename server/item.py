from constants import *

g_item_classes = {}
def register_item_class(name, item_class):
  if name in g_item_classes:
    raise Exception("register_item_class duplicate: %s" % name)

  g_item_classes[name] = item_class


class Item(object):
  def __init__(self):
    # Initial values are for a non-existing item.
    self.id = ITEM_NON_EXISTING_ID
    self.movable = False
    self.gfx_id = ""
    self.name = ""
    self.blocking = False  # True means it blocks the way.
    self.type = ""  # Optional type field.

    self.pos_x, self.pos_y = None, None  # Position if on map.

  def use(self, player, world):
    pass

  def wakeup(self, data):
    for k, v in data.items():
      setattr(self, k, v)
    if not self.gfx_id:
      self.gfx_id = str(self.type)


