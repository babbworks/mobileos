# oware engine + ai + tests
CC ?= cc
AR ?= ar
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -fstack-protector-strong
CFLAGS_DEBUG := -std=c99 -Wall -Wextra -Wpedantic -Werror -g -O0 \
                -fsanitize=address,undefined
INCLUDES := -I.

LIB := liboware-engine.a
LIB_AI := liboware-ai.a
LIB_STORE := liboware-store.a
ENGINE_TEST := test_oware_engine
AI_TEST := test_oware_ai
STORE_TEST := test_oware_store

.PHONY: all test clean

all: $(LIB) $(LIB_AI) $(LIB_STORE)

$(LIB): oware_engine.o
	$(AR) rcs $@ $^

$(LIB_AI): oware_ai.o
	$(AR) rcs $@ $^

$(LIB_STORE): oware_store.o
	$(AR) rcs $@ $^

oware_engine.o: oware_engine.c oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

oware_ai.o: oware_ai.c oware_ai.h oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

oware_store.o: oware_store.c oware_store.h oware_engine.h oware_ai.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(ENGINE_TEST) $(AI_TEST) $(STORE_TEST)
	./$(ENGINE_TEST)
	./$(AI_TEST)
	./$(STORE_TEST)

$(ENGINE_TEST): test_oware_engine.c oware_engine.c oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_engine.c oware_engine.c

$(AI_TEST): test_oware_ai.c oware_ai.c oware_engine.c oware_ai.h oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_ai.c oware_ai.c oware_engine.c

$(STORE_TEST): test_oware_store.c oware_store.c oware_engine.c oware_ai.c oware_store.h oware_engine.h oware_ai.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_store.c oware_store.c oware_engine.c oware_ai.c

clean:
	rm -f *.o $(LIB) $(LIB_AI) $(LIB_STORE) $(ENGINE_TEST) $(AI_TEST) $(STORE_TEST)
