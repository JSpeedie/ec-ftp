#include <stdio.h>

#include "fileops.h"


/** Opens a file in writing mode, effectively clearing the file of all content.
 *
 * \param '*file' the file path for the file to be wiped
 * \return void
 */
void clear_file(char * file) {
	FILE* victim = fopen(file, "w");
	if (victim == NULL) {
		perror("fopen (clear_file)");
		return;
	}
	fclose(victim);

	return;
}


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
	size_t nmem_read = 0;
	if (num_bytes != (nmem_read = fread(ret, 1, num_bytes, f)) ) {
		fprintf(stderr, "ERROR: read_bytes(): could not read %lu bytes, was only able to read %lu\n", num_bytes, nmem_read);
		return -1;
	}

	return 0;
}
