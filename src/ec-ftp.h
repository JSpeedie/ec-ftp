#ifndef ECFTP_HEADER
#define ECFTP_HEADER

#include <arpa/inet.h>
#include <stdint.h>

#define KEEP_TEMP_ENC_FILES 1
#define KEEP_TEMP_COMP_FILES 1
#define MAXLINE 4096
#define LISTENQ 1024
#define NDATAFD 4
#define TRUE 1
#define FALSE 0
#define CMD_LS 1
#define CMD_GET 2
#define CMD_PUT 3
#define CMD_QUIT 4


void trim(char *str);

int get_port(int fd, uint16_t *port);

int get_ip_port(int fd, char *ip, uint16_t *port);

char * temp_recv_name(char * filename);

int prepare_file(char * filename, uint32_t key[4], char ** ret_prepared_fp);

int process_received_file(char * filename, char * recv_fp, uint32_t key[4]);

int do_dh_client(int controlfd, int datafd, uint32_t key[4]);

int do_dh_server(int controlfd, int datafd, uint32_t key[4]);

#endif
