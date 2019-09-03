#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>

#ifdef _WIN32
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "sacagalib.h"

/* TODO after end, for improve.
1 void send_content_of_dir( char *path_of_dir, client_args *client_info, selector *client_selector) 
inserire path_of_dir come argomento del selector.
2 sistemare il check del selector, ora è di 4096 PATH_MAX ma poi va aggiunto all'inizio il ROOT_PATH quindi
se passa un path di 4096 ci sta un buffer overflow.
 */

void print_client_args(client_args *client) {
	write_log(INFO, "SOCKET: %d;  IP: %s;  PORT: %s",
			client->socket, client->client_addr, client->client_addr );
}

// this fuction check if string d_name containt at some position the string word
// es d_name=ciao_mario  word=mar  match
int check_not_match(char d_name[PATH_MAX+1], char *word) {
	int i = 0;
	for (i = 0; i< strlen( d_name ); i++) {
		if (strncmp(&d_name[i], word, strlen(word) ) == 0) {
			return false;
		}
	}
	return true;
}

