CC = gcc
CFLAGS = -O2 -fPIC -Wall -Wextra -g
LDFLAGS = -shared -ldl -pthread
TARGET = memprof.so
SRC = memprof.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)
