BASIC_CFLAGS = -Wall -O3 -fPIC -g -std=c99 -pedantic

ALL_CFLAGS = $(BASIC_CFLAGS) $(CFLAGS)

PREFIX = /usr/local
RM = rm -f
INSTALL = install -p
INSTALL_EXEC = $(INSTALL) -m 0755
INSTALL_DATA = $(INSTALL) -m 0644
LUA_VERSION = 5.2
MODULE_NAME = ssocket

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

ifeq ($(uname_S), Darwin)
	SHARELIB_FLAGS = -dynamiclib -Wl,-undefined,dynamic_lookup
else
	SHARELIB_FLAGS = --shared
endif

a