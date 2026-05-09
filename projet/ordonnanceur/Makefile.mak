CC=gcc

all:
	$(CC) src/main.c src/parser.c policies/*.c -o ordonnanceur

run:
	./ordonnanceur