#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if DEBUG_LEVEL >= 2
#include <sys/syscall.h>
#endif

#include "lzma/LzmaLib.h"
#include "comp.h"
#include "fileops.h"


/** Takes pointer to a struct ec_header and an open file stream, and reads
 * the EC values from the stream into the struct.
 *
 * \param '*echeader' a pointer to a struct ec_header which will be modified
 *     to contain to the EC header values represented by the next few bytes in
 *     the file.
 * \param '*f' an open file stream from which this function will read.
 * \return 0 upon success, a negative int on failure.
 */
int read_ec_header(struct ec_header * echeader, FILE * f) {
	/* {{{ */
	if (1 != fread(&echeader->compressed, sizeof(char), 1, f)) {
		return -1;
	}
	if (1 != fread(&echeader->orig_size, sizeof(size_t), 1, f)) {
		return -2;
	}
	if (1 != fread(&echeader->proc_size, sizeof(size_t), 1, f)) {
		return -3;
	}

	return 0;
	/* }}} */
}


/** Takes pointer to a struct ec_header and an open file stream, and writes
 * the EC values from the struct to the stream.
 *
 * \param '*echeader' a pointer to a struct ec_header which will have its
 *     header values written to the file.
 * \param '*f' an open file stream which this function will write to.
 * \return 0 upon success, a negative int on failure.
 */
int write_ec_header(struct ec_header * echeader, FILE * f) {
	/* {{{ */
	if (1 != fwrite(&echeader->compressed, sizeof(char), 1, f)) {
		return -1;
	}
	if (1 != fwrite(&echeader->orig_size, sizeof(size_t), 1, f)) {
		return -2;
	}
	if (1 != fwrite(&echeader->proc_size, sizeof(size_t), 1, f)) {
		return -3;
	}

	return 0;
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
	int c_ext_len = strlen(COMP_EXT);

	/* GTNC: Generate the name for our compressed file */
	/* GTNC1: Calculate how much space we need for the file name for the
	 * compressed file's file name and create a zero'd array for it. */
	char *c_out_fp = calloc(strlen(filename) + c_ext_len + 1, sizeof(char));

	/* GTNC2: Fill the string with the filename + the compression extension */
	// TODO: can this construction not be done faster or more efficiently?
	strncpy(&c_out_fp[0], filename, strlen(filename));
	strncat(&c_out_fp[0], COMP_EXT, c_ext_len + 1);

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
	int c_ext_len = strlen(COMP_EXT);

	/* GTNC: Generate a temp name for our compressed file */
	/* GTNC1: Calculate how much space we need for the file name for the
	 * compressed file's file name and create a zero'd array for it. + 7 for
	 * 'mkstemp()' "-XXXXXX" */
	char *c_out_fp = calloc(strlen(filename) + c_ext_len + 7 + 1, sizeof(char));

	/* GTNC2: Fill the string with the filename + the compression extension +
	 * the suffix characters 'mkstemp()' requires and will modify */
	// TODO: can this construction not be done faster or more efficiently?
	strncpy(&c_out_fp[0], filename, strlen(filename));
	strncat(&c_out_fp[0], COMP_EXT, c_ext_len + 1);
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
	/* the EC header */
	struct ec_header echeader;
	/* For returning a success/error code */
	int return_val;
}CompThreadArgs;


/** Helper function for compressing a file through multiple threads */
void *compress_chunk_of_file(void *arg) {
	/* {{{ */
#if DEBUG_LEVEL >= 2
	fprintf(stderr, "(%d) STATUS: compression: thread %ld starting...\n", \
		getpid(), syscall(SYS_gettid));
#endif
	struct comp_thread_args *t = (struct comp_thread_args *) arg;

	/* 1. Read the assigned number of bytes from the given (and already
	 * positioned) file stream */
	if (0 != read_bytes(&t->inbuf[0], t->ir_readlen, t->input_reader)) {
		fprintf(stderr, "ERROR: thread couldn't read assigned number of bytes from given input file stream\n");
		t->return_val = -1;
		return NULL;
	}

	/* 2. Compress the data in 't->inbuf' and put in 't->outbuf' */
	MY_STDAPI compret = LzmaCompress( \
		&t->outbuf[0], &t->outbuf_len, &t->inbuf[0], t->ir_readlen, \
		&t->props[0], &t->props_len, COMP_LEVEL, COMP_DICT_SIZE, 3, 0, 2, 32, 1);
	if (compret != SZ_OK) {
		fprintf(stderr, "ERROR: compression call failed!\n");
		t->return_val = -1;
		return NULL;
	}

	/* 3. Prepare the EC header */
	/* If the compressed data (+ its props) takes up the same amount of space
	 * or more than the input data */
	if (t->props_len + t->outbuf_len >= t->ir_readlen) {
		t->echeader.compressed = EC_UNCOMPRESSED;
		/* Store the size of stored data (uncompressed) */
		memcpy(&t->echeader.proc_size, &t->ir_readlen, sizeof(t->ir_readlen));
	} else {
		t->echeader.compressed = EC_COMPRESSED;
		/* Store the size of stored data (compressed) */
		memcpy(&t->echeader.proc_size, &t->outbuf_len, sizeof(t->outbuf_len));
	}
	/* Store the size of the uncompressed data in the EC header */
	memcpy(&t->echeader.orig_size, &t->ir_readlen, sizeof(t->ir_readlen));

#if DEBUG_LEVEL >= 2
	fprintf(stderr, "(%d) STATUS: compression: thread %ld finished successfully\n", \
		getpid(), syscall(SYS_gettid));
#endif

	t->return_val = 0;
	return NULL;
	/* }}} */
}


/** Helper function for uncompressing a file through multiple threads */
void *uncompress_chunk_of_file(void *arg) {
	/* {{{ */
#if DEBUG_LEVEL >= 2
	fprintf(stderr, "(%d) STATUS: uncompression: thread %ld starting...\n", \
		getpid(), syscall(SYS_gettid));
#endif
	struct comp_thread_args *t = (struct comp_thread_args *) arg;

	/* 1. If this chunk has compressed data (and thus has its data preceded by
	 * LZMA props), read the props first */
	if (t->echeader.compressed == EC_COMPRESSED) {
		if (0 != read_bytes(&t->props[0], t->props_len, t->input_reader)) {
			fprintf(stderr, "ERROR: thread couldn't read assigned number of bytes from given input file stream\n");
			t->return_val = -1;
			return NULL;
		}
	}
	/* 2. Read the processed data from file chunk */
	if (0 != read_bytes(&t->inbuf[0], t->ir_readlen, t->input_reader)) {
		fprintf(stderr, "ERROR: thread couldn't read assigned number of bytes from given input file stream\n");
		t->return_val = -1;
		return NULL;
	}

	/* 3. Setup 'outbuf' for printing (uncompress inbuf data, or redirect
	 * 'outbuf' to 'inbuf') */
	/* If the processed data is compressed */
	if (t->echeader.compressed == EC_COMPRESSED) {
		/* Uncompress processed data in 't->inbuf' and put in 't->outbuf' */
		MY_STDAPI uncompret = LzmaUncompress( \
			&t->outbuf[0], &t->outbuf_len, &t->inbuf[0], &t->ir_readlen, \
			&t->props[0], t->props_len);
		if (uncompret != SZ_OK) {
			fprintf(stderr, "ERROR: uncompression call failed!\n");
			t->return_val = -1;
			return NULL;
		}
	/* If the processed data is not compressed, simply redirect */
	} else {
		t->outbuf = t->inbuf;
		t->outbuf_len = t->echeader.orig_size;
	}

#if DEBUG_LEVEL >= 2
	fprintf(stderr, "(%d) STATUS: uncompression: thread %ld finished successfully\n", \
		getpid(), syscall(SYS_gettid));
#endif

	t->return_val = 0;
	return NULL;
	/* }}} */
}


/** Takes an input file path, compresses the file at that location, writing
 * the compressed result to the file at the output file path.
 *
 * \param '*input_fp' the path to the input file.
 * \param '*output_fp' the path to the output file.
 * \return 0 upon success, and a negative int upon failure.
 */
int comp_file(char * input_fp, char * output_fp) {
	/* {{{ */
	struct stat s;
	unsigned int num_batches = 1;
	stat(input_fp, &s);
	long max_bytes_per_batch = COMP_THREAD_MAX_MEM * COMP_MAX_THREADS;

#if DEBUG_LEVEL >= 1
	/* Print warning if the file will require more than one thread */
	if (s.st_size > COMP_THREAD_MAX_MEM) {
		fprintf(stderr, "(%d) WARNING: compression: File size is bigger than " \
			"the memory limit for one compression thread (%ld > %d bytes). " \
			"The file will be compressed across multiple threads.\n", \
			getpid(), s.st_size, COMP_THREAD_MAX_MEM);
	}
#endif

	/* If the size of the file is too large for all its data to be loaded into
	 * memory and compressed in one batch of threads */
	if (s.st_size > max_bytes_per_batch) {
#if DEBUG_LEVEL >= 1
	/* Print warning if the file will require more than one batch of threads */
	fprintf(stderr, "(%d) WARNING: compression: File size is bigger than " \
		"the memory limit for one threaded compression batch " \
		"(%ld > %ld bytes). The file will be compressed over multiple " \
		"multithreaded batches.\n", \
		getpid(), s.st_size, max_bytes_per_batch);
#endif
		/* "Ceiled" division so that the number of jobs is always sufficient
		 * to compress the whole file */
		num_batches = (s.st_size / max_bytes_per_batch) + 1;
	}

#if DEBUG_LEVEL >= 1
	fprintf(stderr, "(%d) STATUS: compression: number of batches needed " \
		"= %d\n", getpid(), num_batches);
#endif


	clear_file(output_fp);

	FILE *out_writer = fopen(output_fp, "ab");
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
	for (unsigned int batch_index = 0; batch_index < num_batches; batch_index++) {

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

#if DEBUG_LEVEL >= 1
	fprintf(stderr, "(%d) STATUS: compression: batch (%d/%d) has %d threads\n", \
		getpid(), batch_index + 1, num_batches, num_threads);
#endif

		/* STA: Set Thread Arguments */

		/* STA1: Open up the input file with 'num_batches' readers, each at their
		* starting location for the reading they have to do */
		for (int thread_index = 0; thread_index < num_threads; thread_index++) {
			args[thread_index].input_reader = fopen(input_fp, "rb");
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
				fprintf(stderr, "ERROR: could not allocate props "\
					"(asked for %d bytes)\n", LZMA_PROPS_SIZE);
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
					fprintf(stderr, "ERROR: could not allocate input buffer "\
						"(asked for %ld bytes)\n", args[thread_index].ir_readlen);
					return -1;
				}
				/* / 3 + 128 was some simple math recommended by LZMA SDK? */
				args[thread_index].outbuf_len = args[thread_index].ir_readlen + args[thread_index].ir_readlen / 3 + 128;
				args[thread_index].outbuf = malloc(args[thread_index].outbuf_len);
				if (args[thread_index].outbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate output buffer "\
						"(asked for %ld bytes)\n", args[thread_index].outbuf_len);
					return -1;
				}
			} else {
				args[thread_index].ir_readlen = COMP_THREAD_MAX_MEM;
				args[thread_index].inbuf = malloc(COMP_THREAD_MAX_MEM);
				if (args[thread_index].inbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate input buffer "\
						"(asked for %d bytes)\n", COMP_THREAD_MAX_MEM);
					return -1;
				}
				/* / 3 + 128 was some simple math recommended by LZMA SDK? */
				size_t max_outbuf_len = COMP_THREAD_MAX_MEM + COMP_THREAD_MAX_MEM / 3 + 128;
				args[thread_index].outbuf_len = max_outbuf_len;
				args[thread_index].outbuf = malloc(max_outbuf_len);
				if (args[thread_index].outbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate output buffer "\
						"(asked for %ld bytes)\n", max_outbuf_len);
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
		if (args[num_threads - 1].return_val != 0) {
			fprintf(stderr, "ERROR: thread failed to compress assigned chunk\n");
			return -1;
		}

		/* RT3: Wait for all the threads to finish their compression */
		for (int t = 0; t < num_threads - 1; t++) {
			pthread_join(thread_id[t], NULL);
			if (args[t].return_val != 0) {
				fprintf(stderr, "ERROR: thread failed to compress assigned chunk\n");
				return -1;
			}
		}

		/* MUTW: Make use of the Threads' Work */
		for (int t = 0; t < num_threads; t++) {
			/* MUTW1: Write the EC header of the current thread to the output file */
			if (0 != write_ec_header(&args[t].echeader, out_writer)) {
				fprintf(stderr, "ERROR: Could not write EC header to output file\n");
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

#if DEBUG_LEVEL >= 1
	fprintf(stderr, "(%d) STATUS: compression: \"%s\" has been compressed. " \
		"The result is stored at \"%s\"\n", getpid(), input_fp, \
		output_fp);
#endif

	return 0;
	/* }}} */
}


/** Takes an input file path, which points to a file compressed by
 * 'comp_file()' and uncompresses it, writing the uncompressed result to the
 * file at the output file path.
 *
 * \param '*input_fp' the path to the input file.
 * \param '*output_fp' the path to the output file.
 * \return 0 upon success, and a negative int upon failure.
 */
int uncomp_file(char * input_fp, char * output_fp) {
	/* {{{ */
	struct stat s;
	stat(input_fp, &s);

	FILE * in_file = fopen(input_fp, "rb");
	if (in_file == NULL) {
		perror("fopen (uncomp_file)");
		return -1;
	}

	clear_file(output_fp);

	FILE * out_writer = fopen(output_fp, "ab");
	if (out_writer == NULL) {
		perror("fopen (uncomp_file)");
		return -1;
	}

	off_t bytes_left = s.st_size;
	off_t cur_pos = 0;
	char num_threads = COMP_MAX_THREADS;
	CompThreadArgs args[num_threads];
#if DEBUG_LEVEL >= 1
	long max_bytes_per_batch = COMP_THREAD_MAX_MEM * COMP_MAX_THREADS;
	short batch_index = 0;
#endif

#if DEBUG_LEVEL >= 1
	/* Print warning if the file will require more than one thread */
	if (s.st_size > COMP_THREAD_MAX_MEM) {
		fprintf(stderr, "(%d) WARNING: uncompression: File size is bigger than " \
			"the memory limit for one uncompression thread (%ld > %d bytes). " \
			"The file will be uncompressed across multiple threads.\n", \
			getpid(), s.st_size, COMP_THREAD_MAX_MEM);
	}
#endif

#if DEBUG_LEVEL >= 1
	/* If the size of the file is too large for all its data to be loaded into
	 * memory and uncompressed in one batch of threads */
	if (s.st_size > max_bytes_per_batch) {
	/* Print warning if the file will require more than one batch of threads */
	fprintf(stderr, "(%d) WARNING: uncompression: File size is bigger than " \
		"the memory limit for one threaded uncompression batch " \
		"(%ld > %ld bytes). The file will be uncompressed over multiple " \
		"multithreaded batches.\n", \
		getpid(), s.st_size, max_bytes_per_batch);
	}
#endif

	/* Go through the input file in batches, breaking into threads for each
	 * batch, each thread uncompressing an assigned part of the file before
	 * pthread_join'ing back to the main thread, which will then loop through
	 * that batch's uncompressed data, appending it to the output file. Note
	 * that before a batch can break into threads, it must be determine how
	 * many threads are needed. This is done by attempting to determine the
	 * work for a set maximum number of threads, and upon reaching the EOF when
	 * attempting to read a given EC header, capping the number of threads.
	 * The EC headers specify all the work parameters for the threads.
	 * On a smaller scale, we can say this all takes place in 3 broad stages:
	 * STA: Set Thread Arguments
	 * RT: Run Threads
	 * MUTW: Make use of the Threads' Work */
	while (bytes_left > 0) {

		num_threads = COMP_MAX_THREADS;

		/* STA: Set Thread Arguments */
		for (int thread_index = 0; thread_index < COMP_MAX_THREADS; thread_index++) {
			/* STA1: Read EC header for chunk */
			if (0 != read_ec_header(&args[thread_index].echeader, in_file)) {
				/* If the read failed because the end of the file was reached... */
				if (0 != feof(in_file)) {
					/* ... then we have already read the last chunk, and
					 * we should start running the threads immediately */
					num_threads = thread_index;
					break;
				} else {
					fprintf(stderr, "ERROR: couldn't read EC header from chunk\n");
					return -1;
				}
			}

			bytes_left -= EC_HEADER_SIZE;
			cur_pos += EC_HEADER_SIZE;

			/* STA2: Set up a reader for each thread such that it will read
			 * at the part of the file it is responsible for reading */
			args[thread_index].input_reader = fopen(input_fp, "rb");
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
				fprintf(stderr, "ERROR: could not allocate props "\
					"(asked for %d bytes)\n", LZMA_PROPS_SIZE);
				return -1;
			}
			/* STA4: Set the read length and allocate space for the input buffer */
			args[thread_index].ir_readlen = args[thread_index].echeader.proc_size;
			args[thread_index].inbuf = malloc(args[thread_index].ir_readlen);
			if (args[thread_index].inbuf == NULL) {
				fprintf(stderr, "ERROR: could not allocate input buffer "\
					"(asked for %ld bytes)\n", args[thread_index].ir_readlen);
				return -1;
			}
			/* STA5: If the processed data is compressed, allocate room for
			 * uncompressed data */
			if (args[thread_index].echeader.compressed == EC_COMPRESSED) {
				args[thread_index].outbuf_len = args[thread_index].echeader.orig_size;
				args[thread_index].outbuf = malloc(args[thread_index].echeader.orig_size);
				if (args[thread_index].outbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate output buffer "\
						"(asked for %ld bytes)\n", args[thread_index].echeader.orig_size);
					return -1;
				}
			}

			/* STA6: If the data for the current (ec header + processed data)
			 * chunk is compressed, and the compressed data is therefore
			 * preceded by LZMA props, then seeking to the next chunk
			 * must take into account the props bytes */
			size_t num_props_bytes = 0;
			if (args[thread_index].echeader.compressed == EC_COMPRESSED) {
				num_props_bytes = args[thread_index].props_len;
				bytes_left -= args[thread_index].props_len;
				cur_pos += args[thread_index].props_len;
			}

			/* STA7: Now that we have set the arguments for the current thread,
			 * move the file cursor to beginning of next (ec header +
			 * processed data) chunk (i.e. seek to the next chunk) for the
			 * setting the next thread's arguments */
			if (0 != fseek(in_file, \
				num_props_bytes + args[thread_index].echeader.proc_size, \
				SEEK_CUR)) {

				perror("fseek (uncomp_file)");
				return -1;
			}

			bytes_left -= args[thread_index].echeader.proc_size;
			cur_pos += args[thread_index].echeader.proc_size;
		}

#if DEBUG_LEVEL >= 1
	fprintf(stderr, "(%d) STATUS: uncompression: batch (%d/?) has " \
		"%d threads\n", getpid(), batch_index + 1, num_threads);
#endif

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
		/* RT2: Have this "thread" uncompress as well since otherwise it would
		 * be waiting idly */
		uncompress_chunk_of_file(&args[num_threads - 1]);
		if (args[num_threads - 1].return_val != 0) {
			fprintf(stderr, "ERROR: thread failed to uncompress assigned chunk\n");
			return -1;
		}

		/* RT3: Wait for all the threads to finish their compression */
		for (int t = 0; t < num_threads - 1; t++) {
			pthread_join(thread_id[t], NULL);
			if (args[t].return_val != 0) {
				fprintf(stderr, "ERROR: thread failed to uncompress assigned chunk\n");
				return -1;
			}
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
			if (args[t].echeader.compressed == EC_COMPRESSED) {
				free(args[t].outbuf);
			}
			/* Close open file streams */
			fclose(args[t].input_reader);
		}

#if DEBUG_LEVEL >= 1
		batch_index++;
#endif
	}

	fclose(out_writer);

	return 0;
	/* }}} */
}
