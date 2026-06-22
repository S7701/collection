all: c4 c4-test c8 c8-test

c4: c4.c
	gcc -Wall -m32 -Og -o c4 c4.c

c4-test: c4 FORCE
	-./test.sh c4

c5: c5.c
	gcc -Wall -Wno-int-conversion -Wno-parentheses -m32 -Og -o c5 c5.c

c5-test: c5 FORCE
	-./test.sh c5

c8: c8.c
	gcc -Wall -m32 -Og -o c8 c8.c

c8-test: c8 FORCE
	-./test.sh c8

c9: c9.c
	gcc -Wall -m32 -Og -o c9 c9.c

c9-test: c9 FORCE
	-./test.sh c9

cx: cx.c
	gcc -Wall -m32 -Og -o cx cx.c

cx-test: cx FORCE
	-./test.sh cx

FORCE: