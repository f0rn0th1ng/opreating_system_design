# Makefile for webserver

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS =
OBJFILES = webserver.o
TARGET = webserver

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LDFLAGS)

webserver.o: webserver.c
	$(CC) $(CFLAGS) -c webserver.c

clean:
	rm -f $(OBJFILES) $(TARGET) *~

.PHONY: all clean

