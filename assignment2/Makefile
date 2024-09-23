C_SOURCES = $(wildcard *.c)
OBJECTS = $(C_SOURCES:.c=.o)
HEADERS = $(wildcard *.h)
COMPONENTS = $(C_SOURCES:.c=)

CC = gcc
CFLAGS_DEBUG = -Wextra -Werror -g -DDEBUG -MMD -MP
CFLAGS = -Wall -pthread

.SUFFIXES:

ifdef DEBUG
CFLAGS += $(CFLAGS_DEBUG)
endif

.PHONY: all clean

all: $(COMPONENTS)

%: %.o $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(OBJECTS:.o=.d)

.PRECIOUS: $(OBJECTS)

clean:
	rm -rf $(COMPONENTS) $(OBJECTS) *.d