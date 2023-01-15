#include <stdio.h>
#include <assert.h>

int A(int x) {
  return (x << 4) + x;
}

int B(int x) {
  return x - (x << 3);
}

int C(int x) {
  return (x << 6) - (x << 2);
}

int D(int x) {
  return (x << 4) - (x << 7);
}

int main(int argc, char* argv[]) {
  int x = 0x87654321;
  assert(A(x) == 17 * x);
  assert(B(x) == -7 * x);
  assert(C(x) == 60 * x);
  assert(D(x) == -112 * x);
  return 0;
}
