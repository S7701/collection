EXT := .exe

ifeq ($(OS),Windows_NT)
GCCFLAGS := -w
else
GCCFLAGS := -w -m32
endif

all: c4 c4d c4x86 c4x86d

c4: c4.c
	gcc $(GCCFLAGS) -O -o c4$(EXT) $<

c4d: c4.c
	gcc $(GCCFLAGS) -g -o c4d$(EXT) $<

c4x86: c4x86.c
	gcc $(GCCFLAGS) -O -o c4x86$(EXT) $<

c4x86d: c4x86.c
	gcc $(GCCFLAGS) -g -o c4x86d$(EXT) $<
