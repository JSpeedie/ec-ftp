#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#if DEBUG_LEVEL >= 2
#include <sys/syscall.h>
#endif

#include "aes.h"
#include "enc.h"
#include "fileops.h"


/** Take a file name and returns a malloc'd string containing a temp file name
 * which has the encryption extension appended. The returned string must
 * be freed by the caller of this function.
 *
 * \param '*filename' the name of the file we wish to generate a temporary
 *     encryption name for.
 * \return a pointer to the temp name string on success, and NULL on failure.
 */
char * temp_encryption_name(char * filename) {
	/* {{{ */
	int e_ext_len = strlen(ENC_EXT);

	/* GTNE: Generate a temp name for our encrypted file */
	/* GTNE1: Calculate how much space we need for the file name for the
	 * encrypted file's file name and create a zero'd array for it. + 7 for
	 * 'mkstemp()' "-XXXXXX" */
	char *e_out_fp = calloc(strlen(filename) + e_ext_len + 7 + 1, sizeof(char));

	/* GTNE2: Fill the string with the filename + the encryption extension +
	 * the suffix characters 'mkstemp()' requires and will modify */
	// TODO: can this construction not be done faster or more efficiently?
	strncpy(&e_out_fp[0], filename, strlen(filename));
	strncat(&e_out_fp[0], ENC_EXT, e_ext_len + 1);
	strncat(&e_out_fp[0], "-XXXXXX", 8);

	/* GTNE3: Generate a temp name for our encrypted file with 'mkstemp()' */
	int r;
	if ( (r = mkstemp(e_out_fp)) == -1) return NULL;
	/* ... close the open FD and deleting the temp file created as we intend to
	 * use its name later */
	close(r);
	unlink(e_out_fp);

	return e_out_fp;
	/* }}} */
}


/* Computes a^b mod c */
uint64_t sq_mp(uint64_t a, uint64_t b, uint64_t c) {
    uint64_t r;
    uint64_t y = 1;

    while (b > 0) {
        r = b % 2;

        if (r == 1) {
            y = (y * a) % c;
        }

        a = a * a % c;
        b = b / 2;
    }

    return y;
}

void to_column_order(uint8_t text[16]) {
    uint8_t temp[16];
    memcpy(temp, text, 16);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            text[4*j + i] = temp[4*i + j];
        }
    }
}

void to_row_order(uint8_t text[16]) {
    uint8_t temp[16];
    memcpy(temp, text, 16);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            text[4*i + j] = temp[4*j + i];
        }
    }
}


/** Helper function for encrypting a file through multiple threads */
void *encrypt_chunk_of_file(void *arg) {
	/* {{{ */
#if DEBUG_LEVEL >= 2
	fprintf(stderr, "(%d) STATUS: encryption: thread %ld starting...\n", \
		getpid(), syscall(SYS_gettid));
#endif
	struct enc_thread_args *t = (struct enc_thread_args *) arg;

	/* 1. Read the assigned number of bytes from the given (and already
	 * positioned) file stream */
	if (0 != read_bytes(&t->inbuf[0], t->ir_readlen, t->input_reader)) {
		fprintf(stderr, "ERROR: thread couldn't read assigned number of bytes from given input file stream\n");
		t->return_val = -1;
		return NULL;
	}

	/* 2. Encrypt the Data: Encrypt the data in 't->inbuf' and put in
	 * 't->outbuf' */
	int num_stranded_bytes = t->ir_readlen % 16;
    uint8_t text[16];
	int i;
	/* ED1: Loop through the read bytes in 16 byte chunks, encrypting each chunk and
	 * storing the results to a write buffer */
	for (i = 0; (size_t) i < t->ir_readlen - num_stranded_bytes; i += 16) {
		memcpy(text, &t->inbuf[i], 16);
		to_column_order(text);
		encrypt(text, t->aes_vars->rkeys, t->aes_vars->sbox);
		memcpy(&t->outbuf[i], text, 16);
	}

	/* ED2: Pad the encrypted data to a boundary of 16 bytes. If the data
	 * already has a size that is a multiple of 16, note that it is not
	 * padded so the encrypted file can be padded correctly at the end. */
	/* ED2a: If the number of read bytes was not a multiple of 16, then take
	 * the last few (stranded) bytes that don't form a 16 byte chunk and pad
	 * them... */
	if (num_stranded_bytes != 0) {
		memcpy(text, &t->inbuf[i], num_stranded_bytes);

		/* Pad the stranded bytes with redundant chars all representing the
		 * number of padding bytes needed to make the current chunk 16 bytes
		 * long */
		char padnum = 16 - num_stranded_bytes;
		for (int j = num_stranded_bytes; j < 16; j++) {
			text[j] = padnum;
		}
		/* Extend the length of outbuf into the safety bytes of the buffer */
		t->outbuf_len += padnum;

		to_column_order(text);
		encrypt(text, t->aes_vars->rkeys, t->aes_vars->sbox);
		memcpy(&t->outbuf[i], text, 16);
		t->padded = 1;
	/* ED2b: If the last 16 byte chunk did not need padding, note that
	 * it was not padded. */
	} else {
		t->padded = 0;
	}

#if DEBUG_LEVEL >= 2
	fprintf(stderr, "(%d) STATUS: encryption: thread %ld finished successfully\n", \
		getpid(), syscall(SYS_gettid));
#endif

	t->return_val = 0;
	return NULL;
	/* }}} */
}


int enc_file(char *input_fp, char *output_fp, uint32_t key[4]) {
	long max_bytes_per_batch = ENC_THREAD_MAX_MEM * ENC_MAX_THREADS;
    FILE *out_stream;

	struct stat s;
	unsigned int num_batches = 1;
	stat(input_fp, &s);

#if DEBUG_LEVEL >= 1
	/* Print warning if the file will require more than one thread */
	if (s.st_size > ENC_THREAD_MAX_MEM) {
		fprintf(stderr, "(%d) WARNING: encryption: File size is bigger than " \
			"the memory limit for one encryption thread (%ld > %d bytes). " \
			"The file will be encrypted across multiple threads.\n", \
			getpid(), s.st_size, ENC_THREAD_MAX_MEM);
	}
#endif

	/* If the size of the file is too large for all its data to be loaded into
	 * memory and encrypted in one batch of threads */
	if (s.st_size > max_bytes_per_batch) {
#if DEBUG_LEVEL >= 1
	/* Print warning if the file will require more than one batch of threads */
	fprintf(stderr, "(%d) WARNING: encryption: File size is bigger than " \
		"the memory limit for one threaded encryption batch " \
		"(%ld > %ld bytes). The file will be encrypted over multiple " \
		"multithreaded batches.\n", \
		getpid(), s.st_size, max_bytes_per_batch);
#endif
		/* "Ceiled" division so that the number of jobs is always sufficient
		 * to encrypt the whole file */
		num_batches = (s.st_size / max_bytes_per_batch) + 1;
	}

	if ((out_stream = fopen(output_fp, "wb")) == NULL) {
		return -1;
	}

	/* Fill in the enc_aes_vars struct which will be used for all threads */
	struct enc_aes_vars avars;
	initialize_aes_sbox(avars.sbox, avars.sboxinv);
	expkey(avars.rkeys, key, avars.sbox);

	char num_threads = ENC_MAX_THREADS;
	struct enc_thread_args args[num_threads];
	uint64_t batch_len;

	/* Go through the input file in batches, breaking into threads for each
	 * batch, each thread encrypting an assigned part of the file before
	 * pthread_join'ing back to the main thread, which will then loop through
	 * the encryped data, writing it (or the raw data if it takes up less
	 * space) to the output file. This all takes place in 3 broad stages:
	 * STA: Set Thread Arguments
	 * RT: Run Threads
	 * MUTW: Make use of the Threads' Work */
	for (unsigned int batch_index = 0; batch_index < num_batches; batch_index++) {
		/* If this is not the last batch, read a batch of maximum length with
		 * the maximum number of threads */
		if (batch_index != num_batches - 1) {
			batch_len = max_bytes_per_batch;
			num_threads = ENC_MAX_THREADS;
		} else {
			/* Set the number of threads to how many 'ENC_THREAD_MAX_MEM' byte
			 * chunks of the file are left */
			batch_len = s.st_size - (batch_index * max_bytes_per_batch);
			num_threads = (batch_len / ENC_THREAD_MAX_MEM) + 1;
		}

#if DEBUG_LEVEL >= 1
	fprintf(stderr, "(%d) STATUS: encryption: batch (%d/%d) has %d threads\n", \
		getpid(), batch_index + 1, num_batches, num_threads);
#endif

		/* STA: Set Thread Arguments */

		/* STA1: Open up the input file with 'num_batches' readers, each at their
		* starting location for the reading they have to do */
		for (int thread_index = 0; thread_index < num_threads; thread_index++) {
			args[thread_index].input_reader = fopen(input_fp, "rb");
			if (args[thread_index].input_reader == NULL) {
				perror("fopen (enc_file)");
				return -1;
			}
			/* STA2: Position the cursor of each reader such that it will
			 * read at the part of the file it is responsible for reading */
			fseek(args[thread_index].input_reader, \
				(batch_index * max_bytes_per_batch) \
				+ (thread_index * ENC_THREAD_MAX_MEM), \
				SEEK_SET);
			/* STA3: Set the thread's AES vars */
			args[thread_index].aes_vars = &avars;

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
					- (thread_index * ENC_THREAD_MAX_MEM); // If this is the last thread, read only the remaining bytes of the file
				args[thread_index].inbuf = malloc(args[thread_index].ir_readlen);
				if (args[thread_index].inbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate input buffer "\
						"(asked for %ld bytes)\n", args[thread_index].ir_readlen);
					return -1;
				}
				/* Make sure outbuf length is rounded up to a multiple of 16
				 * to ensure that there is room for internal padding */
				int num_stranded_bytes = args[thread_index].ir_readlen % 16;
				long alloc_len = 0;
				args[thread_index].outbuf_len = args[thread_index].ir_readlen;

				/* Allocate for 'outbuf' either 'ir_readlen' bytes
				 * (if 'num_stranded_bytes' == 0) or 'ir_readlen + (16 -
				 * num_stranded_bytes)' bytes (if 'num_stranded_bytes' != 0) */
				alloc_len = args[thread_index].ir_readlen \
					+ ((num_stranded_bytes != 0) * (16 - num_stranded_bytes));
				args[thread_index].outbuf = malloc(alloc_len);
				if (args[thread_index].outbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate output buffer "\
						"(asked for %ld bytes)\n", alloc_len);
					return -1;
				}
			} else {
				args[thread_index].ir_readlen = ENC_THREAD_MAX_MEM;
				args[thread_index].inbuf = malloc(ENC_THREAD_MAX_MEM);
				if (args[thread_index].inbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate input buffer "\
						"(asked for %d bytes)\n", ENC_THREAD_MAX_MEM);
					return -1;
				}
				size_t max_outbuf_len = ENC_THREAD_MAX_MEM;
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
				pthread_create(&thread_id[t], NULL, encrypt_chunk_of_file, &args[t])) {

				fprintf(stderr, "ERROR: Could not create threads\n");
				return -1;
			}
		}
		/* RT2: Have this "thread" encrypt as well since otherwise it would be
		 * waiting idly */
		encrypt_chunk_of_file(&args[num_threads - 1]);
		if (args[num_threads - 1].return_val != 0) {
			fprintf(stderr, "ERROR: thread failed to encrypt assigned chunk\n");
			return -1;
		}

		/* RT3: Wait for all the threads to finish their encryption */
		for (int t = 0; t < num_threads - 1; t++) {
			pthread_join(thread_id[t], NULL);
			if (args[t].return_val != 0) {
				fprintf(stderr, "ERROR: thread failed to encrypt assigned chunk\n");
				return -1;
			}
		}

		/* MUTW: Make use of the Threads' Work */
		for (int t = 0; t < num_threads; t++) {
			/* MUTW1: Write the encrypted data in 'outbuf' to the output file */
			if (1 != \
				/* Write the outbuf to the output file */
				fwrite(args[t].outbuf, args[t].outbuf_len, 1, out_stream)) {

				fprintf(stderr, "ERROR: Could not write data content output file\n");
				return -1;
			}
			/* MUTW2: If this is the thread working on the data right at the
			 * end of the input file, handle padding */
			if (batch_index == num_batches - 1 && t == num_threads - 1) {
				/* If the final chunk is not already internally padded, add a
				 * padding chunk */
				if (args[t].padded != 1) {
					uint8_t text[16];

					for (int j = 0; j < 16; j++) {
						text[j] = 16;
					}

					encrypt(text, args[t].aes_vars->rkeys, args[t].aes_vars->sbox);

					if (1 != \
						/* Write the padding chunk */
						fwrite(&text[0], 16, 1, out_stream)) {

						fprintf(stderr, "ERROR: Could not write data content output file\n");
						return -1;
					}
				}
			}

			/* Free dynamically alloc'd buffers */
			free(args[t].inbuf);
			free(args[t].outbuf);
			/* Close open file streams */
			fclose(args[t].input_reader);
		}
	
	}

	fclose(out_stream);

#if DEBUG_LEVEL >= 1
	fprintf(stderr, "(%d) STATUS: encryption: \"%s\" has been encrypted. " \
		"The result is stored at \"%s\"\n", getpid(), input_fp, \
		output_fp);
#endif

	return 0;
}


void *decrypt_chunk_of_file(void *arg) {
	/* {{{ */
#if DEBUG_LEVEL >= 2
	fprintf(stderr, "(%d) STATUS: decryption: thread %ld starting...\n", \
		getpid(), syscall(SYS_gettid));
#endif
	struct enc_thread_args *t = (struct enc_thread_args *) arg;

	/* 1. Read the assigned number of bytes from the given (and already
	 * positioned) file stream */
	if (0 != read_bytes(&t->inbuf[0], t->ir_readlen, t->input_reader)) {
		fprintf(stderr, "ERROR: thread couldn't read assigned number of bytes from given input file stream\n");
		t->return_val = -1;
		return NULL;
	}

	/* 2. DD: Decrypt the Data. Decrypt the data in 't->inbuf' and put in
	 * 't->outbuf' */
	int num_stranded_bytes = t->ir_readlen % 16;
    uint8_t text[16];
	/* DD1: Loop through the read bytes in 16 byte chunks, decrypting each chunk and
	 * storing the results to a write buffer */
	for (int i = 0; (size_t) i < t->ir_readlen - num_stranded_bytes; i += 16) {
		memcpy(text, &t->inbuf[i], 16);
		decrypt(text, t->aes_vars->rkeys, t->aes_vars->sboxinv);
		to_row_order(text);
		memcpy(&t->outbuf[i], text, 16);
		/* DD2: Trim decrypted data if the data is padded and we are on
		 * the last chunk */
		if (t->padded == 1 && (size_t) i == t->ir_readlen - 16) {
			unsigned char num_pad_bytes = t->outbuf[t->ir_readlen - 1];
			t->outbuf_len = t->ir_readlen - num_pad_bytes;
		}
	}

#if DEBUG_LEVEL >= 2
	fprintf(stderr, "(%d) STATUS: decryption: thread %ld finished successfully\n", \
		getpid(), syscall(SYS_gettid));
#endif

	t->return_val = 0;
	return NULL;
	/* }}} */
}


int dec_file(char *input_fp, char *output_fp, uint32_t key[4]) {
	long max_bytes_per_batch = ENC_THREAD_MAX_MEM * ENC_MAX_THREADS;
	FILE *out_stream;

	struct stat s;
	unsigned int num_batches = 1;
	stat(input_fp, &s);


#if DEBUG_LEVEL >= 1
	/* Print warning if the file will require more than one thread */
	if (s.st_size > ENC_THREAD_MAX_MEM) {
		fprintf(stderr, "(%d) WARNING: decryption: File size is bigger than " \
			"the memory limit for one decryption thread (%ld > %d bytes). " \
			"The file will be decrypted across multiple threads.\n", \
			getpid(), s.st_size, ENC_THREAD_MAX_MEM);
	}
#endif

	/* If the size of the file is too large for all its data to be loaded into
	 * memory and decrypted in one batch of threads */
	if (s.st_size > max_bytes_per_batch) {
#if DEBUG_LEVEL >= 1
	/* Print warning if the file will require more than one batch of threads */
	fprintf(stderr, "(%d) WARNING: decryption: File size is bigger than " \
		"the memory limit for one threaded decryption batch " \
		"(%ld > %ld bytes). The file will be decrypted over multiple " \
		"multithreaded batches.\n", \
		getpid(), s.st_size, max_bytes_per_batch);
#endif
		/* "Ceiled" division so that the number of jobs is always sufficient
		 * to decrypt the whole file */
		num_batches = (s.st_size / max_bytes_per_batch) + 1;
	}

	if ((out_stream = fopen(output_fp, "wb")) == NULL) {
		return -1;
	}

	/* Fill in the enc_aes_vars struct which will be used for all threads */
	struct enc_aes_vars avars;
	initialize_aes_sbox(avars.sbox, avars.sboxinv);
	expkey(avars.rkeys, key, avars.sbox);

	char num_threads = ENC_MAX_THREADS;
	struct enc_thread_args args[num_threads];
	uint64_t batch_len;

	/* Read ENC_THREAD_MAX_MEM bytes from the file, decrypting in 16 byte chunks */
	/* Go through the input file in batches, breaking into threads for each
	 * batch, each thread decrypting an assigned part of the file before
	 * pthread_join'ing back to the main thread, which will then loop through
	 * the encryped data, writing it (or the raw data if it takes up less
	 * space) to the output file. This all takes place in 3 broad stages:
	 * STA: Set Thread Arguments
	 * RT: Run Threads
	 * MUTW: Make use of the Threads' Work */
	for (unsigned int batch_index = 0; batch_index < num_batches; batch_index++) {
		/* If this is not the last batch, read a batch of maximum length with
		 * the maximum number of threads */
		if (batch_index != num_batches - 1) {
			batch_len = max_bytes_per_batch;
			num_threads = ENC_MAX_THREADS;
		} else {
			/* Set the number of threads to how many 'ENC_THREAD_MAX_MEM' byte
			 * chunks of the file are left */
			batch_len = s.st_size - (batch_index * max_bytes_per_batch);
			num_threads = (batch_len / ENC_THREAD_MAX_MEM) + 1;
		}

#if DEBUG_LEVEL >= 1
	fprintf(stderr, "(%d) STATUS: decryption: batch (%d/%d) has %d threads\n", \
		getpid(), batch_index + 1, num_batches, num_threads);
#endif

		/* STA: Set Thread Arguments */

		/* STA1: Open up the input file with 'num_batches' readers, each at their
		* starting location for the reading they have to do */
		for (int thread_index = 0; thread_index < num_threads; thread_index++) {
			args[thread_index].input_reader = fopen(input_fp, "rb");
			if (args[thread_index].input_reader == NULL) {
				perror("fopen (dec_file)");
				return -1;
			}
			/* STA2: Position the cursor of each reader such that it will
			 * read at the part of the file it is responsible for reading */
			fseek(args[thread_index].input_reader, \
				(batch_index * max_bytes_per_batch) \
				+ (thread_index * ENC_THREAD_MAX_MEM), \
				SEEK_SET);
			/* STA3: Set the thread's AES vars */
			args[thread_index].aes_vars = &avars;

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
					- (thread_index * ENC_THREAD_MAX_MEM); // If this is the last thread, read only the remaining bytes of the file
				args[thread_index].inbuf = malloc(args[thread_index].ir_readlen);
				if (args[thread_index].inbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate input buffer "\
						"(asked for %ld bytes)\n", args[thread_index].ir_readlen);
					return -1;
				}
				/* Outbuf will be >= inbuf length when decrypting, so
				 * simply allocate inbuf length for outbuf */
				args[thread_index].outbuf_len = args[thread_index].ir_readlen;
				args[thread_index].outbuf = malloc(args[thread_index].outbuf_len);
				if (args[thread_index].outbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate output buffer "\
						"(asked for %ld bytes)\n", args[thread_index].outbuf_len);
					return -1;
				}
				/* If this is the last thread of the last batch, we need
				 * to remove the padding */
				args[thread_index].padded = 1;
			} else {
				args[thread_index].ir_readlen = ENC_THREAD_MAX_MEM;
				args[thread_index].inbuf = malloc(ENC_THREAD_MAX_MEM);
				if (args[thread_index].inbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate input buffer "\
						"(asked for %d bytes)\n", ENC_THREAD_MAX_MEM);
					return -1;
				}
				size_t max_outbuf_len = ENC_THREAD_MAX_MEM;
				args[thread_index].outbuf_len = max_outbuf_len;
				args[thread_index].outbuf = malloc(max_outbuf_len);
				if (args[thread_index].outbuf == NULL) {
					fprintf(stderr, "ERROR: could not allocate output buffer "\
						"(asked for %ld bytes)\n", max_outbuf_len);
					return -1;
				}
				/* Since this is NOT the last thread of the last batch, there
				 * is no need to remove padding */
				args[thread_index].padded = 0;
			}
		}

		/* RT: Run Threads */
		pthread_t thread_id[num_threads];
		/* RT1: Create threads with their given tasks/arguments */
		for (int t = 0; t < num_threads - 1; t++) {
			if (0 != \
				pthread_create(&thread_id[t], NULL, decrypt_chunk_of_file, &args[t])) {

				fprintf(stderr, "ERROR: Could not create threads\n");
				return -1;
			}
		}
		/* RT2: Have this "thread" decrypt as well since otherwise it would be
		 * waiting idly */
		decrypt_chunk_of_file(&args[num_threads - 1]);
		if (args[num_threads - 1].return_val != 0) {
			fprintf(stderr, "ERROR: thread failed to decrypt assigned chunk\n");
			return -1;
		}

		/* RT3: Wait for all the threads to finish their decryption */
		for (int t = 0; t < num_threads - 1; t++) {
			pthread_join(thread_id[t], NULL);
			if (args[t].return_val != 0) {
				fprintf(stderr, "ERROR: thread failed to decrypt assigned chunk\n");
				return -1;
			}
		}

		/* MUTW: Make use of the Threads' Work */
		for (int t = 0; t < num_threads; t++) {
			/* MUTW1: Write the decrypted data in 'outbuf' to the output file */
			if (1 != \
				/* Write the outbuf to the output file */
				fwrite(args[t].outbuf, args[t].outbuf_len, 1, out_stream)) {

				fprintf(stderr, "ERROR: Could not write data content output file\n");
				return -1;
			}

			/* Free dynamically alloc'd buffers */
			free(args[t].inbuf);
			free(args[t].outbuf);
			/* Close open file streams */
			fclose(args[t].input_reader);
		}
	
	}

	fclose(out_stream);

	return 0;
}
