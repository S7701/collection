enum { CharArrSz = 15, IntSz = 5 };

char c1;
char ca1[13];
int ia1[3];
char c2;
char ca2[CharArrSz];
int ia2[IntSz];
char c3;

int f1() {
  char c1;
  char ca1[13];
  int ia1[3];
  char c2;
  char ca2[CharArrSz];
  int ia2[IntSz];
  char c3;

  printf("f1: &c1:%p, &ca1:%p, &ia1:%p, &c2:%p, &ca2:%p, &ia2:%p, &c3:%p\n", &c1, &ca1, &ia1, &c2, &ca2, &ia2, &c3);
  return 0;
}

int main(int argc, int**argv) {
  printf("global: &c1:%p, &ca1:%p, &ia1:%p, &c2:%p, &ca2:%p, &ia2:%p, &c3:%p\n", &c1, &ca1, &ia1, &c2, &ca2, &ia2, &c3);
  f1();
  return 0;
}