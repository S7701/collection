struct s0 { char m0; };

struct S1 {
  int m0, *m1;
  char m2, m3, m4, *m5;
};

struct S2 {
  int m0, *m1;
  char m2, m3, m4, *m5;
  struct S1 m6, *m7;
};

int i1;
struct s0 s0;
struct S1 s1, *p1;
struct S2 s2, *p2;
struct {
  char m0;
  int m1;
} s3;
int i2;

int f1(struct S1 a1, struct S2 *a2) {
  int i;
  struct s0 l0;
  struct S1 l1;
  struct S2 *l2;
  return 1;
}

int main(){
  if (&s0 != &i1 + 1) { printf("&s0 != &i1 + sizeof (int)! &s0: %p, &i1: %p\n", &s0, &i1); exit(-1); }

  if (&s1 != &i1 + 2) { printf("&s1 != &i1 + 2! &s1: %p, &i: %p\n", &s1, &i1); exit(-1); }

  if (sizeof (struct S1) != 16) { printf("sizeof (struct S1) != 16! sizeof (struct S1): %d\n", sizeof (struct S1)); exit(-1); }

  s1.m0 = 4711;
  s1.m1 = &s1.m0;
  s1.m2 = 'x';
  s1.m5 = &s1.m2;

  if (s1.m0 != 4711) { printf("s1.m0 != 4711! s1.m0: %d\n", s1.m0); exit(-1); }
  if (*s1.m1 != 4711) { printf("*s1.m1 != 4711! *s1.m0: %d\n", *s1.m1); exit(-1); }
  if (s1.m2 != 'x') { printf("s1.m2 != 'x'! s1.m2: '%c'\n", s1.m2); exit(-1); }
  if (*s1.m5 != 'x') { printf("*s1.m5 != 'x'! *s1.m5: '%c'\n", *s1.m5); exit(-1); }

  if (&s1.m0 != &s1) { printf("&s1.m0 != &s1! &s1: %p, &s1.m0: %p\n", &s1, &s1.m0); exit(-1); }
  if (&s1.m1 != &s1.m0 + 1) { printf("&s1.m1 != &s1.m0 + 1! &s1.m0: %p, &s1.m1: %p\n", &s1.m0, &s1.m1); exit(-1); }
  if (&s1.m2 != &s1.m1 + 1) { printf("&s1.m2 != &s1.m1 + 1! &s1.m1: %p, &s1.m2: %p\n", &s1.m1, &s1.m2); exit(-1); }
  if (&s1.m5 != &s1.m2 + sizeof (int)) { printf("&s1.m5 != &s1.m2 + sizeof (int)! &s1.m2: %p, &s1.m5: %p\n", &s1.m2, &s1.m5); exit(-1); }

  p1 = &s1;

  if (p1->m0 != 4711) { printf("p1->m0 != 4711! p1->m0: %d\n", p1->m0); exit(-1); }
  if (*p1->m1 != 4711) { printf("*p1->m1 != 4711! *p1->m0: %d\n", *p1->m1); exit(-1); }
  if (p1->m2 != 'x') { printf("p1->m2 != 'x'! p1->m2: '%c'\n", p1->m2); exit(-1); }
  if (*p1->m5 != 'x') { printf("*p1->m5 != 'x'! *p1->m5: '%c'\n", *p1->m5); exit(-1); }

  if (p1 != &s1) { printf("p1 != &s1! &s1: %p, p1: %p\n", &s1, p1); exit(-1); }

  if (&p1->m0 != &s1.m0) { printf("&p1->m0 != &s1.m0! &p1->m0: %p, &s1.m0: %p\n", &p1->m0, &s1.m0); exit(-1); }
  if (&p1->m1 != &s1.m1) { printf("&p1->m1 != &s1.m1! &p1->m1: %p, &s1.m1: %p\n", &p1->m1, &s1.m1); exit(-1); }
  if (&p1->m2 != &s1.m2) { printf("&p1->m2 != &s1.m2! &p1->m2: %p, &s1.m2: %p\n", &p1->m2, &s1.m2); exit(-1); }
  if (&p1->m5 != &s1.m5) { printf("&p1->m5 != &s1.m5! &p1->m5: %p, &s1.m5: %p\n", &p1->m5, &s1.m5); exit(-1); }

  if (sizeof (struct S2) != 36) { printf("sizeof (struct S2) != 36! sizeof (struct S2): %d\n", sizeof (struct S2)); exit(-1); }

  s2.m0 = 4711;
  s2.m1 = &s2.m0;
  s2.m2 = 'x';
  s2.m5 = &s2.m2;
  s2.m6.m0 = 4711;
  s2.m6.m1 = &s2.m0;
  s2.m6.m2 = 'x';
  s2.m6.m5 = &s2.m2;
  s2.m7 = &s2.m6;

  if (&s2.m0 != &s2) { printf("&s2.m0 != &s2! &s2: %p, &s2.m0: %p\n", &s2, &s2.m0); exit(-1); }
  if (&s2.m1 != &s2.m0 + 1) { printf("&s2.m1 != &s2.m0 + 1! &s2.m0: %p, &s2.m1: %p\n", &s2.m0, &s2.m1); exit(-1); }
  if (&s2.m2 != &s2.m1 + 1) { printf("&s2.m2 != &s2.m1 + 1! &s2.m1: %p, &s2.m2: %p\n", &s2.m1, &s2.m2); exit(-1); }
  if (&s2.m5 != &s2.m2 + sizeof (int)) { printf("&s2.m5 != &s2.m2 + sizeof (int)! &s2.m2: %p, &s2.m5: %p\n", &s2.m2, &s2.m5); exit(-1); }

  return 0;
}
