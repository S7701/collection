int imin, imax, i, *pi;
char c, pc;
struct { int m0; char m1;} s, *ps;

int main(){
  imin = 1 << sizeof (int) * 8 - 1;
  imax = imin - 1;
  i = 4711; pi = &i;
  c = 'x'; pc = &c;
  s.m0 = 1147; s.m1 = 'y'; ps = &s;

  printf("imax: %d, imin: %d\n", imax, imin);

  // num + num
  if (2147483647 + 1 != -2147483648) { printf("1 + 2 != 3! : %d\n", 1 + 2); exit(-1); }
  // var + imm
  if (imax + 1 != imin) { printf("imax + 1 != imin! imax:%d, imin:%d\n", imax, imin); exit(-1); }
  // imm + var
  if (1 + imax != imin) { printf("1 + imax != imin! imax:%d, imin:%d\n", imax, imin); exit(-1); }
  // var + var
  // max_int + 1

  // ptr + num

  // num + ptr

  if (0) { printf("! : %d\n", ); exit(-1); }
  return 0;
}
