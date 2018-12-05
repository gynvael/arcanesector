import timeit
print timeit.timeit(stmt='"".join([ch if random.randint(0,1)==0 else "." for ch in text])', setup='import random\ntext="ala ma kota kot ma ale"', number=100000)
print timeit.timeit(stmt='"".join([ch if rnd()==0 else "." for ch in text])', setup='import random\ntext="ala ma kota kot ma ale"\ndef rnd():\n  r = rnd.R % 2\n  rnd.R = (rnd.R * 1337 + 977) % 1235\n  return r\nrnd.R=1234', number=100000)
