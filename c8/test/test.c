#include <stdio.h>

enum { Error, Success };
enum { E0 = 0, E1, E2, E4711 = 4711, Ex = 2, En };

struct s2_s;

int i1;
// int i1; // not allowed

struct s1_s {
  int im;
  char cm;
//  struct s1_s s1m; // not allowed
  struct s2_s *ps2m;
} s1;

struct s2_s {
  int im;
  char cm;
//  struct s1_s s1m; // not allowed
  struct s1_s *ps1m;
} s2;

int i2;

int test() {
  return i1;
}


int main() {
  int *p1, *p2;

//  if (p1 = &i) printf("ERROR\n"); // not allowed
//  9 = 0; // crash
//  10 = 0; // crash

  printf("0 == false: %s\n", 0 ? "FAILED" : "ok");
  printf("1 == true: %s\n", 1 ? "ok" : "FAILED");
  printf("-1 == true: %s\n", -1 ? "ok" : "FAILED");
  printf("\n");

  printf("0 == 0: %s\n", 0 == 0 ? "ok" : "FAILED");
  printf("1 == 1: %s\n", 1 == 1 ? "ok" : "FAILED");
  printf("-1 == -1: %s\n", -1 == -1 ? "ok" : "FAILED");
  printf("\n");

  printf("0 != 1: %s\n", 0 != 1 ? "ok" : "FAILED");
  printf("1 != 0: %s\n", 1 != 0 ? "ok" : "FAILED");
  printf("-1 != 0: %s\n", -1 != 0 ? "ok" : "FAILED");
  printf("-1 != 1: %s\n", -1 != 1 ? "ok" : "FAILED");
  printf("\n");

  printf("enum Error == 0: %s\n", Error == 0 ? "ok" : "FAILED");
  printf("enum Success == 1: %s\n", Success == 1 ? "ok" : "FAILED");
  printf("enum E0 == 0: %s\n", E0 == 0 ? "ok" : "FAILED");
  printf("enum E1 == 1: %s\n", E1 == 1 ? "ok" : "FAILED");
  printf("enum E2 == 2: %s\n", E2 == 2 ? "ok" : "FAILED");
  printf("enum E2 == 2: %s\n", E2 == 2 ? "ok" : "FAILED");
  printf("enum E4711 == 4711: %s\n", E4711 == 4711 ? "ok" : "FAILED");
  printf("enum Ex == E2: %s\n", Ex == E2 ? "ok" : "FAILED");
  printf("enum Ex == 2: %s\n", Ex == 2 ? "ok" : "FAILED");
  printf("enum En == 3: %s\n", En == 3 ? "ok" : "FAILED");
  printf("\n");

  printf("&i1 = %p\n", &i1);
  printf("&i2 = %p\n", &i2);
  p1 = &i1;
  printf("p1 = %p\n", p1);
  p2 = &i1;
  printf("p2 = %p\n", p2);
  printf("p1 == &i1: %s\n", p1 == &i1 ? "ok" : "FAILED");
  printf("p2 == &i1: %s\n", p2 == &i1 ? "ok" : "FAILED");
  printf("p1 == p2: %s\n", p1 == p2 ? "ok" : "FAILED");
  printf("p1 + 4 = %p\n", p1 + 4);
  printf("4 + p1 = %p\n", 4 + p1);
  printf("p1 + p2 = %p FAILED: NOT ALLOWD\n", p1 + p2);
  printf("p1 - 4 = %p\n", p1 - 4);
  printf("4 - p1 = %p FAILED: NOT ALLOWD\n", 4 - p1);
  printf("p1 - p2 = %d\n", p1 - p2);
  printf("i1 = %d\n", i1);
  printf("*p1 = %d\n", *p1);
  printf("*p2 = %d\n", *p2);
  i1 = 4711; printf("i1 == 4711: %s\n", i1 == 4711 ? "ok" : "FAILED");
  printf("*p1 == 4711: %s\n", *p1 == 4711 ? "ok" : "FAILED");
  printf("*p2 == 4711: %s\n", *p2 == 4711 ? "ok" : "FAILED");
  printf("\n");

  printf("&s1 = %p\n", &s1);
  printf("&s1.im = %p\n", &s1.im);
  printf("&s1.cm = %p\n", &s1.cm);
  printf("&s2 = %p\n", &s2);
  printf("\n");

//  printf("sizeof x\n", sizeof x);
  printf("\n");

  test();
  return -1;
}