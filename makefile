
CC = gcc
AR = ar

LIBNAME = ejson
LIBFILE = lib$(LIBNAME).a

CFLAGS = -Wall -Wextra -g

SRCDIR = src
OBJDIR = obj
OUTDIR = lib
INCDIR = inc
EXDIR  = ex

HFILES = $(wildcard $(SRCDIR)/*.h)
CFILES = $(wildcard $(SRCDIR)/*.c)
OFILES = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(CFILES))

EXT = .exe

all: $(OUTDIR)/$(LIBFILE) $(OUTDIR)/ex0$(EXT) $(OUTDIR)/ex1$(EXT)

$(OUTDIR) $(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HFILES) $(OBJDIR)
	$(CC) -c -o $@ $< $(CFLAGS) -I$(INCDIR)

$(OUTDIR)/$(LIBFILE): $(OFILES) $(OUTDIR)
	$(AR) rcs $@ $(OFILES)

$(OUTDIR)/%$(EXT): ex/%.c $(OUTDIR)/$(LIBFILE) $(HFILES)
	$(CC) -o $@ $< $(CFLAGS) -l$(LIBNAME) -I$(INCDIR) -L$(OUTDIR)

clean:
	rm -f $(OBJDIR)/*.o $(OUTDIR)/*.a $(OUTDIR)/*.exe
