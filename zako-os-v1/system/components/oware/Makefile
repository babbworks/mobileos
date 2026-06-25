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
LIB_UI := liboware-ui.a
ENGINE_TEST := test_oware_engine
AI_TEST := test_oware_ai
STORE_TEST := test_oware_store
UI_TEST := test_oware_ui
APP := oware

.PHONY: all test clean

all: $(LIB) $(LIB_AI) $(LIB_STORE) $(LIB_UI) $(APP)

$(LIB): oware_engine.o
	$(AR) rcs $@ $^

$(LIB_AI): oware_ai.o
	$(AR) rcs $@ $^

$(LIB_STORE): oware_store.o
	$(AR) rcs $@ $^

$(LIB_UI): oware_ui.o
	$(AR) rcs $@ $^

oware_ui.o: oware_ui.c oware_ui.h oware_engine.h oware_ai.h oware_store.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(APP): main.c oware_ui.c oware_store.c oware_ai.c oware_engine.c \
        oware_ui.h oware_store.h oware_ai.h oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ main.c oware_ui.c oware_store.c oware_ai.c oware_engine.c

oware_engine.o: oware_engine.c oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

oware_ai.o: oware_ai.c oware_ai.h oware_engine.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

oware_store.o: oware_store.c oware_store.h oware_engine.h oware_ai.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(ENGINE_TEST) $(AI_TEST) $(STORE_TEST) $(UI_TEST)
	./$(ENGINE_TEST)
	./$(AI_TEST)
	./$(STORE_TEST)
	./$(UI_TEST)

$(ENGINE_TEST): test_oware_engine.c oware_engine.c oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_engine.c oware_engine.c

$(AI_TEST): test_oware_ai.c oware_ai.c oware_engine.c oware_ai.h oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_ai.c oware_ai.c oware_engine.c

$(STORE_TEST): test_oware_store.c oware_store.c oware_engine.c oware_ai.c oware_store.h oware_engine.h oware_ai.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_store.c oware_store.c oware_engine.c oware_ai.c

$(UI_TEST): test_oware_ui.c oware_ui.c oware_store.c oware_ai.c oware_engine.c \
            oware_ui.h oware_store.h oware_ai.h oware_engine.h oware_test.h
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) -o $@ test_oware_ui.c oware_ui.c oware_store.c oware_ai.c oware_engine.c

clean:
	rm -f *.o $(LIB) $(LIB_AI) $(LIB_STORE) $(LIB_UI) \
	      $(ENGINE_TEST) $(AI_TEST) $(STORE_TEST) $(UI_TEST) $(APP)
