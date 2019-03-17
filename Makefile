CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Wshadow `pkg-config --cflags icu-uc icu-io sqlite3`
LDFLAGS = `pkg-config --libs icu-uc icu-io sqlite3`

all: utofu
