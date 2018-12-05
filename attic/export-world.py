# This converts the Tiled .json map into Arcane Sector's compatible json.
# 1. The "Imported" and "Patches" layers are merged and the output uses bytes
#    instead of dwords per tile (but the zlib+base64 compression is still used).
# 2. The
import json
import sys
import base64
import struct
import random
import xml.etree.ElementTree as ET

# This is for "compilation-time" error checking.
# Type prefixed with "*" means "optional field, e.g. "*int" means field might
# not exist, but if it does, it has to be an int.
VALID_SYSTEM_OBJECTS = {
    "spawn_item": [
        ("cooldown", "int"),
        ("item_class", "string"),
    ],
    "spawn": [
        ("cooldown", "int"),
        ("mob_class", "string"),
    ],
}

VALID_ITEMS = {
    "door": [
        ("teleport_group", "string"),
    ],
    "sign": [
        ("text", "string"),
    ],
    "flag_sign": [
        ("flag_file", "string"),
    ],
    "lamppost": [
    ],
    "switch": [
        ("switch_group", "string"),
    ],
    "blocker": [
        ("blocker_group", "string"),
    ],
}

TEMPLATES = {}
def obj_get_template(obj):
  if "template" not in obj:
    return None

  template_name = obj["template"]
  if template_name in TEMPLATES:
    return TEMPLATES[template_name]

  with open(template_name) as f:
    template = ET.fromstring(f.read())

  TEMPLATES[template_name] = template
  return template

# Attribs MUST exist.
def obj_get_attrib(obj, name):
  if name in obj:
    return obj[name]

  template = obj_get_template(obj)
  assert template is not None

  object_attributes = template.findall("./object")[0].attrib

  if name not in object_attributes:
    if name == "name":
      return ""

  return object_attributes[name]

# Properties are custom and may not exist.
def obj_get_property(obj, name):
  if "properties" in obj:
    for prop in obj["properties"]:
      if prop["name"] == name:
        return prop

  template = obj_get_template(obj)
  if template is None:
    return None

  for prop in template.findall("./object/properties/property"):
    if prop.attrib["name"] == name:
      v = prop.attrib["value"]
      t = "string"
      try:
        if prop.attrib["type"] == "int":
          v = int(prop.attrib["value"])
          t = "int"
      except KeyError:
        pass


      return {
          "name": name,
          "value": v,
          "type": t
      }
  return None

def obj_get_all_props(obj):
  names = []

  if "properties" in obj:
    for prop in obj["properties"]:
      names.append(prop["name"])

  template = obj_get_template(obj)
  if template is not None:
    for prop in template.findall("./object/properties/property"):
      names.append(prop.attrib["name"])

  return names

def validator_msg(obj, msg_type, msg):
  print "[%s] object of type \"%s\", name/comment \"%s\" at %i,%i: %s" % (
    msg_type,
    obj_get_attrib(obj, "type"),
    obj_get_attrib(obj, "name"),
    obj_get_attrib(obj, "x"),
    obj_get_attrib(obj, "y"),
    msg
  )

def validator_error(obj, msg):
  return validator_msg(obj, "!ERROR!", msg)

def validator_warning(obj, msg):
  return validator_msg(obj, "warning", msg)

def convert_common(obj, required_custom_fields_set):
  # Start with common properties.
  output = {
    "id": obj_get_attrib(obj, "id"),
    "type": obj_get_attrib(obj, "type"),
    "comment": obj_get_attrib(obj, "name"),
    "pos_x": obj_get_attrib(obj, "x") / 16,
    "pos_y": obj_get_attrib(obj, "y") / 16 - 1,
  }

  # Copy custom fields (only specified fields are copied).
  if output["type"] not in required_custom_fields_set:
    validator_error(obj, "unknown object type")
    return None

  required_custom_fields = required_custom_fields_set[output["type"]]
  for field, field_type in required_custom_fields:
    assert field not in output

    is_optional = field_type.startswith("*")
    if is_optional:
      field_type = field_type[1:]

    prop = obj_get_property(obj, field)
    if prop is None:
      if is_optional:
        continue  # Optional field.
      validator_error(obj, "missing property \"%s\"" % field)
      continue

    if prop["type"] != field_type:
      validator_error(
          obj, "wrong property \"%s\" type - is \"%s\", must be \"%s\"" % (
              field, prop["type"], field_type))
      continue

    output[field] = prop["value"]

  # Check for extra fields.
  all_field_names = set(obj_get_all_props(obj))
  required_custom_field_names = set([f[0] for f in required_custom_fields])
  extra_fields = all_field_names - required_custom_field_names
  if extra_fields:
    validator_warning(
        obj, "extra fields: %s" % ' '.join(list(extra_fields)))

  return output


def convert_system_object(obj):
  return convert_common(obj, VALID_SYSTEM_OBJECTS)

def convert_item(item):
  return convert_common(item, VALID_ITEMS)

def dd(v):
  return struct.pack("I", v)

def dw(v):
  return struct.pack("H", v)

def db(v):
  return struct.pack("B", v)

def clamp(n, smallest, largest):
  return max(smallest, min(n, largest))

def decode_layer_data(layer):
  data = layer["data"].decode("base64").decode("zlib")
  sz = len(data) / 4
  return struct.unpack("%iI" % sz, data)

def encode_layer_data_8bit_server(data):
  new_data = ''.join([db(v) for v in data])
  new_data = new_data.encode("zlib")
  new_data = base64.b64encode(new_data)
  return new_data

def calc_tree_density(x, y, m):
  # Distance from trees at x,y to the nearest sand or grass.
  # Totally ignoring bounds checking lol.

  closest_dist = 123456.0

  DIST_CHECK = 15

  for j in xrange(y - DIST_CHECK, y + DIST_CHECK + 1):
    for i in xrange(x - DIST_CHECK, x + DIST_CHECK + 1):
      idx = i + j * 512
      t = m[idx] & 0xff
      if t not in { 1, 4 }:
        continue

      dx = float(x - i)
      dy = float(y - j)
      dsq = dx * dx + dy * dy

      if dsq < closest_dist:
        closest_dist = dsq

  closest_dist = closest_dist ** 0.5
  closest_dist = clamp(closest_dist, 0.0, 14.0)
  return clamp(int(closest_dist) / 3, 0, 4)

tree_density_map = bytearray(512 * 768)

def preprocess_forests(x, y, v, m):
  tree_count = calc_tree_density(x, y, m)
  tree_density_map[x + y * 512] = tree_count << 5

  tree_slot = random.randint(0, 7)

  tree_set = 3  # Mix by default.

  if 156 <= x <= 345 and 229 <= y <= 345:
    tree_set = 1  # Leaf trees only near the big mountain.

  if 373 <= x <= 441 and 317 <= y <= 376:
    tree_set = 0  # Pine trees only on the island on the south east.

  if 346 <= x <= 368 and 155 <= y <= 180:
    tree_set = 2  # Dead trees in this small wood.

  return tree_count | (tree_slot << 3) | (tree_set << 6)

mountain_height_map = bytearray(512 * 768)

def calc_distance(x, y, m, not_one_of, max_dist=15):
  # Totally ignoring bounds checking lol.

  closest_dist = 123456.0

  for j in xrange(y - max_dist, y + max_dist + 1):
    for i in xrange(x - max_dist, x + max_dist + 1):
      idx = i + j * 512
      t = m[idx] & 0xff
      if t in not_one_of:
        continue

      dx = float(x - i)
      dy = float(y - j)
      dsq = dx * dx + dy * dy

      if dsq < closest_dist:
        closest_dist = dsq

  closest_dist = closest_dist ** 0.5
  closest_dist = clamp(closest_dist, 0.0, max_dist - 1.0)
  return closest_dist

def preprocess_mountains(x, y, v, m):
  max_dist = 15
  coef = 127.0 / float(max_dist - 1)

  height = int(calc_distance(x, y, m, {3, 6}, max_dist) * coef)
  height += random.randint(0, 3)
  height = clamp(height, 0, 127)
  value = height
  special_flag = 0

  mountain_height_map[x + y * 512] = value << 1

  return value | (special_flag << 7)

def preprocess_random(x, y, v, m):
  return random.randint(0, 255)

def preprocess_client_map(m):
  # TODO: Add some smart preprocessing. The idea is to get a diverse map with
  # no manual work (e.g. 10 types of forest types, roads looking like roads,
  # etc).
  # Format
  # For each 16-bit pixel:
  #   Bottom 8 bits: tile type (same as server 8 bits)
  #   Top 8 bits: tile variant
  # Note: The tile variant might mean different things depending on the type.
  #       (this should be documented in the .md file).

  handlers = {
      5: preprocess_forests,
      3: preprocess_mountains,

      # Randomize all grounds and floors.
      1: preprocess_random,
      2: preprocess_random,
      4: preprocess_random,
      6: preprocess_random,
      7: preprocess_random,
      9: preprocess_random,
  }

  for i, v in enumerate(m):
    if v not in handlers:
      continue

    handler = handlers[v]

    x = i % 512
    y = i / 512

    v = (handler(x, y, v, m) << 8) | v
    m[i] = v

  return [v for v in m]

def encode_layer_data_16bit_client(data):
  new_data = ''.join([dw(v) for v in data])
  return new_data

def get_layer(world_map, layer_name):
  for layer in world_map["layers"]:
    if layer["name"] == layer_name:
      return layer

  raise Exception("Layer %s not found" % layer_name)

def main(argv):
  with open("world-map.json", "rb") as f:
    world_map = json.load(f)

  # Merge base/patches.
  imported_layer = get_layer(world_map, "Imported")
  patches_layer = get_layer(world_map, "Patches")

  imported_data = decode_layer_data(imported_layer)
  patches_data = decode_layer_data(patches_layer)

  terrain_data = [
    patch if patch != 0 else base
    for patch, base in zip(patches_data, imported_data)
  ]

  # Encode the terrain.
  terrain = encode_layer_data_8bit_server(terrain_data)
  print "Terrain layer after compression/encoding: %u bytes" % len(terrain)

  # Handle pre-defined system objects.
  system_objects = []
  system_objects_layer = get_layer(world_map, "System Objects")
  for obj in system_objects_layer["objects"]:
    converted = convert_system_object(obj)  # Does checks.
    if converted is None:
      continue
    system_objects.append(converted)

  print "Added system objects: %i" % len(system_objects)

  # Handle pre-defined items.
  items = []
  items_layer = get_layer(world_map, "Items")
  for obj in items_layer["objects"]:
    converted = convert_item(obj)  # Does checks.
    if converted is None:
      continue
    items.append(converted)

  print "Added items: %i" % len(items)

  # Final.
  map = {
      "w": imported_layer["width"],
      "h": imported_layer["height"],
      "terrain": terrain,
      "system_objects": system_objects,
      "items": items
  }

  print "Writing server map..."
  with open("../server/data/map.json", "wb") as f:
    json.dump(map, f)

  print "Preprocessing client map..."
  client_terrain_data = preprocess_client_map(terrain_data)  # TODO: perhaps tell client about doors and stuff.

  print "Writing client map..."
  with open("../client/map.data", "wb") as f:
    f.write(encode_layer_data_16bit_client(client_terrain_data))

  print "Writing debug data..."
  with open("tree_density.raw", "wb") as f:
    f.write(tree_density_map)

  with open("mountain_height.raw", "wb") as f:
    f.write(mountain_height_map)

  print "Done."


if __name__ == "__main__":
  sys.exit(main(sys.argv))

