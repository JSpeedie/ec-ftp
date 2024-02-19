#include <stdint.h>
#include <stdio.h>

#define COMP_EXT ".comp"
/* How many bytes each thread involved in compression/uncompression is allowed
 * to read into memory. A Gibibyte (GiB) 1,073,741,824 / 4 (the number of
 * threads) / 2 (since each thread has an inbuf and outbuf allocated) = */
/* #define COMP_THREAD_MAX_MEM 134217728 */

// TODO: remove? Test which is faster - max_mem = 8megs vs 134megs
/* How many bytes each thread involved in compression/uncompression is allowed
 * to read into memory 8000000 = 8 MB */
#define COMP_THREAD_MAX_MEM 8000000

/*        1 GB = (1 << 30) bytes (maximum value for 64-bit version)
 *      128 MB = (1 << 27) bytes (maximum value for 32-bit version)
 *       16 MB = (1 << 24) bytes (default) */
static const size_t COMP_DICT_SIZE = (1 << 24);
/* An integer from 1-9, 5 is the default, higher number = greater compression
 * ratio */
static const size_t COMP_LEVEL = 9;
/* The maximum number of threads that can be created during
 * compression/uncompression. On user computers, setting this number to the
 * number of cores on the CPU often yields good results. On busier computers,
 * threading, but not creating too many threads might be better. */
#define COMP_MAX_THREADS 8
#define EC_UNCOMPRESSED 0
#define EC_COMPRESSED 1
static const size_t EC_HEADER_SIZE = sizeof(char) + sizeof(size_t) + sizeof(size_t);


/* EC Headers are made of 3 elements: a char representing whether the data in
 * the following chunk is compressed or not, a size_t representing the size the
 * data uncompressed will occupy, and another size_t representing the size of
 * the data in the chunk */
struct ec_header {
	/* Compressed char which represents whether the corresponding data has been
	 * compressed or not. Should only ever be == EC_COMPRESSED or
	 * EC_UNCOMPRESSED */
	char compressed;
	/* Original data size which represents how many bytes the original
	 * data will occupy after (possible) uncompression */
	size_t orig_size;
	/* Processed data size which represents how many bytes the processed
	 * (possibly compressed, possibly not-compressed) data occupies */
	size_t proc_size;
};


void clear_file(char * file);

char * compression_name(char * filename);

char * temp_compression_name(char * filename);

int comp_file(char * inputfilepath, char * outputfilepath);

int uncomp_file(char * inputfilepath, char * outputfilepath);
