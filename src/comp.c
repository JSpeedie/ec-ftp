#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lzma/LzmaLib.h"
#include "comp.h"


/** Takes an array of chars '*ret', and an open file '*f' and attempts to read
 * 'num_bytes' bytes from the file, filling '*ret' with what it read.
 *
 * \param '*ret' a pointer to an array chars at least 'num_bytes' long which
 *     will be modified to contain the characters (or bytes) read from the
 *     file.
 * \param 'num_bytes' the number of bytes that should be read from the file.
 * \param '*f' an open file from which the function will attempt to read the
 *     bytes.
 * \return 0 on success, or a negative int upon failure.
 *
 */
int read_bytes(void * ret, size_t num_bytes, FILE * f) {
	/* {{{ */
	size_t nmem_read = 0;
	if (num_bytes != (nmem_read = fread(ret, 1, num_bytes, f)) ) {
		fprintf(stderr, "ERROR: read_bytes(): could not read %lu bytes, was only able to read %lu\n", num_bytes, nmem_read);
		return -1;
	}

	return 0;
	/* }}} */
}


/** Takes pointer to a struct pec_header and an open file stream, and reads
 * the PEC values from the stream into the struct.
 *
 * \param '*pecheader' a pointer to a struct pec_header which will be modified
 *     to contain to the PEC header values represented by the next few bytes in
 *     the file.
 * \param '*f' an open file stream from which this function will read.
 * \return 0 upon success, a negative int on failure.
 */
int read_pec_header(struct pec_header * pecheader, FILE * f) {
	/* {{{ */
	if (1 != fread(&pecheader->compressed, sizeof(char), 1, f)) {
		return -1;
	}
	if (1 != fread(&pecheader->orig_size, sizeof(size_t), 1, f)) {
		return -2;
	}
	if (1 != fread(&pecheader->proc_size, sizeof(size_t), 1, f)) {
		return -3;
	}

	return 0;
	/* }}} */
}


/** Takes pointer to a struct pec_header and an open file stream, and writes
 * the PEC values from the struct to the stream.
 *
 * \param '*pecheader' a pointer to a struct pec_header which will have its
 *     header values written to the file.
 * \param '*f' an open file stream which this function will write to.
 * \return 0 upon success, a negative int on failure.
 */
int write_pec_header(struct pec_header * pecheader, FILE * f) {
	/* {{{ */
	if (1 != fwrite(&pecheader->compressed, sizeof(char), 1, f)) {
		return -1;
	}
	if (1 != fwrite(&pecheader->orig_size, sizeof(size_t), 1, f)) {
		return -2;
	}
	if (1 != fwrite(&pecheader->proc_size, sizeof(size_t), 1, f)) {
		return -3;
	}

	return 0;
	/* }}} */
}


/** Writes nothing to a file, clearing it.
 *
 * \param '*file' the file path for the file to be wiped
 * \return void
 */
void clear_file(char * file) {
	/* {{{ */
	FILE* victim = fopen(file, "w");
	if (victim == NULL) {
		perror("fopen (clear_file)");
		return;
	}
	fprintf(victim, "");
	fclose(victim);

	return;
	/* }}} */
}


/** Take a file name and returns a malloc'd string containing a file name
 * which has the compression extension appended. The returned string must
 * be freed by the caller of this function.
 *
 * \param '*filename' the name of the file we wish to generate a compression
 *     name for.
 * \return a pointer to the name string on success, and NULL on failure.
 */
char * compression_name(char * filename) {
	/* {{{ */
	// TODO: get this working with the ".enc" definition in the header
	char * c_ext = ".comp";
	/* char * c_ext = COMP_EXT; */
	int c_ext_len = strlen(c_ext);

	/* GTNC: Generate the name for our compressed file */
	/* GTNC1: Calculate how much space we need for the file name for the
	 * compressed file's file name and create a zero'd array for it. */
	char *c_out_fp = calloc(strlen(filename) + c_ext_len + 1, sizeof(char));

	/* GTNC2: Fill the string with the filename + the compression extension */
	// TODO: can this construction not be done faster or more efficiently?
	strncpy(&c_out_fp[0], filename, strlen(filename));
	strncat(&c_out_fp[0], c_ext, c_ext_len + 1);

	return c_out_fp;
	/* }}} */
}


/** Take a file name and returns a malloc'd string containing a temp file name
 * which has the compression extension appended. The returned string must
 * be freed by the caller of this function.
 *
 * \param '*filename' the name of the file we wish to generate a temporary
 *     compression name for.
 * \return a pointer to the temp name string on success, and NULL on failure.
 */
char * temp_compression_name(char * filename) {
	/* {{{ */
	// TODO: get this working with the ".enc" definition in the header
	char * c_ext = ".comp";
	/* char * c_ext = COMP_EXT; */
	int c_ext_len = strlen(c_ext);

	/* GTNC: Generate a temp name for our compressed file */
	/* GTNC1: Calculate how much space we need for the file name for the
	 * compressed file's file name and create a zero'd array for it. + 7 for
	 * 'mkstemp()' "-XXXXXX" */
	char *c_out_fp = calloc(strlen(filename) + c_ext_len + 7 + 1, sizeof(char));

	/* GTNC2: Fill the string with the filename + the compression extension +
	 * the suffix characters 'mkstemp()' requires and will modify */
	// TODO: can this construction not be done faster or more efficiently?
	strncpy(&c_out_fp[0], filename, strlen(filename));
	strncat(&c_out_fp[0], c_ext, c_ext_len + 1);
	strncat(&c_out_fp[0], "-XXXXXX", 8);

	/* GTNC3: Generate a temp name for our compressed file with 'mkstemp()' */
	int r;
	if ( (r = mkstemp(c_out_fp)) == -1) return NULL;
	/* ... close the open FD and deleting the temp file created as we intend to
	 * use its name later */
	close(r);
	unlink(c_out_fp);

	return c_out_fp;
	/* }}} */
}


/* Define a struct for passing arguments to a thread used for compressing or
 * uncompressing a file */
typedef struct comp_thread_args {
	/* An open file stream from which will do the reading. It should be
	 * positioned correctly before being passed to the thread */
	FILE *input_reader;
	/* Stores the data read from the file */
	unsigned char * inbuf;
	/* Stores the length in bytes to be read by a given reader, and must also
	 * be equal to the length of the memory represented by '*inbuf' */
	size_t ir_readlen;
	/* Stores the LZMA props for the compressed data */
	unsigned char * props;
	/* The number of bytes in that can be used at '*props' */
	size_t props_len;
	/* Stores the compressed data produced by the thread */
	unsigned char * outbuf;
	/* The number of bytes in the buffer for output (compressed) data */
	size_t outbuf_len;
	/* the PEC header */
	struct pec_header pecheader;
}CompThreadArgs;


/** Helper function for compressing a file through multiple threads */
void *compress_chunk_of_file(void *arg) {
	/* {{{ */
	printf("running thread func\n");
	struct comp_thread_args *t = (struct comp_thread_args *) arg;

	/* 1. Read the assigned number of bytes from the given (and already
	 * positioned) file stream */
	if (0 != read_bytes(&t->inbuf[0], t->ir_readlen, t->input_reader)) {
		fprintf(stderr, "ERROR: thread couldn't read assigned number of bytes from given input file stream\n");
		// TODO: some sort of error return code?
	}

	/* 2. Compress the data in 't->inbuf' and put in 't->outbuf' */
	MY_STDAPI compret = LzmaCompress( \
		&t->outbuf[0], &t->outbuf_len, &t->inbuf[0], t->ir_readlen, \
		&t->props[0], &t->props_len, 5, 1 << 24, 3, 0, 2, 32, 1);
	if (compret != SZ_OK) {
		fprintf(stderr, "ERROR: compression call failed!\n");
		// TODO: some sort of error return code?
	}

	/* 3. Prepare the PEC header */
	/* If the compressed data (+ its props) takes up the same amount of space
	 * or more than the input data */
	if (t->props_len + t->outbuf_len >= t->ir_readlen) {
		t->pecheader.compressed = PEC_UNCOMPRESSED;
		/* Store the size of stored data (uncompressed) */
		memcpy(&t->pecheader.proc_size, &t->ir_readlen, sizeof(t->ir_readlen));
	} else {
		t->pecheader.compressed = PEC_COMPRESSED;
		/* Store the size of stored data (compressed) */
		memcpy(&t->pecheader.proc_size, &t->outbuf_len, sizeof(t->outbuf_len));
	}
	/* Store the size of the uncompressed data in the PEC header */
	memcpy(&t->pecheader.orig_size, &t->ir_readlen, sizeof(t->ir_readlen));

	return NULL;
	/* }}} */
}


/** Helper function for uncompressing a file through multiple threads */
void *uncompress_chunk_of_file(void *arg) {
	/* {{{ */
	printf("running thread func\n");
	struct comp_thread_args *t = (struct comp_thread_args *) arg;

	/* 1. Read the processed data from file chunk */
	if (0 != read_bytes(&t->inbuf[0], t->ir_readlen, t->input_reader)) {
		fprintf(stderr, "ERROR: thread couldn't read assigned number of bytes from given input file stream\n");
		// TODO: some sort of error return code?
	}

	/* 2. Setup 'outbuf' for printing (uncompress inbuf data, or redirect
	 * 'outbuf' to 'inbuf') */
	/* If the processed data is compressed */
	if (t->pecheader.compressed == PEC_COMPRESSED) {
		/* Uncompress processed data in 't->inbuf' and put in 't->outbuf' */
		MY_STDAPI uncompret = LzmaUncompress( \
			&t->outbuf[0], &t->outbuf_len, &t->inbuf[0], &t->ir_readlen, \
			&t->props[0], t->props_len);
		if (uncompret != SZ_OK) {
			fprintf(stderr, "ERROR: uncompression call failed!\n");
			// TODO: some sort of error return code?
		}
	/* If the processed data is not compressed, simply redirect */
	} else {
		t->outbuf = t->inbuf;
		t->outbuf_len = t->pecheader.orig_size;
	}

	return NULL;
	/* }}} */
}


/** Takes an input file path, compresses the file at that location, writing
 * the compressed result to the file at the output file path.
 *
 * \param '*inputfilepath' the path to the input file.
 * \param '*outputfilepath' the path to the output file.
 * \return 0 upon success, and a negative int upon failure.
 */
int comp_file(char * inputfilepath, char * outputfilepath) {
	/* {{{ */
	struct stat s;
	short num_batches = 1;
	stat(inputfilepath, &s);
	long max_bytes_per_batch = COMP_THREAD_MAX_MEM * COMP_MAX_THREADS;

	/* If the size of the file is too large for all its data to be loaded into
	 * memory and compressed in one threaded batch */
	if (s.st_size > max_bytes_per_batch) {
		fprintf(stderr, "WARNING: File size is bigger than the memory limit " \
				"for one threaded compression batch (%ld > %ld bytes) and " \
				"must instead be compressed incrementally.\n", \
				s.st_size, max_bytes_per_batch);
		/* "Ceiled" division so that the number of jobs is always sufficient
		 * to compress the whole file */
		num_batches = (s.st_size / max_bytes_per_batch) + 1;
	}

	printf("num_batches = %d\n", num_batches);

	clear_file(outputfilepath);

	FILE *out_writer = fopen(outputfilepath, "ab");
	if (out_writer == NULL) {
		perror("fopen (comp_file)");
		return -1;
	}

	char num_threads = COMP_MAX_THREADS;
	CompThreadArgs args[num_threads];
	uint64_t batch_len;

	/* Go through the input file in batches, breaking into threads for each
	 * batch, each thread compressing an assigned part of the file before
	 * pthread_join'ing back to the main thread, which will then loop through
	 * the compressed data, writing it (or the raw data if it takes up less
	 * space) to the output file. This all takes place in 3 broad stages:
	 * STA: Set Thread Arguments
	 * RT: Run Threads
	 * MUTW: Make use of the Threads' Work */
	for (int batch_index = 0; batch_index < num_batches; batch_index++) {

		/* If this is not the last batch, read a batch of maximum length with
		 * the maximum number of threads */
		if (batch_index != num_batches - 1) {
			batch_len = max_bytes_per_batch;
			num_threads = COMP_MAX_THREADS;
		} else {
			/* Set the number of threads to how many 'COMP_THREAD_MAX_MEM' byte
			 * chunks of the file are left */
			batch_len = s.st_size - (batch_index * max_bytes_per_batch);
			num_threads = (batch_len / COMP_THREAD_MAX_MEM) + 1;
		}

		printf("num_threads in batch %d = %d\n", batch_index, num_threads);

		/* STA: Set Thread Arguments */

		/* STA1: Open up the input file with 'num_batches' readers, each at their
		* starting location for the reading they have to do */
		for (int thread_index = 0; thread_index < num_threads; thread_index++) {
			args[thread_index].input_reader = fopen(inputfilepath, "rb");
			if (args[thread_index].input_reader == NULL) {
				perror("fopen (comp_file)");
				return -1;
			}
			/* STA2: Position the cursor of each reader such that it will
			 * read at the part of the file it is responsible for reading */
			fseek(args[thread_index].input_reader, \
				(batch_index * max_bytes_per_batch) + (thread_index * COMP_THREAD_MAX_MEM), \
				SEEK_SET);
			/* STA3: Allocate space for the LZMA properties(?) */
			args[thread_index].props_len = LZMA_PROPS_SIZE;
			args[thread_index].props = malloc(LZMA_PROPS_SIZE);
			if (args[thread_index].props == NULL) {
				fprintf(stderr, "ERROR: could not allocate props\n");
				return -1;
			}

			/* STA4: Set the number of bytes (the read length) each input
			 * reader must read ('ir_readlen'), allocate memory for reading
			 * from the file, set the number of bytes in the out buffer
			 * ('outbuf_len'), and allocate memory for the out buffer */
			/* If this is the last thread in the last batch, do not blindly
			 * read the maximum amount, but only what is left to read */
			if (batch_index == num_batches - 1 && thread_index == num_threads - 1) {
				args[thread_index].ir_readlen = \
					s.st_size \
					- (batch_index * max_bytes_per_batch) \
					- (thread_index * COMP_THREAD_MAX_MEM); // If this is the last thread, read only the remaining bytes of the file
				args[thread_index].inbuf = malloc(args[thread_index].ir_readlen);
				if (args[thread_index].inbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate input buffer\n");
					return -1;
				}
				/* / 3 + 128 was some simple math recommended by LZMA SDK? */
				args[thread_index].outbuf_len = args[thread_index].ir_readlen + args[thread_index].ir_readlen / 3 + 128;
				args[thread_index].outbuf = malloc(args[thread_index].outbuf_len);
				if (args[thread_index].outbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate output buffer\n");
					return -1;
				}
			} else {
				args[thread_index].ir_readlen = COMP_THREAD_MAX_MEM;
				args[thread_index].inbuf = malloc(COMP_THREAD_MAX_MEM);
				if (args[thread_index].inbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate input buffer\n");
					return -1;
				}
				/* / 3 + 128 was some simple math recommended by LZMA SDK? */
				size_t max_outbuf_len = COMP_THREAD_MAX_MEM + COMP_THREAD_MAX_MEM / 3 + 128;
				args[thread_index].outbuf_len = max_outbuf_len;
				args[thread_index].outbuf = malloc(max_outbuf_len);
				if (args[thread_index].outbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate output buffer\n");
					return -1;
				}
			}
		}

		/* RT: Run Threads */
		pthread_t thread_id[num_threads];
		/* RT1: Create threads with their given tasks/arguments */
		for (int t = 0; t < num_threads - 1; t++) {
			if (0 != \
				pthread_create(&thread_id[t], NULL, compress_chunk_of_file, &args[t])) {

				fprintf(stderr, "ERROR: Could not create threads\n");
				return -1;
			}
		}
		/* RT2: Have this "thread" compress as well since otherwise it would be
		 * waiting idly */
		compress_chunk_of_file(&args[num_threads - 1]);

		/* RT3: Wait for all the threads to finish their compression */
		for (int t = 0; t < num_threads - 1; t++) {
			pthread_join(thread_id[t], NULL);
		}

		/* MUTW: Make use of the Threads' Work */
		for (int t = 0; t < num_threads; t++) {
			/* MUTW1: Write the PEC header of the current thread to the output file */
			if (0 != write_pec_header(&args[t].pecheader, out_writer)) {
				fprintf(stderr, "ERROR: Could not write PEC header to output file\n");
				return -1;
			}

			/* MUTW2: Write the (lzma props + the outbuf) or (the inbuf) to the
			 * output file, writing 'inbuf' if the compressed data (+ its props)
			 * takes up the same amount of space or more than the input data */
			if (args[t].props_len + args[t].outbuf_len >= args[t].ir_readlen) {
				if (1 != \
					fwrite(&args[t].inbuf[0], args[t].ir_readlen, 1, out_writer)) {

					fprintf(stderr, "ERROR: Could not write data content to output file\n");
					return -1;
				}
			} else {
				if (1 != \
					fwrite(args[t].props, args[t].props_len, 1, out_writer)) {

					fprintf(stderr, "ERROR: Could not write data content output file\n");
					return -1;
				}

				if (1 != \
					fwrite(args[t].outbuf, args[t].outbuf_len, 1, out_writer)) {

					fprintf(stderr, "ERROR: Could not write data content output file\n");
					return -1;
				}
			}
			/* Free dynamically alloc'd buffers */
			free(args[t].inbuf);
			free(args[t].props);
			free(args[t].outbuf);
			/* Close open file streams */
			fclose(args[t].input_reader);
		}
	}

	fclose(out_writer);

	return 0;
	/* }}} */
}


/** Takes an input file path, which points to a file compressed by
 * 'comp_file()' and uncompresses it, writing the uncompressed result to the
 * file at the output file path.
 *
 * \param '*inputfilepath' the path to the input file.
 * \param '*outputfilepath' the path to the output file.
 * \return 0 upon success, and a negative int upon failure.
 */
int uncomp_file(char * inputfilepath, char * outputfilepath) {
	/* {{{ */
	struct stat s;
	stat(inputfilepath, &s);

	FILE * in_file = fopen(inputfilepath, "rb");
	if (in_file == NULL) {
		perror("fopen (uncomp_file)");
		return -1;
	}

	clear_file(outputfilepath);

	FILE * out_writer = fopen(outputfilepath, "ab");
	if (out_writer == NULL) {
		perror("fopen (uncomp_file)");
		return -1;
	}

	off_t bytes_left = s.st_size;
	off_t cur_pos = 0;
	char num_threads = COMP_MAX_THREADS;
	CompThreadArgs args[num_threads];
	int batch_index = 0;

	/* Go through the input file in batches, breaking into threads for each
	 * batch, each thread uncompressing an assigned part of the file before
	 * pthread_join'ing back to the main thread, which will then loop through
	 * that batch's uncompressed data, appending it to the output file. Note
	 * that before a batch can break into threads, it must be determine how
	 * many threads are needed. This is done by attempting to determine the
	 * work for a set maximum number of threads, and upon reaching the EOF when
	 * attempting to read a given PEC header, capping the number of threads.
	 * The PEC headers specify all the work parameters for the threads.
	 * On a smaller scale, we can say this all takes place in 3 broad stages:
	 * STA: Set Thread Arguments
	 * RT: Run Threads
	 * MUTW: Make use of the Threads' Work */
	while (bytes_left > 0) {

		num_threads = COMP_MAX_THREADS;

		/* STA: Set Thread Arguments */
		for (int thread_index = 0; thread_index < COMP_MAX_THREADS; thread_index++) {
			/* STA1: Read PEC header for chunk */
			if (0 != read_pec_header(&args[thread_index].pecheader, in_file)) {
				/* If the read failed because the end of the file was reached... */
				if (0 == feof(in_file)) {
					/* ... then we have already read the last chunk, and
					 * we should start running the threads immediately */
					num_threads = thread_index;
					break;
				} else {
					fprintf(stderr, "ERROR: couldn't read PEC header from chunk\n");
					return -1;
				}
			}

			bytes_left -= PEC_HEADER_SIZE;
			cur_pos += PEC_HEADER_SIZE;

			/* STA2: Set up a reader for each thread such that it will read
			 * at the part of the file it is responsible for reading */
			args[thread_index].input_reader = fopen(inputfilepath, "rb");
			if (args[thread_index].input_reader == NULL) {
				perror("fopen (uncomp_file)");
				return -1;
			}
			/* Position the cursor of each reader */
			fseek(args[thread_index].input_reader, cur_pos, SEEK_SET);

			/* STA3: Allocate space for the LZMA properties(?) */
			args[thread_index].props_len = LZMA_PROPS_SIZE;
			args[thread_index].props = malloc(LZMA_PROPS_SIZE);
			if (args[thread_index].props == NULL) {
				fprintf(stderr, "ERROR: could not allocate props\n");
				return -1;
			}
			/* STA4: Set the read length and allocate space for the input buffer */
			args[thread_index].ir_readlen = args[thread_index].pecheader.proc_size;
			args[thread_index].inbuf = malloc(args[thread_index].ir_readlen);
			if (args[thread_index].inbuf == NULL) {
				fprintf(stderr, "ERROR: could not allocate input buffer\n");
				return -1;
			}
			/* STA5: If the processed data is compressed, allocate room for
			 * uncompressed data */
			if (args[thread_index].pecheader.compressed == PEC_COMPRESSED) {
				args[thread_index].outbuf_len = args[thread_index].pecheader.orig_size;
				args[thread_index].outbuf = malloc(args[thread_index].pecheader.orig_size);
				if (args[thread_index].outbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate output buffer\n");
					return -1;
				}
			}

			/* STA6: Now that we have set the arguments for the current thread,
			 * move the file cursor to beginning of next (pec header +
			 * processed data) chunk for the next thread */
			if (0 != fseek(in_file, args[thread_index].pecheader.proc_size, SEEK_CUR)) {
				perror("fseek (uncomp_file)");
				return -1;

			}

			bytes_left -= args[thread_index].pecheader.proc_size;
			cur_pos += args[thread_index].pecheader.proc_size;
		}

		printf("num_threads in batch %d = %d\n", batch_index, num_threads);

		/* RT: Run Threads */
		pthread_t thread_id[num_threads];
		/* RT1: Create threads with their given tasks/arguments */
		for (int t = 0; t < num_threads - 1; t++) {
			if (0 != \
				pthread_create(&thread_id[t], NULL, uncompress_chunk_of_file, &args[t])) {

				fprintf(stderr, "ERROR: Could not create threads\n");
				return -1;
			}
		}
		/* RT2: Have this "thread" uncompress as well since otherwise it would be
		 * waiting idly */
		uncompress_chunk_of_file(&args[num_threads - 1]);

		/* RT3: Wait for all the threads to finish their compression */
		for (int t = 0; t < num_threads - 1; t++) {
			pthread_join(thread_id[t], NULL);
		}

		/* MUTW: Make use of the Threads' Work */
		for (int t = 0; t < num_threads; t++) {
			/* Write the outbuf to the output file */
			if (1 != \
				fwrite(args[t].outbuf, args[t].outbuf_len, 1, out_writer)) {

				fprintf(stderr, "ERROR: Could not write data content output file\n");
				return -1;
			}
			/* Free dynamically alloc'd buffers */
			free(args[t].inbuf);
			free(args[t].props);
			/* 'outbuf' is set to point to 'inbuf' if the data was not
			 * compressed. To avoid double freeing 'inbuf', check that the data
			 * was compressed before freeing 'outbuf' */
			if (args[t].pecheader.compressed == PEC_COMPRESSED) {
				free(args[t].outbuf);
			}
			/* Close open file streams */
			fclose(args[t].input_reader);
		}

		batch_index++;
	}

	fclose(out_writer);

	return 0;
	/* }}} */
}
