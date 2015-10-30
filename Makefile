CFLAGS += -lSDL -lSDL_image -lrt -O2
GAMEBIN += game

all: game lvlgen

game: game.c
	$(CC) $(CFLAGS) game.c -o $(GAMEBIN)

lvlgen: lvl.c
	$(CC) lvl.c -o lvlgen

clean:
	rm -f game lvlgen

run: game
	./game

.PHONY: clean run
