CC := clang
CFLAGS := -Wall -Wextra -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=12.0
FRAMEWORKS := -framework IOKit -framework CoreFoundation
TARGET := keyboard-logo-fix

.PHONY: all clean app

all: $(TARGET)

$(TARGET): keyboard_logo_fix.c
	$(CC) $(CFLAGS) $(FRAMEWORKS) $< -o $@

app: $(TARGET)
	./build-app.sh

clean:
	rm -rf $(TARGET) dist
