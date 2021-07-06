#include <stdio.h>

enum { Error, Success };
enum { E0 = 0, E1, E2, E4711 = 4711, En };

int i1;

int test() {
  return i1;
}

int main() {
  int *p1, *p2;

  printf("0: %s\n", 0 ? "FAILED" : "ok");
  printf("1: %s\n", 1 ? "ok" : "FAILED");
  printf("-1: %s\n", -1 ? "ok" : "FAILED");

  printf("0 == 0: %s\n", 0 == 0 ? "ok" : "FAILED");
  printf("1 == 1: %s\n", 1 == 1 ? "ok" : "FAILED");
  printf("-1 == -1: %s\n", -1 == -1 ? "ok" : "FAILED");

  printf("0 != 1: %s\n", 0 != 1 ? "ok" : "FAILED");
  printf("1 != 0: %s\n", 1 != 0 ? "ok" : "FAILED");
  printf("-1 != 0: %s\n", -1 != 0 ? "ok" : "FAILED");
  printf("-1 != 1: %s\n", -1 != 1 ? "ok" : "FAILED");

  printf("Error == 0: %s\n", Error == 0 ? "ok" : "FAILED");
  printf("Success == 1: %s\n", Success == 1 ? "ok" : "FAILED");
  printf("E0 == 0: %s\n", E0 == 0 ? "ok" : "FAILED");
  printf("E1 == 1: %s\n", E1 == 1 ? "ok" : "FAILED");
  printf("E2 == 2: %s\n", E2 == 2 ? "ok" : "FAILED");
  printf("E2 == 2: %s\n", E2 == 2 ? "ok" : "FAILED");
  printf("E4711 == 4711: %s\n", E4711 == 4711 ? "ok" : "FAILED");

  printf("&i1 %p\n", &i1);
  p1 = &i1; printf("&p1 %p, p1 %p\n", &p1, p1);
  p2 = &i1; printf("&p2 %p, p2 %p\n", &p2, p2);
  printf("p1 + 4 = %p\n", p1 + 4);
  printf("4 + p1 = %p\n", 4 + p1);
//  printf("p1 + p2 = %p\n", p1 + p2); // not allowed

  printf("\n");
  return test();
}