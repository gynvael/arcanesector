#include <cstdio>
#include <cstring>
#include <cstdlib>

int main(int argc, char **argv, char **envp) {
  for (int i = 0; i < argc; i++) {
    printf("ARGV[%i]: %s\n", i, argv[i]);
  }

  while (*envp) {
    if (strlen(*envp) > strlen("ARCANE_") && memcmp(*envp, "ARCANE_", 7) == 0) {
      printf("ENVP: %s\n", *envp);
    }
    envp++;
  }

  system("ls -la /proc/`pgrep client`/fd");
}
