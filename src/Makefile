#Makefile
LIBS = -lpthread
CFLAGS = -Wall
# CC = gcc -g
CC = gcc

COMPRESSDEP = lzma/Alloc.c lzma/CpuArch.c lzma/LzFind.c lzma/LzFindMt.c \
	lzma/LzFindOpt.c lzma/LzmaDec.c lzma/LzmaEnc.c lzma/LzmaLib.c \
	lzma/Threads.c comp.c
ENCRYPTDEP = enc.c aes.c
PARDEP = ftputil.c

make: ftpclient.c ftpserver.c $(COMPRESSDEP) $(ENCRYPTDEP) $(PARDEP) | bin
	$(CC) $(CFLAGS) $(LIBS) ftpclient.c $(COMPRESSDEP) $(ENCRYPTDEP) $(PARDEP) -o ftpclient
	mv ftpclient ../bin/ftpclient/
	$(CC) $(CFLAGS) $(LIBS) ftpserver.c $(COMPRESSDEP) $(ENCRYPTDEP) $(PARDEP) -o ftpserver
	mv ftpserver ../bin/ftpserver/

bin:
	mkdir -p ../bin
	mkdir -p ../bin/ftpclient/
	mkdir -p ../bin/ftpserver/

clean:
	rm ../bin/ftpclient ../bin/ftpserver
