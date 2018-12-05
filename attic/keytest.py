class A(object):
  def __init__(self, a, b):
    self.a = a
    self.b = b

  def __hash__(self):
    return hash(self.a) ^ hash(self.b)

  def __eq__(self, obj):
    return self.a == obj.a and self.b == obj.b



a = A("ala", "kot")
b = A("ala", "kot")
c = A("ala", "kxt")

d = {a: "costam"}
print d[a]

