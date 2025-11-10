all: c4 c4-test c8 c8-test

c4: c4.c
	gcc -Wall -m32 -Og -o c4 c4.c

c4-test: c4 FORCE
	-./test.sh c4

c8: c8.c
	gcc -Wall -m32 -Og -o c8 c8.c

c8-test: c8 FORCE
	-./test.sh c8

FORCE: