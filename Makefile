TEST_SRC = $(wildcard src/*_test.c) $(wildcard src/cwt_*.c)
SRC = $(filter-out $(TEST_SRC),$(wildcard src/*.c))
OBJ = $(SRC:.c=.o)
TEST_OBJ = $(TEST_SRC:.c=.o)
DEP = $(SRC:.c=.d)
TEST_DEP = $(TEST_SRC:.c=.d)
TARGET = bin/cw
TEST_TARGET = bin/cw_test
MAIN_OBJ = src/main.o

CFLAGS += \
	  -std=gnu99 \
	  -Wall -Wextra \
	  -Wrestrict -Wnull-dereference  -Wjump-misses-init \
	  -Wdouble-promotion -Wshadow -Wconversion -Wsign-conversion \
	  -Wswitch-enum -Wswitch-default -Wundef -Wmissing-field-initializers


all: $(TARGET)
PHONY += all


%.d: %.c
	$(CC) -MM $(CPPFLAGS) -MT$(<:.c=.o) $< | \
		sed 's|\($*\)\.o[ :]*|\1.o $@ : |g' >$@


include $(DEP) $(TEST_DEP)
CLEAN += $(DEP)


$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o$@ $^

CLEAN += $(TARGET) $(OBJ)


$(TEST_TARGET): $(filter-out $(MAIN_OBJ),$(OBJ)) $(TEST_OBJ)
	$(CC) -o$@ $^

CLEAN += $(TEST_TARGET) $(OBJ) $(TEST_OBJ)


test: $(TEST_TARGET)
	$(TEST_TARGET)

PHONY += test


clean:
	-rm -f $(CLEAN)


build-watch:
	-$(MAKE) $(TARGET) $(TEST_TARGET)
	while fswatch -1 Makefile src/; do \
		clear; \
		$(MAKE) $(TARGET) $(TEST_TARGET) \
			&& $(TEST_TARGET); \
		printf "Finished with exit code $$?."; \
	done


PHONY += clean


.PHONY: $(PHONY)
