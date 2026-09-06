CC := clang
CFLAGS := -Wall -Wextra -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=12.0
CPPFLAGS := -Isrc
FRAMEWORKS := -framework IOKit -framework CoreFoundation
TARGET := keyboard-logo-fix

SOURCES := $(wildcard src/*.c)

.PHONY: all clean app

all: $(TARGET)

$(TARGET): $(SOURCES) $(wildcard src/*.h)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(FRAMEWORKS) $(SOURCES) -o $@

app: $(TARGET)
	./build-app.sh

clean:
	rm -rf $(TARGET) dist
