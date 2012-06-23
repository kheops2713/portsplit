IDIR = ../include
CC = gcc
CFLAGS=-I$(IDIR)

_DEPS = child.h config_parse.h log.h main.h sigs.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

OBJ = child.o config_parse.o log.o main.o sigs.o

LIBS = -lutil

TARGET = portsplit

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

.PHONY: clean

clean:
	rm -f $(TARGET) $(OBJ) *~ $(IDIR)/*~
