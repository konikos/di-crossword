SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)
TARGET = bin/cw

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


include $(DEP)
CLEAN += $(DEP)


$(TARGET): $(OBJ)
	$(CC) -o$@ $^

CLEAN += $(TARGET) $(OBJ)


clean:
	-rm -f $(CLEAN)

PHONY += clean


.PHONY: $(PHONY)
