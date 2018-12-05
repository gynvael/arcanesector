# Replaces the "Imported" layer in world-map.json.
# Do remember to turn off Tiled when doing this.
import json
import sys
import base64
import struct

def dd(v):
  return struct.pack("I", v)

def db(v):
  return struct.pack("B", v)

def decode_layer_data(layer):
  data = layer["data"].decode("base64").decode("zlib")
  sz = len(data) / 4
  return struct.unpack("%iI" % sz, data)

def encode_layer_data(data):
  new_data = ''.join([dd(v) for v in data])
  new_data = new_data.encode("zlib")
  new_data = base64.b64encode(new_data)
  return new_data

with open("world-map.json", "rb") as f:
  world_map = json.load(f)

with open("world-map.json.backup", "wb") as f:
  json.dump(world_map, f, indent=2)

imported_layer = None
patches_layer = None
for layer in world_map["layers"]:

  if layer["name"] == "Imported":
    imported_layer = layer

  if layer["name"] == "Patches":
    patches_layer = layer

imported_data = decode_layer_data(imported_layer)
patches_data = decode_layer_data(patches_layer)

# Merge
merged_data = [
    patch if patch != 0 else base
    for patch, base in zip(patches_data, imported_data)
]
empty_data = [0] * len(patches_data)

imported_layer["data"] = encode_layer_data(merged_data)
patches_layer["data"] = encode_layer_data(empty_data)

with open("world-map.json", "wb") as f:
  json.dump(world_map, f, indent=2)

# Dump merged.
with open("merged_dump.data", "wb") as f:
  data = ''.join([db(v) for v in merged_data])
  f.write(data)

with open("patches_dump.data", "wb") as f:
  data = ''.join([db(v) for v in patches_data])
  f.write(data)

