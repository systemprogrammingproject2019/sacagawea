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

int SERVER_PORT = DEFAULT_SERVER_PORT;
char MODE_CLIENT_PROCESSING = 0; // 0=thread 1=subProcess


void print_client_args( client_args *client){
	fprintf( stdout, "SOCKET: %d\nIP:PORT: %s\n", client->socket, client->client_addr );
}

// this function take a path as argument and return the gopher char associated.
// in the Gopher.md u can see all gopher char and the translate
char type_path( char path[PATH_MAX] ){

	// we check the tipe or file with file bash command
	char command[ (strlen(path)+9) ];
	// file with -b option: 
	strcpy( command, "file -bi "); // 9 for "file -b " + \0 at end
	strcat( command, path );
	FILE* popen_output_stream = popen( command , "r" );
	if ( popen_output_stream == NULL ){ 
		fprintf( stderr,"popen() failed: %s\n", strerror(errno));
	 	exit(5);
	}
	char popen_output[20]; // is useless read all output, i need only the first section and the max is "application/gopher"
	
	fgets( &popen_output, 20, popen_output_stream);
	//fprintf( stdout, "%s\n", popen_output); 
	if( strncmp( popen_output, "cannot", 6 )==0 ){
		fprintf( stdout, "%s", popen_output);
		while ( fgets( &popen_output, 20, popen_output_stream) != NULL){
			fprintf( stdout, "%s", popen_output); 
		}
		fprintf( stdout, "\n"); 
	}
	close( popen_output_stream );
	

	if( (strncmp( popen_output, DIR_1, strlen(DIR_1))==0 ) || (strncmp( popen_output, GOPHER_1, strlen(GOPHER_1))==0) ){
		return '1';
	}
	if( strncmp( popen_output, MULTIPART_M, strlen(MULTIPART_M))==0 ){
		return 'M';
	}
	if( strncmp( popen_output, APPLICATION_9, strlen(APPLICATION_9))==0 ){
		return '9';
	}
	if( strncmp( popen_output, AUDIO_s, strlen(AUDIO_s)) ==0 ){
		return 's';
	}
	if( strncmp( popen_output, HTML_h, strlen(HTML_h))==0 ){
		return 'h';
	}
	if( ( strncmp( popen_output, TEXT_0, strlen(TEXT_0))==0 ) || ( strncmp( popen_output, EMPTY_0, strlen(EMPTY_0))==0 ) ){
		return '0';
	}
	if( strncmp( popen_output, GIF_g, strlen(GIF_g))==0 ){
		return 'g';
	}
	if( strncmp( popen_output, IMAGE_I, strlen(IMAGE_I))==0 ){
		return 'I';
	}
	if( strncmp( popen_output, MAC_4, strlen(MAC_4))==0 ){
		return '4';
	}
	return '3';
}

// this fuction check if string d_name containt at some position the string word
// es d_name=ciao_mario  word=mar  match
int check_not_match( char d_name[PATH_MAX+1], char *word){
	int i=0;
	for( i=0; i< strlen( d_name ); i++){
		if ( strncmp( &d_name[i], word, strlen(word) ) == 0){
			return false;
		}
	}
	return true;
}

