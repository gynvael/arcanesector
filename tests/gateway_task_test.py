#!/usr/bin/python
from Crypto.Cipher import AES
from md5 import md5

BLOCK = "#!/bin/bash\n# Ba"

def test(passwd):
  key = md5(passwd).digest()
  cipher = AES.new(key, AES.MODE_ECB)
  return cipher.decrypt(BLOCK).startswith("CFG")

def dec(passwd, txt):
  key = md5(passwd).digest()
  cipher = AES.new(key, AES.MODE_ECB)
  return cipher.encrypt(txt)


# 36393351
#for i in xrange(10000000, 200000000):
#  if test(str(i)):
#    print i
#    break

DATA = [
"CFG\x85[\x0fi}1\x9c\x82\xc6\xf8\x09z\xaf",
"+&\x88\xf6\x95\x07\xe86Ek[\xe2z\x86\xfcL",
"_\xc7\xd2Q[#k\xec\xe8`\xa8S\xc88\x86\xa6",
"\xfa\xff\xc9\xecKtj\xd3WL\xe2<t\x81\xee\xfa",
"\xae\xd0l5\"R$\x8e\xd4*k\xf7\xf8\x99\x12{",
"H\xc9\x85W\xed\xf5V\xd0\xfe3\xca\x99\x0e\x97h\x95",
"\xd7\xd1\xb4\xe5\xde-\xb4]Hj\x0e\x87\xcb\xed\x9f\xa9",
"731e9c207ce5cd231224a041333ef2b6".decode("hex"),
"894cf0cd1277cdc8fa7c96303d7734af".decode("hex"),
"bc29019fb212209fe2536396ec513ada".decode("hex"),
"86a30049cc53345dd38c8c4a5475334b".decode("hex"),
"caad2c02885f779abba126e55cdeb23e".decode("hex"),
"4bc635b718440b2246dd79414e6b29d8".decode("hex"),
"c54d4dd449db7eed63644fdacd66cf6b".decode("hex"),
]

o = []
for d in DATA:
  if len(d) != 0x10:
    print "LENGTH: ", len(d), `d`
  o.append(dec("36393351", d))

print ''.join(o)

