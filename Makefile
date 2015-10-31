LFLAGS += -lSDL -lSDL_image -lrt
UFLAGS += -O2
GAMEBIN = game

all: game lvlgen

game: game.c
	$(CC) game.c $(LFLAGS) -o $(GAMEBIN) $(LFLAGS)

lvlgen: lvl.c
	$(CC) lvl.c -o lvlgen

clean:
	rm -f game lvlgen

run: game
	./game

.PHONY: clean run
