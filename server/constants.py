EV_UNKNOWN = 0
EV_PACKET = 1  # Packets from players.
EV_ASYNC = 2  # Any generator-based event.
EV_SPECIAL = 3  # Any other events with special handling.

# ID to mark a non-existing item/mob.
ITEM_NON_EXISTING_ID = 0xffffffffffffffff  # 64-bit ~0
MOB_NON_EXISTING_ID = -1

MOB_TYPE_PLAYER = "player"

# Pre-defined items (part of the initial map) have IDs from 0 to 2**60-1.
# Dynamically generated items (during gameplay) have IDs starting from 2**60.
ITEM_LAST_WORLD_ID = 0x0fffffffffffffff  # 60-bits of 1

DIR_NORTH = 0
DIR_SOUTH = 1
DIR_WEST = 2
DIR_EAST = 3

PLAYER_STARTING_POSITION = (185, 428, DIR_WEST)

MOVE_FORWARD = 0
MOVE_BACKWARD = 1
MOVE_STRAFE_LEFT = 2
MOVE_STRAFE_RIGHT = 3

# Translates initial direction with move direction to actual move direction.
MOVE_TO_DIR_TRANSLATION = [
  [ DIR_NORTH, DIR_SOUTH, DIR_WEST, DIR_EAST ], # DIR_NORTH
  [ DIR_SOUTH, DIR_NORTH, DIR_EAST, DIR_WEST ], # DIR_SOUTH
  [ DIR_WEST, DIR_EAST, DIR_SOUTH, DIR_NORTH ], # DIR_WEST
  [ DIR_EAST, DIR_WEST, DIR_NORTH, DIR_SOUTH ]  # DIR_EAST
]

DIR_TO_VECTOR = [
  ( 0, -1),  # DIR_NORTH
  ( 0,  1),  # DIR_SOUTH
  (-1,  0),  # DIR_WEST
  ( 1,  0)   # DIR_EAST
]

TILE_NOTHING = 0
TILE_GRASSLAND = 1
TILE_WATER = 2
TILE_MOUNTAIN = 3
TILE_SAND = 4
TILE_FOREST = 5
TILE_ROCKY_ROAD = 6
TILE_DIRT_ROAD = 7
TILE_STONE_WALL = 8
TILE_WOOD_FLOOR = 9

BLOCKING_TILES = {
    TILE_NOTHING,
    TILE_WATER,
    TILE_MOUNTAIN,
    TILE_STONE_WALL
}

FLASK_HEALTH_POTION = 0b00010000001000
FLASK_MANA_POTION   = 0b00000101000000
