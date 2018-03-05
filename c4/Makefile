EXT := .exe
GPPFLAGS := -w -O

all: release debug

release: c4.c
	gcc $(GPPFLAGS) -o c4$(EXT) $<

debug: c4.c
	gcc $(GPPFLAGS) -g -o c4d$(EXT) $<
