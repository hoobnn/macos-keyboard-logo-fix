CC := clang
CFLAGS := -Wall -Wextra -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=12.0
CPPFLAGS := -Isrc
FRAMEWORKS := -framework IOKit -framework CoreFoundation
TARGET := keyboard-logo-fix

SOURCES := $(wildcard src/*.c)
# Everything but the entry point, linked into the test runner instead.
LIBRARY_SOURCES := $(filter-out src/main.c,$(SOURCES))
TEST_SOURCES := $(wildcard tests/*.c)
TEST_TARGET := tests/run-tests

.PHONY: all clean app test

all: $(TARGET)

$(TARGET): $(SOURCES) $(wildcard src/*.h)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(FRAMEWORKS) $(SOURCES) -o $@

app: $(TARGET)
	./build-app.sh

# Host architecture only: tests run where they are built.
$(TEST_TARGET): $(LIBRARY_SOURCES) $(TEST_SOURCES) $(wildcard src/*.h)
	$(CC) -Wall -Wextra -O0 -g $(CPPFLAGS) $(FRAMEWORKS) \
		$(LIBRARY_SOURCES) $(TEST_SOURCES) -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(TARGET) $(TEST_TARGET) $(TEST_TARGET).dSYM dist
