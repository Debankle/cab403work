C_SOURCES = $(wildcard *.c)
OBJECTS = $(C_SOURCES:.c=.o)
HEADERS = $(wildcard *.h)
COMPONENTS = $(C_SOURCES:.c=)

CC = gcc
CFLAGS_DEBUG = -Wextra -Werror -g -DDEBUG -MMD -MP
CFLAGS = -Wall
LDFLAGS = -pthread

ifdef DEBUG
CFLAGS += $(CFLAGS_DEBUG)
.SUFFIXES:
.PRECIOUS: $(OBJECTS)
endif

.PHONY: all clean

all: $(COMPONENTS)

%: %.o $(HEADERS)
	$(CC) $(LDFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(OBJECTS:.o=.d)

clean:
	rm -rf $(COMPONENTS)

fclean: clean
	rm -rf $(OBJECTS) *.d