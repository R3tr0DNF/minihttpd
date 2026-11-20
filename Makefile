CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic
CPPFLAGS = -Iinclude

SRC = $(wildcard src/*.c)
TARGET = minihttpd

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRC) -o $(TARGET)