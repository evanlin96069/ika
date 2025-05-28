.PHONY: all prep release debug clean format

# Compiler flags
CC ?= gcc
CFLAGS = -pedantic -std=c11 -Wall -Wextra
CFLAGS += -include src/common.h

# Project files
SRCDIR = src
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, %.o, $(SOURCES))
DEPS = $(patsubst $(SRCDIR)/%.c, %.d, $(SOURCES))
EXE = ikac

# Release build settings
RELDIR = release
RELEXE = $(RELDIR)/$(EXE)
RELOBJS = $(addprefix $(RELDIR)/, $(OBJS))
RELDEPS = $(addprefix $(RELDIR)/, $(DEPS))
RELCFLAGS = -O2 -DNDEBUG

# Debug build settings
DBGDIR = debug
DBGEXE = $(DBGDIR)/$(EXE)
DBGOBJS = $(addprefix $(DBGDIR)/, $(OBJS))
DBGDEPS = $(addprefix $(DBGDIR)/, $(DEPS))
DBGCFLAGS = -Og -g3 -D_DEBUG

# Default target
all: prep release

# Release build
release: $(RELEXE)
$(RELEXE): $(RELOBJS)
	$(CC) $(CFLAGS) $(RELCFLAGS) -s -o $(RELEXE) $^
$(RELDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -MMD $(CFLAGS) $(RELCFLAGS) -o $@ $<

# Debug build
debug: $(DBGEXE)
$(DBGEXE): $(DBGOBJS)
	$(CC) $(CFLAGS) $(DBGCFLAGS) -o $(DBGEXE) $^
$(DBGDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -MMD $(CFLAGS) $(DBGCFLAGS) -o $@ $<

-include $(RELDEPS) $(DBGDEPS)

# Prepare
prep:
	@mkdir -p $(RELDIR) $(DBGDIR)

# Clean target
clean:
	rm -f $(RELEXE) $(RELDEPS) $(RELOBJS) $(DBGEXE) $(DBGDEPS) $(DBGOBJS)

# Format all files
format:
	clang-format -i $(SRCDIR)/*.h $(SRCDIR)/*.c
