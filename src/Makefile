#Makefile
LIBS = -lpthread
CFLAGS = -Wall
# CC = gcc -g
CC = gcc

# For more information on 'patsubst' and the possibly-difficult-to-understand
# code going on in the array creation code below, please see the GNU Make
# manual "8.2 Functions for String Substitution and Analysis"
# (https://www.gnu.org/software/make/manual/html_node/Text-Functions.html)

# The name of the subdirectory in the current directory that contains all the
# LZMA source files, and where the LZMA object files will be stored.
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
# LZMA-related objects files (E.g. = lzma/Alloc.o lzma/CpuArch.o ... )
# {{{
# Created by pattern substituting (for all elements in 'LZMASRC')
# "[wildcard].c" to "[wildcard].o".
# }}}
LZMAOBJ = $(patsubst %.c,%.o,$(LZMASRC))
DEP = comp.o enc.o aes.o ec-ftp.o

# .DEFAULT_GOAL := compile

# `compile` first because we want `make` to just compile the program, and the
# default target is always the the first one that doesn't begin with "."
compile: lzma csshareddep ftpserver ftpclient

# Build an object file for each LZMA source file in the LZMA directory
# (the default behaviour of Make will be to produce an .o file using
# the .c file of the same name, so we leave this recipe empty so as
# to make use of that implicit rule)
lzma: $(LZMAOBJ)

# Build an object file for each client/server shared dependency
# (the default behaviour of Make will be to produce an .o file using
# the .c file of the same name, so we leave this recipe empty so as
# to make use of that implicit rule)
csshareddep: $(DEP)

# Build the object file (-c flag stops gcc from linking)
ftpserver.o: ftpserver.c
	$(CC) $(CFLAGS) ftpserver.c -c

# Link the various files to create the server executable
ftpserver: lzma ftpserver.o $(DEP) | bin
	$(CC) $(CFLAGS) ftpserver.o $(DEP) $(LZMAOBJ) $(LIBS) -o ftpserver
	mv ftpserver ../bin/ftpserver/

# Build the object file (-c flag stops gcc from linking)
ftpclient.o: ftpclient.c
	$(CC) $(CFLAGS) ftpclient.c -c

# Link the various files to create the client executable
ftpclient: lzma ftpclient.o $(DEP) | bin
	$(CC) $(CFLAGS) ftpclient.c $(DEP) $(LZMAOBJ) $(LIBS) -o ftpclient
	mv ftpclient ../bin/ftpclient/

bin:
	mkdir -p ../bin
	mkdir -p ../bin/ftpclient/
	mkdir -p ../bin/ftpserver/

# rm all object files in the lzma directory
cleanlzma:
	cd $(LZMADIR) && \
	rm $(_LZMAOBJ)

# rm all the object files in the source directory
cleanobj:
	rm *.o

# rm the local client and server executables
cleanexec:
	rm ../bin/ftpclient/ftpclient ../bin/ftpserver/ftpserver

clean: cleanlzma cleanobj cleanexec
