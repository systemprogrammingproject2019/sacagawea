#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <poll.h>

#ifdef _WIN32
#else
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


#ifdef _WIN32
int listen_descriptor(SOCKET svr_socket) {
	SOCKET new_socket, s;
	int addrlen = sizeof(struct sockaddr_in), num_fd_ready;
	struct sockaddr_in address;
	struct client_args* client_info = malloc(sizeof(struct client_args));

	struct timeval timeout;
	timeout.tv_sec  = 13 * 60;
	timeout.tv_usec = 0;

	fd_set working_set;
	char str_client_addr[ADDR_MAXLEN];

	// /* create a copy of fds_set called working_set, is a FD_SET to work on  */
	memcpy(&working_set, &fds_set, sizeof(fds_set));

	// Prepare the socket set for network I/O notification
	FD_ZERO(&working_set);
	// Always look for connection attempts
	FD_SET(svr_socket, &working_set);

	//add child sockets to fd set
	for (int i = 0; i < MAX_CLIENTS; i++) {
		s = client_socket[i];
		if (s > 0) {
			FD_SET(s, &working_set);
		}
	}

	//wait for an activity on any of the sockets, timeout is NULL , so wait indefinitely
	if ((num_fd_ready = select(0, &working_set, NULL, NULL, &timeout)) == SOCKET_ERROR) {
		printf("select failed with error: %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i <= MAX_CLIENTS && num_fd_ready > 0; ++i) {

		// Check to see if the i-th descriptor is ready
		if (FD_ISSET(svr_socket, &working_set)) {
			/* if we come there, the descriptor is readable. */
			num_fd_ready -= 1;

			if (i==SERVER_SOCKET) {
				printf("\n--------------------\nListening socket is readable\n--------------------\n\n");
				/* Accept all incoming connections that are queued up on the listening socket
					* before we loop back and call select again. */
				do {
					/*Accept each incoming connection.  If accept fails with EWOULDBLOCK,
					then we have accepted all of them.
					Any other failure on accept will cause us to end the server.  */
					memset(&address, 0, sizeof(address));
					size_t address_len = sizeof(address);
					if ((new_socket = accept(svr_socket, &address, &address_len)) == SOCKET_ERROR){
						int e = WSAGetLastError();
						if (e != WSAEWOULDBLOCK) {
							fprintf(stderr, "accept failed with error: %d\n", e);
						}
						break;
					}
					/* we create a t/p for management the incoming connection, call the right function with (socket , client_addr) as argument */
					snprintf(client_info->client_addr, ADDR_MAXLEN, "%s:%d", 
							inet_ntoa(address.sin_addr), address.sin_port);
					client_info->socket = new_socket;

					printf("New connection estabilished at socket - %d from %s\n",
							client_info->socket, client_info->client_addr);
					if (MODE_CLIENT_PROCESSING == 0) {
						thread_management(&client_info);
					} else {
						if ( MODE_CLIENT_PROCESSING == 1){
							process_management( client_info );
						}else{
							fprintf( stderr,"WRONG MODE PLS CHECK: %d\n", MODE_CLIENT_PROCESSING );
							exit(5);
						}
					}
				} while (new_socket != INVALID_SOCKET);
			}
		}
	}
	return false;
}
#else
// this function call the select() and check the FDS_SET if some socket is readable
int listen_descriptor(int useless) {
	// Some declaretion of usefull variable
	struct sockaddr_in client_addr;
	int i, num_fd_ready, check, client_addr_len;
	struct timeval timeout;
	fd_set working_set;
	int close_conn;
	char str_client_addr[ (12+3+1+5) ]; // max lenght of IP is 16 254.254.254.254 + 5 char for port 65000
	int new_s;
	// struct defined in sacagawea.h for contain client information
	client_args *client_info;
	client_info = (client_args*) malloc( sizeof(client_args));
	memset( client_info, 0, sizeof(client_info));
	/* Initialize the timeval struct to 13 minutes.  If no        
	activity after 13 minutes this program will end.           */
	timeout.tv_sec  = 13 * 60;
	timeout.tv_usec = 0;

	/* create a copy of fds_set called working_set, is a FD_SET to work on  */
	memcpy( &working_set, &fds_set, sizeof(fds_set));

	// start select and check if failed
	printf("Waiting on select()...\n");
	check = select( max_num_s+1, &working_set, NULL, NULL, &timeout);
	/* if errno==EINTR the select is interrupted becouse of sigaction 
	so we have to repeat select, not exit(5) */
	if ( (check < 0) && (errno != EINTR) ){
		fprintf( stderr,"select() failed: %s\n", strerror(errno) );
		exit(5);
	}// Chek if select timed out
	if (check == 0){
		printf("select() timed out. End program.\n");
		return true;
	}
	/* 1 or more descriptors are readable we have to check which they are */
	num_fd_ready=check;
	
	// for, for check all ready FD in fds_set until, FD are finish or we check all the ready fd


	// qui ci sta un for perche avevo fatto una versione differente prima , devo sistemare don't worry
	// in realta il for non serve perche prima inserivo anche le nuove connessioni dentro FD_SET cosi
	// creavo il thread/processo solo quando era effettivamente leggibile, ma non cambiava nulla anzi
	// mi complicavo la vita a dover creare un dizionario per salvarmi informazioni ecc... dopo lo sistemo
	for (i=0;  i <= max_num_s && num_fd_ready > 0; ++i){
		close_conn = false;
		// Check to see if the i-esimo descriptor is ready
		if (FD_ISSET(i, &working_set)){
			/* if we come there, the descriptor is readable. */
			num_fd_ready -= 1;

			if (i==SERVER_SOCKET){
				printf("\n--------------------\nListening socket is readable\n--------------------\n\n");
				/*Accept all incoming connections that are queued up on the listening socket before we
				loop back and call select again. */
				do{
					/*Accept each incoming connection.  If accept fails with EWOULDBLOCK,
					then we have accepted all of them.
					Any other failure on accept will cause us to end the server.  */
					memset(&client_addr, 0, sizeof(client_addr));
					client_addr_len = sizeof(client_addr);
					new_s = accept(SERVER_SOCKET, &client_addr, &client_addr_len);
					if (new_s < 0){
						if (errno != EWOULDBLOCK){
							fprintf( stderr,"socket accept() failed: %s\n", strerror(errno) );
							exit(5);
						}
						break;
					}
					/* we create a t/p for management the incoming connection, call the right function with (socket , client_addr) as argument */
					snprintf( client_info->client_addr, ADDR_MAXLEN, "%s:%d",
							inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
					printf("New connection stabilished at fd - %d from %s\n",
							new_s, client_info->client_addr);
					client_info->socket=new_s;
					
					if ( MODE_CLIENT_PROCESSING == 0){
						thread_management( client_info );
					}else{
						if ( MODE_CLIENT_PROCESSING == 1){
							process_management( client_info );
						}else{
							fprintf( stderr,"WRONG MODE PLS CHECK: %d\n", MODE_CLIENT_PROCESSING );
							exit(5);
						}
					}

				}while (new_s != -1);
					
			}
		} // End of select loop
	}
	return false; 
}
#endif
