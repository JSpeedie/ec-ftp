#include <stdint.h>
#include <stdio.h>

/* How many bytes each thread is allowed to read into memory
 * A Gibibyte (GiB) 1,073,741,824 / 4 (the number of threads) / 2 (since each thread has an inbuf and outbuf allocated)
 * = */
#define COMP_THREAD_MAX_MEM 134217728
/* The maximum number of threads that can be created during compression */
#define COMP_MAX_THREADS 4

#define PEC_UNCOMPRESSED 0
#define PEC_COMPRESSED 1
static const size_t PEC_HEADER_SIZE = sizeof(char) + sizeof(size_t) + sizeof(size_t);


/* PEC Headers are made of 3 elements: a char representing whether the data in
 * the following chunk is compressed or not, a size_t representing the size the
 * data uncompressed will occupy, and another size_t representing the size of
 * the data in the chunk */
struct pec_header {
	/* Compressed char which represents whether the corresponding data has been
	 * compressed or not. Should only ever be == PEC_COMPRESSED or
	 * PEC_UNCOMPRESSED */
	char compressed;
	/* Original data size which represents how many bytes the original
	 * data will occupy after (possible) uncompression */
	size_t orig_size;
	/* Processed data size which represents how many bytes the processed
	 * (possibly compressed, possibly not-compressed) data occupies */
	size_t proc_size;
};


int read_bytes(void * ret, size_t num_bytes, FILE * f);

unsigned char * read_chunk_of_file(FILE * f, uint64_t * ret_len);

void clear_file(char * file);

char * compression_name(char * filename);

char * temp_compression_name(char * filename);

int comp_file(char * inputfilepath, char * outputfilepath);

int uncomp_file(char * inputfilepath, char * outputfilepath);
