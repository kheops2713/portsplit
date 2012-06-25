IDIR = ../include
CC = gcc
CFLAGS=-I$(IDIR) \
	-ansi -O2 -Wchar-subscripts -Wcomment -Wformat=2 -Wimplicit-int \
	-Werror-implicit-function-declaration -Wmain -Wparentheses \
	-Wsequence-point -Wreturn-type -Wswitch -Wtrigraphs -Wunused \
	-Wuninitialized -Wunknown-pragmas -Wfloat-equal -Wundef \
	-Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings \
	-Wconversion -Wsign-compare -Waggregate-return -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn \
	-Wformat -Wmissing-format-attribute -Wno-deprecated-declarations \
	-Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long \
	-Wunreachable-code

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
