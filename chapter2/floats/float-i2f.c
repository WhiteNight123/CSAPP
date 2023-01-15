#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include "float-i2f.h"

int bits_length(int i) {
  if ((i & INT_MIN) != 0) {
    return 32;
  }

  unsigned u = (unsigned)i;
  int length = 0;
  while (u >= (1<<length)) {
    length++;
  }
  return length;
}

unsigned bits_mask(int l) {
  return (unsigned) -1 >> (32-l);
}

float_bits float_i2f(int i) {
  unsigned sig, exp, frac, rest, exp_sig, round_part;
  unsigned bits, fbits;
  unsigned bias = 0x7F;

  if (i == 0) {
    sig = 0;
    exp = 0;
    frac = 0;
    return sig << 31 | exp << 23 | frac;
  }
  if(i == INT_MIN) {
    sig = 1;
    exp = bias + 31;
    frac = 0;
    return sig << 31 | exp << 23 | frac;
  }

  sig = 0;
  // 2's complatation
  if (i < 0) {
    sig = 1;
    i = -i;
  }

  bits = bits_length(i);
  fbits = bits - 1;
  exp = bias + fbits;

  rest = i & bits_mask(fbits);
  if (fbits <= 23) {
    frac = rest << (23 - fbits);
    exp_sig = exp << 23 | frac;
  } else {
    int offset = fbits - 23;
    int round_mid = 1 << (offset - 1);

    round_part = rest & bits_mask(offset);
    frac = rest >> offset;
    exp_sig = exp << 23 | frac;

    if (round_part < round_mid) {
      // nothing
    } else if (round_part > round_mid) {
      exp_sig += 1;
    } else {
      // round_part == round_mid
      if ((frac & 0x1) == 1) {
        // round to even
        exp_sig += 1;
      }
    }
  }
  
  return sig << 31 | exp_sig;
}
