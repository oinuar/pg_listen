CFLAGS = -std=c99 -Wpedantic -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2 `pkg-config --cflags libpq`
LDLIBS = `pkg-config --libs libpq`

default: pg_listen

clean:
	rm pg_listen
