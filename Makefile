CFLAGS += -lSDL -lSDL_image -O2
GAMEBIN = game

game: test.c
	$(CC) $(CFLAGS) test.c -o $(GAMEBIN)

clean:
	rm -f game

run: game
	./game

.PHONY: clean run
