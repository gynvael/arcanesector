#include <stdio.h>

class Base {
 public:
  Base() {
    printf("Base(): ");
    this->test();
  }

  virtual void test() {
    puts("Base::test");
  }

  void init() {
    printf("Init: ");
    this->test();
  }
};

class Child : public Base {
 public:
  Child() {
    printf("Child(): ");
    this->test();
  }

  virtual void test() {
    puts("Child::test");
  }

  static Base* Make() {
    Child *t = new Child{};
    t->init();
    return t;
  }
};

int main(void) {
  Base *a = new Child{};
  puts("--");
  a->test();

  puts("--");
  Base *b = Child::Make();


}

