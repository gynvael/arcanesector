#include <windows.h>

int main(void) {
  HWND hwnd=(HWND)0x481350;
  SendMessage(hwnd, WM_CLOSE, 0,0);

  return 0;
}
