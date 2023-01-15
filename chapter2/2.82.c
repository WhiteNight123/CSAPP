#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include "./lib/random.h"

int A(int x, int y) {
  return (x < y) == (-x > -y);
}

int B(int x, int y) {
  return ((x + y) << 4) + y - x == 17 * y + 15 * x;
}

int C(int x, int y) {
  return ~x + ~y + 1 == ~(x + y);
}

int D(int x, int y) {
  unsigned ux = (unsigned) x;
  unsigned uy = (unsigned) y;

  return (ux - uy) == -(unsigned) (y - x);
}

int E(int x, int y) {
  return ((x >> 2) << 2) <= x;
}

int main(int argc, char* argv[]) {
  init_seed();
  int x = random_int();
  int y = random_int();

  // assert(!A(INT_MIN, 0));
  assert(B(x, y));
  assert(C(x, y));
  assert(D(x, y));
  assert(E(x, y));
  return 0;
}
