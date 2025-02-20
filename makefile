CC = gcc
CFLAGS = -g -Wall
TARGET = assignment2

all: $(TARGET)

$(TARGET): assignment2.c
	$(CC) $(CFLAGS) -o $(TARGET) assignment2.c

clean:
	rm -f $(TARGET)