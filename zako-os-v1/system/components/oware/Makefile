# oware engine + ai + tests
CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -fstack-protector-strong
CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror -g -O0 \
                -fsanitize=address,undefined
INCLUDES := -I.

LIB := liboware-engine.a
LIB_AI := liboware-ai.a
ENGINE_TEST := test_oware_engine
AI_TEST := test_oware_ai

.PHONY: all test clean

all: $(LIB) $(LIB_AI)

$(LIB): oware_engine.o
	$(AR) rcs $@ $^

$(LIB_AI): oware_ai.o
	$(AR) rcs $@ $^

oware_engine.o: oware_engine.c oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

oware_ai.o: oware_ai.c oware_ai.h oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(ENGINE_TEST) $(AI_TEST)
	./$(ENGINE_TEST)
	./$(AI_TEST)

$(ENGINE_TEST): test_oware_engine.c oware_engine.c oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_engine.c oware_engine.c

$(AI_TEST): test_oware_ai.c oware_ai.c oware_engine.c oware_ai.h oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_ai.c oware_ai.c oware_engine.c

clean:
	rm -f *.o $(LIB) $(LIB_AI) $(ENGINE_TEST) $(AI_TEST)
