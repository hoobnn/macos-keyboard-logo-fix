CC := clang
CFLAGS := -Wall -Wextra -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=12.0
FRAMEWORKS := -framework IOKit -framework CoreFoundation
TARGET := t100-logo

.PHONY: all clean app

all: $(TARGET)

$(TARGET): t100_logo.c
	$(CC) $(CFLAGS) $(FRAMEWORKS) $< -o $@

app: $(TARGET)
	./build-app.sh

clean:
	rm -rf $(TARGET) dist
