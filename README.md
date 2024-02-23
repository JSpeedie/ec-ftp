# ec-ftp

## Technical Description

This repo contains a partial C implementation of FTP (as specified in [RFC
959](https://www.ietf.org/rfc/rfc959.txt)) extended such that transferred data
is both compressed and encrypted.

<p align="center">
  <img src="https://raw.githubusercontent.com/wiki/JSpeedie/ec-ftp/images/ec-ftp-demo.gif" width="100%"/>
</p>


## Purpose

To add encryption and compression to FTP to improve security and reduce network
strain. FTP is no longer supported by Firefox (since July 2021) and Chrome
(since January 2021) because of security concerns, and since FTP might send a large
file over a long network path, compressing the file first may reduce the load
faced by routers, switches, and other network elements.


## Elements of Note

* Multithreading
    * When compressing a file, decompressing a file, encrypting a file and
      decrypting a file, the program performing the process will break into
      threads in order to finish the task faster.
* Network communication
    * This repo contains a server and client which communicate over
      both a data and control connection.
* Efficient and well-featured Makefile
    * More than compiling in an organized way (such as placing objects files in
      `obj/` subdirectories to prevent recompiling them neeedlessly, while
      still keeping the working directory clear), the Makefile also includes a
      `debug` rule for compiling the server and client such that they will
      output debug information when run.
* This project has a [wiki](https://github.com/JSpeedie/ec-ftp/wiki) which
  documents the various file formats used by the executables, their process
  flows, and walkthroughs and tests.


## Installation, Compilation and Running

### Installation

```bash
# If you want to push to the repo
git clone git@github.com:JSpeedie/ec-ftp.git ec-ftpGit
# If you are just installing
git clone https://www.github.com/JSpeedie/ec-ftp ec-ftpGit
cd ec-ftpGit/
```

### Compilation

First you will need to compile the project. You can do this by running the
following commands:

```bash
cd ec-ftpGit/
cd src/
# If you want a standard compilation
make
# If you want to compile the binaries to output debug information
make debug
```

This will compile both the `ecftpclient` and `ecftpserver` binaries placing them in
`ec-ftp/bin/ecftpclient/` and
`ec-ftp/bin/ecftpserver/`, respectively.

### Running the code

In one terminal, start the server:

```
cd ec-ftpGit/
cd bin/ecftpserver/
./ecftpserver <listen-port>
```

For example:

```
./ecftpserver 45678
```

In another terminal, start the client:

```
cd ec-ftpGit/
cd bin/ecftpclient/
./ecftpclient <server-ip> <server-listen-port>
```

For example: (make sure you run the server first)

```
./ecftpclient 127.0.0.1 45678
```

If your `ecftpclient` says its connection was denied, make sure you entered the
same port for both your `ecftpserver` and `ecftpclient`. If you did, try a
different port.

You will interface with the server through the client, you cannot run any
commands on the server side, but you will see some output that might be helpful.

#### Client Interface Commands

- ls, lists the current directory
- get <filename>, gets the file from server to client.
- put <filename>, puts the files from the client to the server.
- quit, exits the client program


## Details about the underlying FTP implementation

RFC959 was used as the reference specifications for this project:

[https://www.ietf.org/rfc/rfc959.txt](https://www.ietf.org/rfc/rfc959.txt)

The underlying FTP implementation uses the following FTP settings:

* Image as the representation type (3.1.1)
* file-structure as the data structure for files (3.1.2) (this is FTP default)
* Stream mode as the transmission mode (3.4) (this is FTP default)

As such, all file transfers are effectively preceded by the following FTP commands:

```
TYPE I
STRU F
MODE S
```

The FTP implementation also supports the following FTP commands:

```
QUIT
PORT h1,h2,h3,h4,p1,p2
RETR [filename]
STOR [filename]
LIST
```


## Contributions

### Julian Speedie

Julian made the small fixes to the original FTP client/server program pair to
get it to work with all types of files, worked on the compression part of the
project, cleaned up and documented the code base, rewrote the Makefile, wrote
the wiki, and wrote the multithreading code for compression, decompressiong,
encryption, and decryption. Most of the compression work can be found in
`comp.c`, as well as the compression-related changes made in `do_put()`,
`do_get()` of `src/ecftpclient.c` and in `do_retr()`, `do_stor()` of
`src/ecftpserver.c`, but other related work can be found in
`process_received_file()`, `prepare_file()` in `ecftp.c`. Multithreading work
can be found in `compress_chunk_of_file()`, `uncompress_chunk_of_file()`,
`comp_file()` and `uncomp_file()` in `comp.c`, as well as in
`encrypt_chunk_of_file()`, `enc_file()`, `decrypt_chunk_of_file()`, and
`dec_file()` in `enc.c`.


### Dawson Brown

Dawson worked on the encryption portion of the project. Work can be found in
the files `enc.c`, `enc.h`, `aes.c`, and `aes.h`. Specifically, `aes.c`
includes an implementation of Rjindael encryption (see
https://en.wikipedia.org/wiki/Advanced_Encryption_Standard) with a 128-bit key,
using ECB mode. `enc.c` includes methods for encrypting and decrypting files
using the aes code, as well as an implementation of the square and multiply
algorithm for calculating the power of large values mod n.

DISCLAIMER: Since this is a minimal implementation, it carries many security
concerns. First, Electronic Code Book (ECB) mode encrypts each 16 byte block
independently of the others, using the same key. This means patterns in the
data are preserved, and identical blocks will be encrypted identically, making
it very vulnerable to cryptanalysis (For more information, see
https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Electronic_codebook_(ECB)).
For more secure encryption, a different mode (such as Cypher Block Chaining,
which modifies each block based on the previous one) would have to be used.
Additonally, the Diffie-Hellman private keys are generated using the built in C
rand() function seeded with the system time. The built in rand() implementation
produces correlated numbers with a short period, which can lead to
vulnerabilities. Cryptographically secure random number generators are
available, and would be far more secure. Finally, there are many side channel
attacks (see https://en.wikipedia.org/wiki/Side-channel_attack) which this
implementation does not defend against. While the implementation is ultimately
suitable for our needs and does provide a level of encryption, it should not be
considered secure in any production environment.


## Acknowledgements

This project is comprised of 4 major, distinct contributions.

### 1. Basic FTP Client/Server by users pranav93y and nishant-sachdeva

[https://github.com/pranav93y/Basic-FTP-Client-Server](https://github.com/pranav93y/Basic-FTP-Client-Server)  
It's worth noting that while it ultimately was able to serve as the FTP
implementation on which we made our extensions (as *extensions* to FTP was the
focus of this project), this code base was not fully functional. Importantly,
changes had to be made to enable support for non-text files, and cleaning up
and documenting the code base were necessary.

### 2. The 7-Zip LZMA SDK

[https://7-zip.org/sdk.html](https://7-zip.org/sdk.html)  
The compression algorithm used in our extension to FTP was the LZMA (aka the
Lempel-Ziv-Markov chain algorithm), and the C implementation of it that our
project depends on was provided by this public domain SDK. All the files in
`src/lzma` come from the 7-Zip LZMA SDK and were not touched by us in anyway.

### 3. Minor Contributions

Several minor contributions came from Wikipedia. Specifically, the `ROTC` definition, 
as well as the `initialize_aes_sbox()` and `g_mul()` functions, all in `aes.c`.

### 4. The CSCD58 students who worked on this project

Collectively Julian Speedie and Dawson Brown made the following files:

```
aes.c
aes.h
comp.c
comp.h
ecftp.c
ecftp.h
enc.c
enc.h
```

And made important modifications to the remaining files:

```
ecftpclient.c
ecftpserver.c
```

### 5. Julian Speedie, continuing work on this project

After submission Julian continued to work on the project, redoing the Makefile,
making large changes to...

```
comp.c
comp.h
enc.c
enc.h
```

to enable multithreaded compression, decompression, encryption and decryption,
as well as doing lots of work in...

```
ecftp.c
ecftp.h
```

to maximize code readability and reuseability. Similar work to document and
increase readability of the code was done in...

```
ecftpclient.c
ecftpserver.c
```
