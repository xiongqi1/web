CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
STRIP := echo

CFLAGS := -g -c -Wall -Werror -DUNIT_TEST
ifdef CDCS_INCLUDE_OPTS
	CFLAGS += $(CDCS_INCLUDE_OPTS)
else
	CFLAGS += -I$(CDCS_INCLUDE)
endif

LDFLAGS := $(CDCS_LIB)/unit_test.a

TEST_PROJECT = store_bot_test
TEST_SOURCES = store_bot_test.c ../store_bot.c
TEST_OBJECTS = $(patsubst %.c,%.o,$(TEST_SOURCES))

DEPS = ../store_bot.h

.PHONY : all
all: $(TEST_PROJECT)

.PHONY : clean distclean
clean distclean:
	find -type f -name "*.[ch]" | xargs chmod 644 Makefile* $(TEST_PROJECT).*; \
	rm -f $(TEST_PROJECT) $(TEST_PROJECT).a $(TEST_OBJECTS); \
	rm -f *~ ~*;

$(TEST_OBJECTS) : $(DEPS)

$(TEST_PROJECT): $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(LDFLAGS) -o $@
