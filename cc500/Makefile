EXT := .exe
GPPFLAGS := -w -O

all: release debug

release: cc500.c
	gcc $(GPPFLAGS) -o cc500$(EXT) $<

debug: cc500.c
	gcc $(GPPFLAGS) -g -o cc500d$(EXT) $<
