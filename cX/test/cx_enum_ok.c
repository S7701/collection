enum { e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15, e16,
       eVeryLoooooooooooooooooooooooooooooooooooooooooooooooooooooongId, e18 = 'x', e19 };
enum { e20 };

int main(){
  if (e0 != 0) { printf("e0 != 0! e0: %d\n", e0); exit(-1); }
  if (e1 != e0 + 1) { printf("e1 != e0 + 1! e0: %d, e1: %d\n", e0, e1); exit(-1); }
  if (e2 != e0 + 2) { printf("e2 != e0 + 2! e0: %d, e1: %d\n", e0, e2); exit(-1); }
  if (e3 != e0 + 3) { printf("e3 != e0 + 3! e0: %d, e1: %d\n", e0, e3); exit(-1); }
  if (e4 != e0 + 4) { printf("e4 != e0 + 4! e0: %d, e1: %d\n", e0, e4); exit(-1); }
  if (e5 != e0 + 5) { printf("e5 != e0 + 5! e0: %d, e1: %d\n", e0, e5); exit(-1); }
  if (e6 != e0 + 6) { printf("e6 != e0 + 6! e0: %d, e1: %d\n", e0, e6); exit(-1); }
  if (e7 != e0 + 7) { printf("e7 != e0 + 7! e0: %d, e1: %d\n", e0, e7); exit(-1); }
  if (e8 != e0 + 8) { printf("e8 != e0 + 8! e0: %d, e1: %d\n", e0, e8); exit(-1); }
  if (e9 != e0 + 9) { printf("e9 != e0 + 9! e0: %d, e1: %d\n", e0, e9); exit(-1); }
  if (e10 != e0 + 10) { printf("e10 != e0 + 10! e0: %d, e1: %d\n", e0, e10); exit(-1); }
  if (e11 != e0 + 11) { printf("e11 != e0 + 11! e0: %d, e1: %d\n", e0, e11); exit(-1); }
  if (e12 != e0 + 12) { printf("e12 != e0 + 12! e0: %d, e1: %d\n", e0, e12); exit(-1); }
  if (e13 != e0 + 13) { printf("e13 != e0 + 13! e0: %d, e1: %d\n", e0, e13); exit(-1); }
  if (e14 != e0 + 14) { printf("e14 != e0 + 14! e0: %d, e1: %d\n", e0, e14); exit(-1); }
  if (e15 != e0 + 15) { printf("e15 != e0 + 15! e0: %d, e1: %d\n", e0, e15); exit(-1); }
  if (e16 != e0 + 16) { printf("e16 != e0 + 16! e0: %d, e1: %d\n", e0, e16); exit(-1); }
  if (eVeryLoooooooooooooooooooooooooooooooooooooooooooooooooooooongId != e16 + 1) { printf("eVeryLoooooooooooooooooooooooooooooooooooooooooooooooooooooongId != e16 + 1! eVeryLoooooooooooooooooooooooooooooooooooooooooooooooooooooongId: %d, e16: %d", eVeryLoooooooooooooooooooooooooooooooooooooooooooooooooooooongId, e16); exit(-1); }
  if (e18 != 'x') { printf("e18 != 'x'! e18: %d, 'x': %d\n", e18, 'x'); exit(-1); }
  if (e19 != e18 + 1) { printf("e19 != e18 + 1! e18: %d, e19: %d\n", e18, e19); exit(-1); }
  if (e20 != 0) { printf("e20 != 0! e20: %d\n", e20); exit(-1); }
  return 0;
}
