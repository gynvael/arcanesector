# Common functions / classes for event handlers.
class GetHandlersMixin(object):
  MAGIC_PREFIX = "handle_"
  MAGIC_PREFIX_LEN = len(MAGIC_PREFIX)

  def get_handlers(self):
    return {
        k[self.MAGIC_PREFIX_LEN:]: getattr(self, k)
            for k in dir(self) if k.startswith(self.MAGIC_PREFIX)
    }

# Remove all characters which are non-printable ASCII.
def escape_text(s):
  s = bytearray(s)
  out = [ch for ch in s if ch >= 0x20 and ch <= 0x7e]
  return str(bytearray(out))

