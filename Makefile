CC = gcc
CFLAGS = -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -g
TARGET = mycp
OBJECTS = main.o file_copy.o directory_copy.o metadata.o

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

main.o: main.c mycp.h
	$(CC) $(CFLAGS) -c main.c

file_copy.o: file_copy.c mycp.h
	$(CC) $(CFLAGS) -c file_copy.c

directory_copy.o: directory_copy.c mycp.h
	$(CC) $(CFLAGS) -c directory_copy.c

metadata.o: metadata.c mycp.h
	$(CC) $(CFLAGS) -c metadata.c

test: $(TARGET)
	bash tests/test_copy.sh

clean:
	rm -f $(OBJECTS) $(TARGET)
