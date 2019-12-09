struct S {
  char c;
  int i;
  char *cp;
  int *ip;
} s1;
int x1;

int main(int argc, char **argv)
{
  int x2;
  struct S s2;

  x1 = 1;
  s1.c = '1';
  s1.i = 1001;
  s1.cp = "hello s1!";
  s1.ip = &x1;
  x2 = 2;
  s2.c = '*';
  s2.i = 4711;
  s2.cp = "hello world!";
  s2.ip = &x2;
  printf("s1.c=%c, s1.i=%d, s1.cp='%s', s1.ip=%d\n", s1.c, s1.i, s1.cp, *s1.ip);
  printf("s2.c=%c, s2.i=%d, s2.cp='%s', s2.ip=%d\n", s2.c, s2.i, s2.cp, *s2.ip);
  return 0;
}