CFLAGS += -lSDL -lSDL_image -lrt -O2
GAMEBIN += game

ifeq (run,$(firstword $(MAKECMDGOALS)))
  # use the rest as arguments for "run"
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # ...and turn them into do-nothing targets
  $(eval $(RUN_ARGS):;@:)
endif

all: game

game: game.c gpio.o
	$(CC) game.c $(CFLAGS) -o $(GAMEBIN) gpio.o

gpio.o: gpio.c
	$(CC) gpio.c $(CFLAGS) -c

lvlgen: lvl.c
	$(CC) lvl.c -o lvlgen

clean:
	rm -f game lvlgen

run: game
	./game ${RUN_ARGS}

.PHONY: clean run ${RUN_ARGS}
