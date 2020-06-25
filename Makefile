BASIC_CFLAGS = -Wall -O3 -fPIC -g -std=c99 -pedantic

ALL_CFLAGS = $(BASIC_CFLAGS) $(CFLAGS)

PREFIX = /usr/local
RM = rm -f
INSTALL = install -p
INSTALL_EXEC = $(INSTALL) -m 075