#ifndef ENC_HEADER
#define ENC_HEADER
#include <stdint.h>
#include <stdio.h>

#define ENC_EXT ".enc"

/* How many bytes each thread involved in encryption/decryption is allowed to
 * read into memory. The value specified /MUST/ be a multiple of 16.
 * 4194304 ~= 4 MB */
#define ENC_THREAD_MAX_MEM 4194304
/* The maximum number of threads that can be created during
 * encryption/decryption. */
#define ENC_MAX_THREADS 4


/* Define a struct for passing arguments to a thread used for
 * encrypting/decrypting a file */
typedef struct enc_thread_args {
	/* An open file stream from which will do the reading. It should be
	 * positioned correctly before being passed to the thread */
	FILE *input_reader;
	/* Stores the data read from the file */
	unsigned char * inbuf;
	/* Stores the length in bytes to be read by a given reader, and must also
	 * be equal to the length of the memory represented by '*inbuf' */
	size_t ir_readlen;
	/* Stores the compressed data produced by the thread */
	unsigned char * outbuf;
	/* The number of bytes in the buffer for output (compressed) data */
	size_t outbuf_len;
	/* Whether the encrypted data is padded or not */
	char padded;
	/* Necessary encryption vars */
	uint8_t sbox[256];
	uint8_t rkeys[11][16];
	/* For returning a success/error code */
	int return_val;
}EncThreadArgs;


char * temp_encryption_name(char * filename);

void init_enc();

uint64_t sq_mp(uint64_t, uint64_t, uint64_t);

int enc_file(char *, char *, uint32_t[4]);

int dec_file(char *, char *, uint32_t[4]);

#endif
