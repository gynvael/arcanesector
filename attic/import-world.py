# Replaces the "Imported" layer in world-map.json.
# Do remember to turn off Tiled when doing this.
import json
import sys
import base64
import struct

def dd(v):
  return struct.pack("I", v)

with open("world-map.json", "rb") as f:
  world_map = json.load(f)

with open("world-map.json.backup", "wb") as f:
  json.dump(world_map, f, indent=2)

imported_layer = None
for layer in world_map["layers"]:
  if layer["name"] == "Patches":
    imported_layer = layer
    break

# Dump the current layer.
assert imported_layer is not None
data = imported_layer["data"].decode("base64").decode("zlib")
with open("imported_layer_dump.data", "wb") as f:
  f.write(data)

# Actually import the data (it has 8bpp index format).
with open("flag.data", "rb") as f:
  new_raw_data = bytearray(f.read())

new_data = ''.join([dd(v) for v in new_raw_data])
new_data = new_data.encode("zlib")
new_data = base64.b64encode(new_data)
imported_layer["data"] = new_data

with open("world-map.json", "wb") as f:
  json.dump(world_map, f, indent=2)

