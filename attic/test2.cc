#include <vector>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <algorithm>


class MyClass {
 public:
  int a;
};

std::unique_ptr<MyClass> f(int a) {
  auto p = std::make_unique<MyClass>();
  p->a = a;

  if (a == 5) {
    return nullptr;
  }

  return p;
}

int main(void) {
  auto p1 = f(2);
  auto p2 = f(5);

  printf("%p %p\n", p1.get(), p2.get());

  printf("%zu\n", std::max<size_t>(2, 45ULL));


  return 0;
}


