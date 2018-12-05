import sys
import time
sys.path.append('..')

from constants import *
from item import *
from high_magic_decrypt import high_magic_decrypt

__all__ = ["spellcast"]

g_spells = {}
g_high_spells = {}

def magic_spell(spell):
  def reg(handler):
    s = spell.decode("hex").ljust(8, '\0')
    g_spells[s] = handler
    return handler
  return reg

def high_magic_spell(spell):
  if len(spell) != 8:
    raise Exception("High spell name must have 8 characters")

  def reg(handler):
    g_high_spells[spell] = handler
    return handler
  return reg

def get_spell_power(spell):
  count = 0
  for b in spell:
    if b != 0:
      count += 1
  return count

def high_magic(spell, player, world):
  crypto_key = bytearray("77e79098dd34aeb7".decode("hex"))
  real_spell_name = str(high_magic_decrypt(spell, crypto_key))

  handler = g_high_spells.get(str(real_spell_name))
  if handler is None:
    player.show_text("The spell failed miserably.")
    return

  return handler(real_spell_name, player, world)

def spellcast(spell, player, world):
  power = get_spell_power(spell)

  required_mana = power * 5

  if required_mana > player.mana:
    player.show_text("Not enough mana.")
    return

  player.mana -= required_mana
  player.send_stats()  # Might be redundant, but whatever.

  if power == 8:  # High magic.
    last_time_used = getattr(player, "high_magic_last_used", 0)
    time_diff = time.time() - last_time_used

    if time_diff < 30:
      player.show_text("Too tired and failed miserably. Rest a while.")
      return

    setattr(player, "high_magic_last_used", time.time())
    return high_magic(spell, player, world)

  handler = g_spells.get(str(spell))
  if handler is None:
    player.show_text("The spell failed miserably. (%s)" % (str(spell).encode("hex")))
    return

  return handler(str(spell), player, world)

# -------------------------------------------------------------------
# Spells
# -------------------------------------------------------------------

@magic_spell("4a5a")
def spell_fire_arrow(spell, player, world):
  player.show_text("FIRE ARROW")

@magic_spell("515151")
def spell_force_ram(spell, player, world):
  # Shove a mob away.
  player.show_text("Select who you want to ram:")

  for _ in player.YIELDING_select():  # yield from
    if _ is False:
      return
    yield _

  if player.select_result is None:
    player.show_text("Nevermind...")
    return

  target_type, target = player.select_result
  if target_type != "mob":
    player.show_text("You can only shove away mobs.")
    return

  if (player.pos_x != target.pos_x or
      player.pos_y != target.pos_y):
    player.show_text("The target is too far away.")
    return

  move_vector = DIR_TO_VECTOR[player.direction]
  idx_candidate = (
      target.pos_x + move_vector[0] +
      (target.pos_y + move_vector[1]) * 512
  )

  candidate_tile = world.map.terrain[idx_candidate]
  if candidate_tile in BLOCKING_TILES:
    player.show_text("Can't push the target.")
    return

  pos_candidate = (
      idx_candidate % 512,
      idx_candidate / 512
  )

  if (pos_candidate[0] < 0 or pos_candidate[0] >= 512 or
      pos_candidate[1] < 0 or pos_candidate[1] >= 768):
    player.show_text("Can't push the target.")
    return

  items = world.map.get_items(pos_candidate[0], pos_candidate[1])
  if items:
    for item in items:
      if item.blocking:
        player.show_text("Can't push the target.")
        return

      if item.type == "teleport":
        player.show_text("A block of force hits the target and pushes it away!")
        item.teleport(target, world)
        if target.type == MOB_TYPE_PLAYER:
          target.show_text("You've been shoved away by a wall of force")
        return

  world.map.move_mob(target, pos_candidate[0], pos_candidate[1])
  if target.type == MOB_TYPE_PLAYER:
    target.send_position()
    target.show_text("You've been shoved away by a wall of force")
  world.broadcast_mobs(target.pos_x, target.pos_x)
  world.broadcast_mobs(player.pos_x, player.pos_y)
  player.show_text("A block of force hits the target and pushes it away!")



class ItemTeleportRing(Item):
  def __init__(self):
    Item.__init__(self)
    self.movable = True
    self.gfx_id = "teleport_ring"

    self.name = "Teleport Ring (Unbound)"
    self.type = "teleport_ring"
    self.teleport_set = False
    self.teleport_x = None
    self.teleport_y = None

  def use(self, player, world):
    if not self.teleport_set:
      player.show_text("Ring not bound to any location. Use proper binding spell.")
      return

    player.show_text("Teleporting in 3...")
    for _ in world.YIELDING_sleep(1.0):  # yield from
      yield _

    player.show_text("Teleporting in 2...")
    for _ in world.YIELDING_sleep(1.0):  # yield from
      yield _

    player.show_text("Teleporting in 1...")
    for _ in world.YIELDING_sleep(1.0):  # yield from
      yield _

    idx = self.teleport_x + self.teleport_y * 512
    candidate_tile = world.map.terrain[idx]
    if candidate_tile in BLOCKING_TILES:
      player.show_text("You detected flawed binding")
      self.name = "Teleport Ring (Flawed)"
      return

    world.broadcast_mobs(player.pos_x, player.pos_y, ignore=player)
    world.map.remove_mob(player)
    player.pos_x = self.teleport_x
    player.pos_y = self.teleport_y
    world.map.add_mob(player)
    player.send_position()
    world.broadcast_mobs(player.pos_x, player.pos_y, ignore=player)
    player.show_text("You feel dizzy, but it seems the ring worked.")

register_item_class("teleport_ring", ItemTeleportRing)

@magic_spell("76777879")
def spell_bind_teleport_ring(spell, player, world):
  player.show_text("Select teleport ring you want to bind to this location")

  for _ in player.YIELDING_select():  # yield from
    if _ is False:
      return
    yield _

  if player.select_result is None:
    player.show_text("Aborting teleport ring bind.")
    return

  target_type, target = player.select_result
  if target_type != "item":
    player.show_text("Aborting teleport ring bind.")
    return

  item, item_location, item_location_info = target

  if item.type != "teleport_ring":
    player.show_text("Not a teleport ring, aborting spell.")
    return

  player.show_text("Starting location binding ritual...")
  backup_pos_x = player.pos_x
  backup_pos_y = player.pos_y
  item.teleport_set = False
  item.name = "Teleport Ring (Unbound)"

  for _ in world.YIELDING_sleep(1.0):  # yield from
    yield _

  player.show_text("Verifying location of planets...")
  item.teleport_x = player.pos_x

  for _ in world.YIELDING_sleep(1.0):  # yield from
    yield _

  player.show_text("Measuring disturbance of force...")

  for _ in world.YIELDING_sleep(1.0):  # yield from
    yield _

  item.teleport_y = player.pos_y
  player.show_text("Calming local aether fields...")

  for _ in world.YIELDING_sleep(1.0):  # yield from
    yield _

  player.show_text("Probing nearby dimensions...")

  for _ in world.YIELDING_sleep(1.0):  # yield from
    yield _

  if player.pos_x != backup_pos_x or player.pos_y != backup_pos_y:
    player.show_text("Ritual failed! Don't move next time.")
    return

  item.name = "Teleport Ring (%i, %i)" % (item.teleport_x, item.teleport_y)
  item.teleport_set = True
  player.show_text("Teleport ring bound to new location!")
  player.send_inventory()
  player.send_ground()

@high_magic_spell("vexillum")
def spell_fire_arrow(spell, player, world):
  try:
    with open("flags/flag_spell.txt") as f:
      flag = f.read().strip()
    player.show_text("You've summoned the flag: %s" % flag)
  except IOError:
    player.show_text("ERROR: Contact CTF admin, this should not happen.")

