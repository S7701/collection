CFLAGS = -Wall -Wshadow -O3 -g -march=native
LDLIBS = -lm

all: nn

nn: nn.o


clean:
	$(RM) *.o
	$(RM) test example1 example2 example3 example4 *.exe
	$(RM) persist.txt

.PHONY: clean
