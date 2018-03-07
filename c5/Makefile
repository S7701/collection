EXT := .exe

ifeq ($(OS),Windows_NT)
GCCFLAGS := -w
else
GCCFLAGS := -w -m32
endif

all: c5 c5d c5x86 c5x86d

c5: c5.c
	gcc $(GCCFLAGS) -O -o c5$(EXT) $<

c5d: c5.c
	gcc $(GCCFLAGS) -g -o c5d$(EXT) $<

c5x86: c5x86.c
	gcc $(GCCFLAGS) -O -o c5x86$(EXT) $<

c5x86d: c5x86.c
	gcc $(GCCFLAGS) -g -o c5x86d$(EXT) $<
