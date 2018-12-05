import socket

# Both of these are vulnerable to slowloris DoS, but in this scenario it doesn't matter.
# Actually recvuntil's implementation is super slow by itself.
def recvuntil(sock, txt):
  d = ""
  while d.find(txt) == -1:
    dnow = sock.recv(1)
    if len(dnow) == 0:
      raise socket.error(107, "Disconnected on recv")
    d += dnow
  return d

def recvall(sock, n):
  d = []
  total_length = 0
  while total_length != n:
    dnow = sock.recv(n - total_length)
    if len(dnow) == 0:
      raise socket.error(107, "Disconnected on recv")
    d.append(dnow)
    total_length += len(dnow)
  return ''.join(d)

# Proxy object for sockets.
class gsocket(object):
  def __init__(self, *p):
    self._sock = socket.socket(*p)

  def __getattr__(self, name):
    return getattr(self._sock, name)

  def recvall(self, n):
    return recvall(self._sock, n)

  def recvuntil(self, txt):
    return recvuntil(self._sock, txt)

class gsocket_wrapper(gsocket):
  def __init__(self, s):
    self._sock = s

