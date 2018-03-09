CC = clang
CFLAGS = -Wall -Wextra $(shell pkg-config --cflags libpq)
LDFLAGS = $(shell pkg-config --libs libpq)

default: pg_listen

pg_listen: pg_listen.o
	$(CC) $(LDFLAGS) -o $@ pg_listen.o

pg_listen.o: pg_listen.c
	$(CC) $(CFLAGS) -c pg_listen.c

clean:
	rm pg_listen pg_listen.o
