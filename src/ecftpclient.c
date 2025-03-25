#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "ecftp.h"


int get_user_input(char * buffer){
	//clear buffer
	memset(buffer, 0, (int) sizeof(buffer));

	//print the prompt
	printf("> ");

	// TODO: should this not loop? Currently it only ever reads a maximum of 1024 characters
	//get user input
	if (fgets(buffer, 1024, stdin) == NULL){
		return -1;
	}

	return 1;
}


// TODO: not sure if this function is endianness safe? I've cleaned it up,
// but the code is still the student code
/** Takes a port 'port' and modifies the uint8_ts at 'p1' and 'p2' such that the
 * former represents the 8 most significant bits of 'port' and the latter
 * represents the 8 least significant bits.
 * \param 'port' the port number.
 * \param '*p1' a pointer to a uint8_t that will be modified to contain the 8 most
 *     signifiant bits of 'port'.
 * \param '*p2' a pointer to a uint8_t that will be modified to contain the 8 least
 *     signifiant bits of 'port'.
 * \return void.
 */
void split_port(uint16_t port, uint8_t *p1, uint8_t *p2) {
	int x = 1;
	*p1 = 0;
	*p2 = 0;
	/* Set the 8 least significant bits of '(*p2)' to the
	   8 least significant bits of 'port' */
	for (int i = 0; i < 8; i++) {
		*p2 = (*p2)|(port & x);
		x = x << 1;
	}

	/* Set the 8 least significant bits of '(*p1)' to the
	   8 most significant bits of 'port' */
	port = port >> 8;
	x = 1;

	for (int i = 0; i < 8; i++) {
		*p1 = (*p1)|(port & x);
		x = x << 1;
	}
}


/* Takes a string representing an ip address and two ints representing the 8
 * least and 8 most significant bits in a port number and modifies 'str' to
 * contain a port command of the format specified by FTP.
 *
 * \param '*str' a string which will be modified to contain the PORT command.
 *     It must be at least 29 bytes long.
 * \param '*ip' a pointer to string representation of the ip address of the
 *     client.
 * \param 'p1' an int containing in its 8 least significant bits the 8 most
 *     significant bits of the port number.
 * \param 'p2' an int containing in its 8 least significant bits the 8 least
 *     significant bits of the port number.
 * \return void.
 */
void generate_port_command(char *str, char *ip, uint16_t port) {
	/* Generate the port command, as specified at page 28 of:
	 * https://www.ietf.org/rfc/rfc959.txt */

	int i = 0;
	char ip_temp[INET_ADDRSTRLEN];
	strncpy(ip_temp, ip, INET_ADDRSTRLEN);

	int ip_len = strlen(ip);

	for (i = 0; i < ip_len; i++){
		if (ip_temp[i] == '.'){
			ip_temp[i] = ',';
		}
	}

	uint8_t p1;
	uint8_t p2;
	split_port(port, &p1, &p2);

	sprintf(str, "PORT %s,%d,%d", ip_temp, p1, p2);
}


// TODO: what does this function do? my best guess right now
// is that it returns 1 if the number of whitespace characters in '*command'
// is <= 1.
int check_command(char *command){
	int len = strlen(command);
	int space = FALSE;
	int count = 0;

	for (int i = 0; i < len; i++) {
		if (isspace(command[i]) == 0) {
			space = FALSE; // TODO: space is first instantiated to FALSE and is never set to TRUE, so what is the point of this?
			continue; // TODO: is this continue necessary?
		} else {
			if (space == FALSE) { // TODO: space is first instantiated to FALSE and is never set to TRUE, so what is the point of this?
				count++;
			}
		}
	}

	if (count <= 1) {
		return 1;
	} else {
		return -1;
	}
}


// TODO: what does this function do? my best guess right now: ...
int get_command(char *command){
	int value, check = -1;
	char copy[1024];

	while (check < 0) {
		char *str;
		if (get_user_input(command) < 0) {
			printf("Cannot Read Command...\nPlease Try Again...\n");
			bzero(command, (int)sizeof(command));
			continue;
		}

		if (strlen(command) < 2) {
			printf("No Input Detected...\nPlease Try Again\n");
			bzero(command, (int)sizeof(command));
			continue;
		}

		trim(command);
		strcpy(copy, command);

		if (check_command(copy) < 0) {
			printf("Invalid Format...\nPlease Try Again...\n");
			bzero(command, (int)sizeof(command));
			bzero(copy, (int)sizeof(copy));
			continue;
		}

		char delimit[]=" \t\r\n\v\f";
		/* Get the first token from the command */
		str = strtok(copy, delimit);
		/* If that token is a valid FTP command */
		// TODO: these should be strncmp
		if ((strcmp(str, "ls") == 0) || (strcmp(str, "get") == 0) \
			|| (strcmp(str, "put") == 0) || (strcmp(str, "quit") == 0)) {

			check = 1;

			//populated value valriable to indicate back to main which input was entered
			// TODO: these should be strncmp
			if(strcmp(str, "ls") == 0){value = 1;}
			else if(strcmp(str, "get") == 0){value = 2;}
			else if(strcmp(str, "put") == 0){value = 3;}
			else if(strcmp(str, "quit") == 0){value = 4;}
		}else{
			printf("Incorrect Command Entered...\nPlease Try Again...\n");
			bzero(command, strlen(command));
			bzero(copy, sizeof(copy));
			continue;
		}
	}
	return value;
}


// TODO: clean up this function, add brief documentation
int get_filename(char *input, char *fileptr){
    char cpy[1024];
    char *filename = NULL;
    strcpy(cpy, input);
    trim(cpy);
    filename = strtok(cpy, " ");
    filename = strtok(NULL, " ");

    if(filename == NULL){
        fileptr = "\0";
        return -1;
    }else{
        strncpy(fileptr, filename, strlen(filename));
        return 1;
    }
}


/** Perform the necessary operations to enact the QUIT FTP service command.
 *
 * \param 'controlfd' a file descriptor representing the control connection
 *     of the FTP.
 * \return 0 upon success, a negative int upon failure.
 */
int do_quit(int controlfd) {
	char quit[1024];
	sprintf(quit, "QUIT");
	int quit_len = strlen(quit);

	ssize_t nbytes = write(controlfd, quit, quit_len);
	if ((long long) nbytes < (long long) quit_len) {
		return -1;
	}

	bzero(quit, (int)sizeof(quit));
	// TODO: check for errors on read?
	read(controlfd, quit, 1024);
	printf("Server Response: %s\n", quit);

	return 0;
}


// TODO: possibly break up this function, add brief documentation
int do_ls(int controlfd, int datafd, char *input){

    char filelist[256], str[MAXLINE+1], recvline[MAXLINE+1], *temp;
    bzero(filelist, (int)sizeof(filelist));
    bzero(recvline, (int)sizeof(recvline));
    bzero(str, (int)sizeof(str));

    fd_set rdset;
    int maxfd, data_finished = FALSE, control_finished = FALSE;

    if(get_filename(input, filelist) < 0){
#if DEBUG_LEVEL >= 1
	fprintf(stdout, "No input filelist detected...\n");
#endif
        sprintf(str, "LIST");
    }else{
        sprintf(str, "LIST %s", filelist);
    }

    bzero(filelist, (int)sizeof(filelist));

    FD_ZERO(&rdset);
    FD_SET(controlfd, &rdset);
    FD_SET(datafd, &rdset);

    if(controlfd > datafd){
        maxfd = controlfd + 1;
    }else{
        maxfd = datafd + 1;
    }

    write(controlfd, str, strlen(str));

    while(1){
        if(control_finished == FALSE){FD_SET(controlfd, &rdset);}
        if(data_finished == FALSE){FD_SET(datafd, &rdset);}
        select(maxfd, &rdset, NULL, NULL, NULL);

        if(FD_ISSET(controlfd, &rdset)){
            read(controlfd, recvline, MAXLINE);
			// TODO: I think this always prints the like, http code e.g. "200 Command OK"
			printf("Server Response: %s\n", recvline);
            temp = strtok(recvline, " ");
            if(atoi(temp) != 200){
                printf("Exiting...\n");
                break;
            }
            control_finished = TRUE;
            bzero(recvline, (int)sizeof(recvline));
            FD_CLR(controlfd, &rdset);
        }

        if(FD_ISSET(datafd, &rdset)){
#if DEBUG_LEVEL >= 1
	fprintf(stdout, "Server Data Response:\n");
#endif
            while(read(datafd, recvline, MAXLINE) > 0){
                printf("%s", recvline);
                bzero(recvline, (int)sizeof(recvline));
            }

            data_finished = TRUE;
            FD_CLR(datafd, &rdset);
        }

        if((control_finished == TRUE) && (data_finished == TRUE)){
            break;
        }

    }
    bzero(filelist, (int)sizeof(filelist));
    bzero(recvline, (int)sizeof(recvline));
    bzero(str, (int)sizeof(str));
    return 1;
}


// TODO: break up this function, add brief documentation
int do_get(int controlfd, int datafd, char *input) {
	char filename[256], serv_cmd[MAXLINE+1], recvline[MAXLINE+1];
	// TODO:do we have to bzero the whole string, for any of these strings? Can
	// we not just make the 0th element = \0?
	bzero(filename, (int)sizeof(filename));
	bzero(recvline, (int)sizeof(recvline));
	bzero(serv_cmd, (int)sizeof(serv_cmd));
	int err = 0;

	if (get_filename(input, filename) < 0) {
		printf("No filename Detected...\n");
		char send[1024];
		sprintf(send, "SKIP");
		write(controlfd, send, strlen(send));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Response: %s\n", send);
		return -1;
	}

	/* Construct FTP service command and send it over the control connection */
	sprintf(serv_cmd, "RETR %s", filename);
	write(controlfd, serv_cmd, strlen(serv_cmd));

	/* CSCD58 addition - Encryption */
	uint32_t key[4];
	do_dh_client(controlfd, datafd, key);
	/* CSCD58 end of addition - Encryption */

	/* CSCD58 addition - Compression */
	/* Make temporary name for receiving file
	 * ( '<filename>.comp.enc-XXXXXX' ) */
	char * recv_fp;
	if ( (recv_fp = temp_recv_name(filename)) == NULL) {
		fprintf(stderr, "ERROR: failed to receive file!\n");
		return -1;
	}
	/* CSCD58 end of addition - Compression */

	FILE *recv_file_stream;

	if ((recv_file_stream = fopen(recv_fp, "w")) == NULL) {
		perror("file error");
		return -1;
	}

	int read_len = 0;

	/* Receive data from the server, writing it to the receive file */
	while (1) {
		read_len = read(datafd, recvline, MAXLINE);
		/* If there was an error */
		if (read_len < 0) {
			err = 1;
			break;
		/* If something was read, write it to the receiving file */
		} else if (read_len > 0) {
			fwrite(recvline, 1, read_len, recv_file_stream);
			bzero(recvline, (int)sizeof(recvline));
		/* If nothing was read, stop receiving */
		} else {
			break;
		}
	}
	fclose(recv_file_stream);
	printf("File received\n"); // TODO: remove

	/* Read server response - did the server successfully send the file? */
	char serv_resp[1024];
	bzero(serv_resp, (int)sizeof(serv_resp));
	// TODO: check for errors on read?
	read(controlfd, serv_resp, 1024);
	printf("Server Response: %s\n", serv_resp);

	/* If there was an error receiving the file, delete the temp file it was to
	 * be written to */
	if (err) {
		if (0 != remove(recv_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary file following an error!\n");
		}
		return -1;
	}

	/* CSCD58 addition - Compression + Encryption */
	/* Decrypt (using key 'key') and decompress received file stored at the
	 * filepath 'recv_fp', outputting the result to the file at path
	 * 'filename' */
	if (process_received_file(filename, recv_fp, key) != 0) {
		fprintf(stderr, "ERROR: failed to process received file!\n");
		return -1;
	}

	/* Free dynamically allocated memory */
	free(recv_fp);
	/* CSCD58 end of addition - Compression + Encryption */

	printf("File processed\n"); // TODO: remove

	return 1;
}


// TODO: break up this function, add brief documentation
int do_put(int controlfd, int datafd, char *input) {
	char filename[256];
	char str[MAXLINE+1];
	char recvline[MAXLINE+1];
	char sendline[MAXLINE+1];
	char serv_cmd[MAXLINE+1];
	char *temp;
	bzero(filename, (int)sizeof(filename));
	bzero(recvline, (int)sizeof(recvline));
	bzero(str, (int)sizeof(str));

	fd_set wrset, rdset;
	int maxfd;
	int data_finished = FALSE;
	int control_finished = FALSE;

	if (get_filename(input, filename) < 0) {
		printf("No filename Detected...\n");
		char send[1024];
		sprintf(send, "SKIP");
		write(controlfd, send, strlen(send));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Control Response: %s\n", send);
		return -1;
	}

	/* Prepare STOR and send the command */
	sprintf(serv_cmd, "STOR %s", filename);
	write(controlfd, serv_cmd, strlen(serv_cmd));

	/* select() initialization */
	FD_ZERO(&wrset);
	FD_SET(datafd, &wrset);
	FD_ZERO(&rdset);
	FD_SET(controlfd, &rdset);
	fd_set wrset_stable = wrset; // Make a backup of each set since
	fd_set rdset_stable = rdset; // select() is destructive

	/* Get the max file descripter number for select() */
	if(controlfd > datafd){
		maxfd = controlfd + 1;
	}else{
		maxfd = datafd + 1;
	}

	/* CSCD58 addition - Encryption */
	uint32_t key[4];
	do_dh_client(controlfd, datafd, key);
	/* CSCD58 end of addition - Encryption */

	/* CSCD58 addition - Compression + Encryption */
	char * prepared_fp;

	/* Encrypt (using key 'key') and compress the file stored at the
	 * filepath 'filename', outputting the result to the file at path
	 * 'prepared_fp' */
	if (0 != prepare_file(filename, key, &prepared_fp)) {
		fprintf(stderr, "ERROR: could not prepare file!\n");
		char send[1024];
		sprintf(serv_cmd, "SKIP");
		write(controlfd, serv_cmd, strlen(serv_cmd));
		bzero(send, (int)sizeof(send));
		read(controlfd, send, 1000);
		printf("Server Control Response: %s\n", send);
		return -1;
	}
	/* CSCD58 end of addition - Compression + Encryption */

	FILE *in;

	if ((in = fopen(prepared_fp, "rb")) == NULL) {
		fprintf(stderr, "ERROR: could not read file that is to be sent!\n");
		return -1;
	}

	while (1) {
		/* Recreate the FD sets */
		wrset = wrset_stable;
		rdset = rdset_stable;

		select(maxfd, &rdset, &wrset, NULL, NULL);

		if (FD_ISSET(controlfd, &rdset)) {
			bzero(recvline, (int)sizeof(recvline));
			read(controlfd, recvline, MAXLINE);
			printf("Server Control Response: %s\n", recvline);
			temp = strtok(recvline, " ");
			if (atoi(temp) != 200) {
				printf("File Error...\nExiting...\n");
				break;
			}
			control_finished = TRUE;
			bzero(recvline, (int)sizeof(recvline));
			FD_CLR(controlfd, &rdset);
		}

		if (FD_ISSET(datafd, &wrset)) {
			bzero(sendline, (int)sizeof(sendline));
			/* CSCD58 addition */
			size_t nmem_read = 0;
			while (0 != (nmem_read = fread(sendline, 1, sizeof(sendline), in)) ) {
				write(datafd, sendline, nmem_read);
				bzero(sendline, (size_t)sizeof(sendline));
			}
			/* CSCD58 end of addition */

			data_finished = TRUE;
			FD_CLR(datafd, &wrset);
			close(datafd);
		}
		if ((control_finished == TRUE) && (data_finished == TRUE)) {
			break;
		}
	}
	/* CSCD58 addition - Compression */
	if (KEEP_TEMP_ENC_FILES != 1) {
		if (0 != remove(prepared_fp)) {
			fprintf(stderr, "WARNING: could not remove temporary encrypted .enc file!\n");
		}
	}

	/* Note that equivalent "KEEP" check for the temporary compressed
	 * file is performed in prepare_file() */

	/* Close open files */
	fclose(in);
	/* Free dynamically allocated memory */
	free(prepared_fp);
	/* CSCD58 end of addition - Compression */

	return 1;
}


/* Set up control connection */
int setup_control_conn(int * controlfd, struct sockaddr_in * serv_addr, \
	char * ip_addr, uint16_t port) {

	/* Create a control socket */
	if ( ( (*controlfd) = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		return -1;
	}

	/* Create the configuration for the control connection */
	bzero(serv_addr, sizeof( (*serv_addr) ));
	serv_addr->sin_family = AF_INET;
	serv_addr->sin_port = htons(port);
	if (inet_pton(AF_INET, ip_addr, &(serv_addr->sin_addr)) <= 0) {
		return -2;
	}

	/* Connect the control socket to the server (using the configuration) */
	if (connect( (*controlfd), (struct sockaddr *) serv_addr, sizeof( (*serv_addr) )) < 0) {
		return -3;
	}

	return 0;
}


/* Set up data connection */
int setup_data_conn(int * listenfd, struct sockaddr_in * data_addr, \
	uint16_t port) {

	/* Create a data socket for listening */
	if (( (*listenfd) = socket(AF_INET, SOCK_STREAM, 0) ) < 0) {
		return -1;
	}

	/* Create the configuration for the data connection */
	bzero(data_addr, sizeof( (*data_addr) ));
	data_addr->sin_family = AF_INET;
	data_addr->sin_addr.s_addr = htonl(INADDR_ANY);
	data_addr->sin_port = htons(port);

	/* Bind the data socket (apply configuration) */
	if ( bind( (*listenfd), (struct sockaddr *) data_addr, sizeof( (*data_addr) )) < 0) {
		return -2;
	}

	/* Set the socket as passive (one that listens) with a backlog of
	 * 'LISTENQ' */
	if ( listen( (*listenfd), LISTENQ) < 0) {
		return -3;
	}

	return 0;
}


int main(int argc, char **argv) {
	int server_port, controlfd, listenfd, datafd, cmd;
	struct sockaddr_in serv_addr, data_addr;
	char command[1024], ip[INET_ADDRSTRLEN], port_command[MAXLINE+1];

	if (argc != 3) {
		printf("Invalid Number of Arguments...\n");
		printf("Usage: ./ecftpclient <server-ip> <server-listen-port>\n");
		exit(-1);
	}

	/* Parse server address and port from commandline args */
	strncpy(ip, argv[1], INET_ADDRSTRLEN);
	sscanf(argv[2], "%d", &server_port);

	/* Setup control and data connections, as required by the FTP at page 8 of:
	 * https://www.ietf.org/rfc/rfc959.txt */
	if (setup_control_conn(&controlfd, &serv_addr, argv[1], server_port) < 0) {
		perror("control connection setup error");
		exit(-1);
	}
	// TODO: should the port be 0?
	if (setup_data_conn(&listenfd, &data_addr, 0) < 0) {
		perror("data connection setup error");
		exit(-1);
	}

	/* Get the port through which the client communicating */
	uint16_t client_conn_port = 0;
	if (get_port(listenfd, &client_conn_port) < 0) {
		perror("data connection setup error");
		exit(-1);
	}
	printf("IP: %s, Port: %d\n", ip, client_conn_port);
	/* Produce the port command of structure: "PORT h1,h2,h3,h4,p1,p2" */
	bzero(port_command, (int) sizeof(port_command));
	generate_port_command(port_command, ip, client_conn_port);

	while (1) {
		bzero(command, strlen(command));
		//get command from user
		cmd = get_command(command);

#if DEBUG_LEVEL >= 1
	fprintf(stdout, "command: %s\n", command);
#endif

		/* If the user entered the "quit" command at the prompt */
		if (cmd == CMD_QUIT){
			// TODO: double check this if statement. What is it checking for?
			// In which (if any) circumstances should the program then
			if (do_quit(controlfd) < 0) {
				close(datafd);
			}
			break;
		}

		/* Send the port command that was constructed earlier */
		write(controlfd, port_command, strlen(port_command));
		/* Establish data connection by listening for the server who is
		 * attempting to connect() */
		datafd = accept(listenfd, (struct sockaddr *) NULL, NULL);

#if DEBUG_LEVEL >= 1
	fprintf(stdout, "Data connection established. Ready to receive data!\n");
#endif

		if (cmd == CMD_LS) {
			if (do_ls(controlfd, datafd, command) < 0) {
				close(datafd);
				continue;
			}
		} else if (cmd == CMD_GET) {
			if (do_get(controlfd, datafd, command) < 0) {
				close(datafd);
				continue;
			}
		} else if(cmd == CMD_PUT) {
			if (do_put(controlfd, datafd, command) < 0) {
				close(datafd);
				continue;
			}
		}
		close(datafd);
	}
	close(controlfd);
	return TRUE;
}
