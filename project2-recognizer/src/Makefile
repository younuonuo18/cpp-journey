CC ?= gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic
all: recognizer_starter
recognizer_starter: recognizer_starter.c
	$(CC) $(CFLAGS) $< -lm -o $@
clean:
	rm -f recognizer_starter
