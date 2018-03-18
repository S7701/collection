EXT := .exe

ifeq ($(OS),Windows_NT)
GCCFLAGS := -w
else
GCCFLAGS := -w -m32
endif

all: cc ccd

cc: cc.cpp
	g++ $(GCCFLAGS) -O -o $@$(EXT) $<

ccd:cc.cpp
	g++ $(GCCFLAGS) -g -o $@$(EXT) $<
