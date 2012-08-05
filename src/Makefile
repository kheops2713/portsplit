IDIR = ../include
CC = gcc
CPPFLAGS=-D_POSIX_C_SOURCE=200112L
CFLAGS=-I$(IDIR) \
	-ansi -pedantic -O2 -Wchar-subscripts -Wcomment -Wformat=2 -Wimplicit-int \
	-Werror-implicit-function-declaration -Wmain -Wparentheses \
	-Wsequence-point -Wreturn-type -Wswitch -Wtrigraphs -Wunused \
	-Wuninitialized -Wunknown-pragmas -Wfloat-equal -Wundef \
	-Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings \
	-Wconversion -Wsign-compare -Waggregate-return -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn \
	-Wformat -Wmissing-format-attribute \
	-Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long \
	-Wunreachable-code -Wno-missing-braces -Wno-missing-field-initializers \

_DEPS = child.h config_parse.h log.h main.h sigs.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

OBJ = child.o config_parse.o log.o main.o sigs.o

LIBS = -lutil

TARGET = portsplit

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

.PHONY: clean

clean:
	rm -f $(TARGET) $(OBJ) *~ $(IDIR)/*~
