from threading import Lock

__all__ = ["get_unique_id"]

# Returns a 64-bit unique ID. Actually the ID is just a locked incremented
# value, but that's enough. The values overlap on exceeding 64-bits, but uhm,
# that's not bound to happen, ever.
def get_unique_id():
  global unique_id
  with unique_id_mutex:
    ret = unique_id
    unique_id = (unique_id + 1) & 0xffffffffffffffff
    return ret

unique_id = 0x4472676e53656374  # ;)
unique_id_mutex = Lock()


