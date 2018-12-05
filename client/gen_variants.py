# Generate texture variant. Actually just rotate the textures. Could be done
# in-engine, but whatever.
import subprocess
import os

convert_bin = "d:\\commands\\bin\\convert.exe"

FILES = [
    "grass2.png",
    "water.png",
    "dirt1.png",
    "rocky_road.png",
    "sand1.png"
]

def run_convert(fname):
  path, ext = os.path.splitext(fname)

  for rotate, sufix in zip([0, 90, 180, 270], "ABCD"):
    subprocess.check_call([
        convert_bin,
        "-rotate", "%i" % rotate,
        fname,
        "%s%s%s" % (path, sufix, ext)
    ])

for fname in FILES:
  print "--- %s" % fname
  run_convert("gfx/" + fname)


