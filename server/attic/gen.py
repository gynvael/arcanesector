import os

#print os.urandom(8).encode("hex")

CRYPTO_KEY = bytearray("77e79098dd34aeb7".decode("hex"))

# This has to be nicely written and readable - it will NOT be given to the
# players. On the other hand the decrypt function has to be inlined (though
# should not be obfuscated).

def rshift_row(key, i):
  tmp = key[i * 8 + 7]
  key[i * 8 + 1:i * 8 + 8] = key[i * 8 + 0:i * 8 + 7]
  key[i * 8] = tmp

def lshift_row(key, i):
  tmp = key[i * 8 + 0]
  key[i * 8 + 0:i * 8 + 7] = key[i * 8 + 1:i * 8 + 8]
  key[i * 8 + 7] = tmp

def ushift_col(key, i):
  tmp = key[i]
  for j in xrange(7):
    key[i + j * 8] = key[i + (j + 1) * 8]
  key[i + 7 * 8] = tmp

def dshift_col(key, i):
  tmp = key[i + 7 * 8]
  for j in xrange(7):
    key[i + (j + 1) * 8] = key[i + j * 8]
  key[i] = tmp

def high_magic_decrypt_nice(block, key):
  if len(block) != 8:
    raise Exception("Incorrect block size")

  if len(key) != 8:
    raise Exception("Incorrect key size")

  key = bytearray(key)

  hex_matrix = (
      "b42bef325e1d3f74cfff8c90abdc6d2ad3bfad8ce9899dd87df6eaf444ffd036"
      "eb642ff20184a05196b4d4d641fbf3dcef4aa28b32293952256e009bf463f492"
  )

  matrix = bytearray(hex_matrix.decode("hex"))

  block = bytearray(block)

  # For 16 rounds:
  for j in xrange(16):
    #print "encmatrix %2i: " % j, str(matrix).encode("hex")

    #   1. Do transformations on internal registers.
    for i in xrange(8): block[i] = (block[i] + matrix[i + 0 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] ^ matrix[i + 1 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] - matrix[i + 2 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] ^ matrix[i + 3 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] + matrix[i + 4 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] ^ matrix[i + 5 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] - matrix[i + 6 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] ^ matrix[i + 7 * 8]) & 0xff

    #   2. Some form of bit/byte mixing (can be shifting).
    block = block[7:8] + block[0:7]

    for i in xrange(6, -1, -1): block[i] ^= block[i+1]

    #   3. Do transformations on the matrix.
    for i in xrange(key[0] % 67): rshift_row(matrix, i % 8)

    for i in xrange(key[1] % 67): ushift_col(matrix, i % 8)

    for i in xrange(key[2]): matrix[i % 64] = (matrix[i % 64] + key[2]) & 0xff

    for i in xrange(key[3]): matrix[i % 64] = (matrix[i % 64] - key[3]) & 0xff

    for i in xrange(key[4]): matrix[i % 64] = (matrix[i % 64] + key[4]) & 0xff

    for i in xrange(key[5]): matrix[i % 64] = (matrix[i % 64] - key[5]) & 0xff

    for i in xrange(key[6] % 67): rshift_row(matrix, i % 8)

    for i in xrange(key[7] % 67): ushift_col(matrix, i % 8)

    #  4. Transform key in predictable and reversible way.
    for i in xrange(8): key[i] = (key[i] + 97) & 0xff

  return block

def high_magic_encrypt(block, key):
  hex_matrix = (
      "b42bef325e1d3f74cfff8c90abdc6d2ad3bfad8ce9899dd87df6eaf444ffd036"
      "eb642ff20184a05196b4d4d641fbf3dcef4aa28b32293952256e009bf463f492"
  )

  matrix = bytearray(hex_matrix.decode("hex"))

  block = bytearray(block)

  key = bytearray(key)

  # Start with getting the matrix to the 15th iteration.
  matrix_at = []
  for j in xrange(16):
    matrix_at.append(matrix[:])

    for i in xrange(key[0] % 67): rshift_row(matrix, i % 8)

    for i in xrange(key[1] % 67): ushift_col(matrix, i % 8)

    for i in xrange(key[2]): matrix[i % 64] = (matrix[i % 64] + key[2]) & 0xff

    for i in xrange(key[3]): matrix[i % 64] = (matrix[i % 64] - key[3]) & 0xff

    for i in xrange(key[4]): matrix[i % 64] = (matrix[i % 64] + key[4]) & 0xff

    for i in xrange(key[5]): matrix[i % 64] = (matrix[i % 64] - key[5]) & 0xff

    for i in xrange(key[6] % 67): rshift_row(matrix, i % 8)

    for i in xrange(key[7] % 67): ushift_col(matrix, i % 8)

    for i in xrange(8): key[i] = (key[i] + 97) & 0xff


  # Decrypt rounds.
  for j in xrange(15, -1, -1):
    #print "decmatrix %2i: " % j, str(matrix_at[j]).encode("hex")

    for i in xrange(0, 7, 1): block[i] ^= block[i+1]
    block = block[1:8] + block[0:1]

    for i in xrange(8): block[i] = (block[i] ^ matrix_at[j][i + 7 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] + matrix_at[j][i + 6 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] ^ matrix_at[j][i + 5 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] - matrix_at[j][i + 4 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] ^ matrix_at[j][i + 3 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] + matrix_at[j][i + 2 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] ^ matrix_at[j][i + 1 * 8]) & 0xff

    for i in xrange(8): block[i] = (block[i] - matrix_at[j][i + 0 * 8]) & 0xff

  return block


# CTF function ------------------------------------------------------
from high_magic_decrypt import high_magic_decrypt

def enc_and_print(what):
  if len(what) != 8:
    raise Exception("what should be 8")

  ret = high_magic_encrypt(what, CRYPTO_KEY)

  ret_dec = high_magic_decrypt_nice(ret, CRYPTO_KEY)

  print "%s --> %s --> %s" % (what, str(ret).encode('hex'), str(ret_dec))

def test():
  for i in xrange(20):
    key = os.urandom(8)
    block = os.urandom(8)
    ret = high_magic_encrypt(block, key)
    ret_dec = high_magic_decrypt_nice(ret, key)
    ret_dec2 = high_magic_decrypt(ret, key)

    if ret_dec != block:
      print "WTF dec_nice", `key`, `block`, `ret`, `ret_dec`

    if ret_dec2 != block:
      print "WTF dec_ctf", `key`, `block`, `ret`, `ret_dec`

  print "All fine."

test()
enc_and_print("vexillum")


