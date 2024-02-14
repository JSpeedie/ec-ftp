#Makefile
LIBS = -lpthread
CFLAGS = -Wall
# CC = gcc -g
CC = gcc

# For more information on 'patsubst' and the possibly-difficult-to-understand
# code going on in the array creation code below, please see the GNU Make
# manual "8.2 Functions for String Substitution and Analysis"
# (https://www.gnu.org/software/make/manual/html_node/Text-Functions.html)

# The name of the subdirectory in both the source directory /and/ the LZMA
# directory where all object files will be stored.
OBJDIR = obj
# The name of the subdirectory of the source directory that contains all the
# LZMA source files.
LZMADIR = lzma
_LZMASRC = Alloc.c CpuArch.c LzFind.c LzFindMt.c LzFindOpt.c LzmaDec.c \
	LzmaEnc.c LzmaLib.c Threads.c
# LZMA source files prefixed by the LZMA directory (E.g. = lzma/Alloc.c lzma/CpuArch.c ... )
# {{{
# Created by pattern substituting (for all elements in '_LZMASRC')
# "[wildcard]" to "$(LZMADIR)/[wildcard]".
# }}}
LZMASRC = $(patsubst %,$(LZMADIR)/%,$(_LZMASRC))
# LZMA-related objects files (E.g. = Alloc.o CpuArch.o ... )
# {{{
# Created by pattern substituting (for all elements in '_LZMASRC')
# "[wildcard].c" to "[wildcard].o".
# }}}
_LZMAOBJ = $(patsubst %.c,%.o,$(_LZMASRC))
# LZMA-related objects files (E.g. = lzma/obj/Alloc.o lzma/obj/CpuArch.o ... )
# {{{
# Created by pattern substituting (for all elements in 'LZMASRC')
# "[wildcard].c" to "$(LZMADIR)$(OBJDIR)[wildcard].o".
# }}}
LZMAOBJ = $(patsubst %.c,$(LZMADIR)/$(OBJDIR)/%.o,$(_LZMASRC))
# Dependency C files
DEPC = comp.c enc.c aes.c ec-ftp.c
# Dependency object files (E.g. = obj/comp.o obj/enc.o ... )
# {{{
# Created by pattern substituting (for all elements in 'DEPC')
# "[wildcard].c" to "$(OBJDIR)[wildcard].o".
# }}}
DEP = $(patsubst %.c,$(OBJDIR)/%.o,$(DEPC))

# .DEFAULT_GOAL := compile

# `compile` first because we want `make` to just compile the program, and the
# default target is always the the first one that doesn't begin with "."
.PHONY: compile
compile: lzmaobj lzma obj csshareddep ftpserver ftpclient

# Build an object file for each LZMA source file in the object file
# subdirectory of the LZMA directory (Here we don't specify a recipe and
# instead rely on our modified version of the implicit "%.o: %.c" rule to
# produce our LZMA object files using the corresponding C files.
.PHONY: lzma
lzma: $(LZMAOBJ)

# Build an object file for each source file in the object file subdirectory
# (Here we don't specify a recipe and instead rely on our modified version of
# the implicit "%.o: %.c" rule to produce our object files using the
# corresponding C files.
.PHONY: csshareddep
csshareddep: $(DEP)

# Link the various files to create the server executable
ftpserver: lzma $(OBJDIR)/ftpserver.o $(DEP) | bin
	$(CC) $(CFLAGS) $(OBJDIR)/ftpserver.o $(DEP) $(LZMAOBJ) $(LIBS) -o ftpserver
	mv ftpserver ../bin/ftpserver/

# Link the various files to create the client executable
ftpclient: lzma $(OBJDIR)/ftpclient.o $(DEP) | bin
	$(CC) $(CFLAGS) $(OBJDIR)/ftpclient.o $(DEP) $(LZMAOBJ) $(LIBS) -o ftpclient
	mv ftpclient ../bin/ftpclient/

.PHONY: obj
obj:
	mkdir -p $(OBJDIR)

.PHONY: lzmaobj
lzmaobj:
	mkdir -p $(LZMADIR)/$(OBJDIR)

.PHONY: bin
bin:
	mkdir -p ../bin
	mkdir -p ../bin/ftpclient/
	mkdir -p ../bin/ftpserver/

# rm all object files in the lzma directory, and the LZMA object file directory
.PHONY: cleanlzma
cleanlzma:
	rm -f $(LZMAOBJ)
	rm -d $(LZMADIR)/$(OBJDIR)

# rm all the object files in the source directory, and their object file directory
.PHONY: cleanobj
cleanobj:
	rm -f $(DEP)
	rm -f $(OBJDIR)/ftpserver.o $(OBJDIR)/ftpclient.o
	rm -d $(OBJDIR)

# rm the local client and server executables
.PHONY: cleanexec
cleanexec:
	rm -f ../bin/ftpclient/ftpclient ../bin/ftpserver/ftpserver

.PHONY: clean
clean: cleanlzma cleanobj cleanexec

# Override the implicit rule for generating an object file for a given C file
# for the C files in the LZMA directory so that the object files are generated
# not in the same directory, but in an object subdirectory
# {{{
# For more info on the strange variables used here, please see:
# (https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html)
# }}}
$(LZMADIR)/$(OBJDIR)/%.o: $(LZMADIR)/%.c
	$(CC) $(CFLAGS) $< -c -o $@

# Override the implicit rule for generating an object file for a given C file
# for the C files in the source directory so that the object files are generated
# not in the same directory, but in an object subdirectory
# {{{
# For more info on the strange variables used here, please see:
# (https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html)
# }}}
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
