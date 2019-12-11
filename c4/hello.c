int x1;

int f1() {
  return 4711;
}

int main(int argc, char **argv)
{
  int x2;

  x1 = 1;
  x2 = 2;
  printf("x1=%d x2=%d\n", x1, x2);
  return f1();
}