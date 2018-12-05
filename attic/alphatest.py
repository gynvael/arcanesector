from random import randint

for _ in xrange(10000):
  a = randint(0, 255)
  b = randint(0, 255)
  alpha = randint(0, 255)
  alpha_f = alpha / 255.
  correct = int(a * alpha_f + b * (1.0 - alpha_f))
  intbased = (a * alpha + b * (255 - alpha)) >> 8

  print "%3i %3i alpha=%3i, cor=%3i int=%3i, diff=%i" % (
      a, b, alpha, correct, intbased, abs(correct - intbased)
  )


