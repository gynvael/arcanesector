#include <string>
#include <stdio.h>
using namespace std::string_literals;
int main(int argc, char **argv) {
  printf("res: %i\n", argv[1] == "asdf"s);
  return 0;
}
