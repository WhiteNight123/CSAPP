#include <stdio.h>
#include <assert.h>

int leftmost_one(unsigned x) {
  // generate a mask that all bits after leftmost one are one
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  // mask && 1 deals with case x = 0
  return (x >> 1) + (x && 1);
}

int main(int argc, char* argv[]) {
  assert(leftmost_one(0xFF00) == 0x8000);
  assert(leftmost_one(0x6000) == 0x4000);
  assert(leftmost_one(0x0) == 0x0);
  assert(leftmost_one(0x80000000) == 0x80000000);
  return 0;
}
