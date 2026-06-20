# oware engine + tests
CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -fstack-protector-strong
CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror -g -O0 \
                -fsanitize=address,undefined
INCLUDES := -I.

LIB := liboware-engine.a
ENGINE_TEST := test_oware_engine

.PHONY: all test clean

all: $(LIB)

$(LIB): oware_engine.o
	$(AR) rcs $@ $^

oware_engine.o: oware_engine.c oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(ENGINE_TEST)
	./$(ENGINE_TEST)

$(ENGINE_TEST): test_oware_engine.c oware_engine.c oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_engine.c oware_engine.c

clean:
	rm -f *.o $(LIB) $(ENGINE_TEST)
