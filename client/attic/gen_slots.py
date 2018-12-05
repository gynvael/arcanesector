import random
import sys

TILE_SZ = 5.0
MIN_DIST = 1.75 / TILE_SZ  # meters / TILE_SZ

def randf():
  return float(random.randint(0, 4096)) / 4096.0

def dist(a, b):
  dx = a[0] - b[0]
  dy = a[1] - b[1]
  dist = (dx * dx + dy * dy) ** 0.5
  return dist

def check_dist(cand, current):
  if (cand[0] < MIN_DIST / 2.0 or
      cand[1] < MIN_DIST / 2.0 or
      cand[0] > 1.0 - MIN_DIST / 2.0 or
      cand[1] > 1.0 - MIN_DIST / 2.0):
    return False

  return all([dist(cand, b) >= MIN_DIST for b in current])

def gen_candidate():
  return (randf(), randf())

slots = []
attempt = 0
while len(slots) != 8:
  ns = []

  good = True

  for i in xrange(5):
    tries = 0
    while True:
      c = gen_candidate()
      if check_dist(c, ns):
        break
      tries += 1
      if tries > 1000:
        good = False
        break

    if not good:
      break
    ns.append(c)

  if good:
    print "Found in %i attempts!" % attempt
    slots.append(ns)
    attempt = 0
  else:
    attempt += 1

for s in slots:
  sys.stdout.write("    ")
  for c in s:
    print "{ %.4ff, %.4ff }," % c,
  print




