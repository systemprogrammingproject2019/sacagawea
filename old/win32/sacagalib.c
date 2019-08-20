#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "sacagawea.h"
#include "win32/sacagalib.h"

int SERVER_PORT = DEFAULT_SERVER_PORT;
char MODE_CLIENT_PROCESSING = 0; // 0=thread 1=subProcess

// this function check if a line contain a new config
int check_if_conf(char line[]) {
	fprintf(stdout, S_LINE_READ_FROM_CONF_FILE, line);
	int port_change = false;
	// if line is type "mode [t/p]"
	if (strncmp(S_MODE, line, 4) == 0) {
		char mode;
		memcpy(&mode, &line[5], 1);
		if (mode == S_MODE_THREADED) {
			MODE_CLIENT_PROCESSING = 0;
		}
		if (mode == S_MODE_MULTIPROCESS) {
			MODE_CLIENT_PROCESSING = 1;
		}
		//fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
	}

	// if line is "port XXX" with XXX a port number
	if (strncmp(S_PORT, line, 4) == 0) {
		long int val;
		val = strtol(&line[5], NULL, 10);

		if (val != SERVER_PORT) {
			SERVER_PORT = val;
			port_change = true;
		}
	}
	return port_change;
}

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

// this fuction opens a listening socket
SOCKET open_socket() {
	WSADATA wsaData;
	int err;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char recvbuf[SOCK_RECVBUF_LEN];
	int recvbuflen = SOCK_RECVBUF_LEN;

	// Initialize Winsock
	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) == SOCKET_ERROR) {
		printf("WSAStartup failed with error: %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags    = AI_PASSIVE;

	// windows requires the port to be a string
	char* port_to_string = malloc(16);
	snprintf(port_to_string, 15, "%d", SERVER_PORT);
	// Resolve the server address and port
	err = getaddrinfo(NULL, port_to_string, &hints, &result);
	if (err != 0) {
		printf("getaddrinfo failed: %d\n", err);
		WSACleanup();
		exit(EXIT_FAILURE);
	}
	free(port_to_string);

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %d", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// Setup the TCP listening socket
	if ((bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen)) == SOCKET_ERROR) {
		printf("bind failed with error: %d", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);

	if ((listen(ListenSocket, SOMAXCONN)) == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// Change the socket mode on the listening socket from blocking to
	// non-block so the application will not block waiting for requests
	ULONG NonBlock = true;
	if (ioctlsocket(ListenSocket, FIONBIO, &NonBlock) == SOCKET_ERROR){
		printf("ioctlsocket failed with error %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	return ListenSocket;
}

void print_client_args(struct client_args *client){
		fprintf( stdout, "SOCKET: %d\nIP:PORT: %s\n", client->socket);
}

int process_management(struct client_args *client_info) {
	return false;
}

// this function read the sacagawea.conf line by line 
int read_and_check_conf() {
	// some declaretion 
	FILE *fp;
	const size_t max_line_size = 100;
	char line[max_line_size];
	int keep_going = true;
	int port_change = false;
	//open config file and check if an error occured
	fp = fopen(SACAGAWEACONF_PATH, "r");
	if (fp == NULL) {
		fprintf(stderr, S_ERROR_FOPEN, strerror(errno));
	 	exit(5);
	}

	//readline or 100 char
	do {
		if (fgets(line, max_line_size, fp) == NULL) {
			if (feof(fp)) {
				keep_going = false;
			} else {
				fprintf(stderr, S_ERROR_FGETS, strerror(errno));
				exit(5);
			}
		}
		// check if the line is a config line
		if ((strlen(line) != 100) && (check_if_conf(line))) {
			port_change = true;
		}
	} while (keep_going);

	return port_change;
}

// this fuction check if the input contain a selector or not and return it
selector request_to_selector(char *input){

	int read_bytes;
	selector client_selector;
	memset( &client_selector, 0, sizeof(client_selector));
	client_selector.num_words=-1; // -1 mean 0 words, 0=1word .... n=(n-1)words. Like array index
	
	// check if input start with selector or not
	if( ( input[0] == '\t' ) || ( input[0] == ' ') ){ // if not, we don't take it
		strcpy( client_selector.selector, "" );
		read_bytes=0;
	}else{ // if contain it we take it
		sscanf( input, "%4096s", client_selector.selector);
		read_bytes = strlen(client_selector.selector);
	}
	
	fprintf( stdout, "\nSELECTOR: %s,%d bytes\n", client_selector.selector , read_bytes );

	/* if the client send a tab \t, it means that the selector is followed by words 
	that need to match with the name of the searched file */
	if( input[ read_bytes ] == '\t' ){
		int i=0;
		client_selector.words = (char **) malloc( 3*sizeof( char *) );
		do{
			// put this check, becouse if the request contain 2+ consecutive \t or 2+ consecutive ' ' the scanf don't read an empty word.
			if( (input[read_bytes] == '\t') || (input[read_bytes] == ' ') ){
				//fprintf( stdout, "CHAR %c\n", input[read_bytes]);
				read_bytes++;
				continue;
			}
			// realloc check. we do a realloc every 3 words, just for limit overhead of realloc, and don't do every word
			// first call do a malloc(3) becouse words=NULL after do realloc( 6 ) ... 9, 12 ...
			if( (i % 3) == 0){
				client_selector.words = (char **) realloc(  client_selector.words, (i+3)*sizeof( char *)  );
			}
			/* declare a space for word and read it, OPPURE c'è l'opzione %m che passandogli  client_selector.words[i], senza 
			fare prima la malloc la fa scanf in automatico della grandezza della stringa letta + 1, sarebbe piu efficente dato che 
			MAX_FILE_NAME è spazio sprecato per parole di piccole len_string */
			client_selector.words[i] = (char *) malloc( ( (MAX_FILE_NAME+1)*sizeof( char ) ) );
			sscanf( &input[read_bytes], "%255s", client_selector.words[i]);
			// upgrade read_bytes for check when we finish the client input
			read_bytes += ( strlen( client_selector.words[i] )  );
			fprintf( stdout, "WORD %d: %s,%d bytes\n", i, client_selector.words[i] , strlen(client_selector.words[i]) );
			// upgrade the num of words, contained in client_selector
			client_selector.num_words=i;
			i++;

		}while( input[read_bytes] != '\n' );
	}

	return client_selector;
}


// this fuction is the real management of the client responce with thread as son
void *thread_function(void* c){
	// declare a variable of STRUCT client_args
	client_args *client_info = (client_args*) c;

	char type;
	int check;
	int read_bytes = 0;
	int sd = (*client_info).socket; // dato che (*client_info).socket era troppo lungo da riscrivere sempre ho usato sd 
	// becouse the request is a path (SELECTOR) and the max path is 4096, plus
	// eventualy some words which have to match with file name, wE put a MAX input = 4096
	char *input = malloc( PATH_MAX*sizeof(char) );

	/* Receive data on this connection until the recv \n of finish line.
	If any other failure occurs, we will close the connection.    */
	int stop=true;
	while( stop ){
		if(  (PATH_MAX-read_bytes) <= 0 ){
			// the client send a wrong input, or the lenght is > PATH_MAX, without a \n at end or send more bytes after \n
			close(sd);
			pthread_exit(NULL);
		}
		check = recv(sd, &input[read_bytes], (PATH_MAX-read_bytes), 0);
		if (check < 0){
			int e = WSAGetLastError();
			if (e != WSAEWOULDBLOCK){
				// if recv fail the error can be server side or client side so we close the connection and go on 
				fprintf( stderr,"recv() of sd - %d, failed: %d we close that connection\n", sd, e );
				return;
			}
			fprintf( stderr,"recv() of sd - %d WSAEWOULDBLOCK", sd );
			continue;
		}
		/* Check to see if the connection has been closed by the client, so recv return 0  */
		if (check == 0){
			printf("	Connection closed %d\n", sd );
			// client close the connection so we can stop the thread
			close(sd);
			pthread_exit(NULL);
		}
		if( check > 0){
			read_bytes += check;
			if(input[ (read_bytes-1) ]=='\n'){
				stop = false;
			}
		}
	}
	// if we are there, print that message
	//fprintf( stdout, "READ: %s%d bytes at %p\n", input, check, &input );

	// check if the input contain a selector or not
	selector client_selector;
	memset( &client_selector, 0, sizeof(client_selector));

	client_selector = request_to_selector( input );
	/* if ( client_selector == NULL ){
		send( sd, S_ERROR_SELECTOR_REQUEST, strlen(S_ERROR_SELECTOR_REQUEST), 0);
	} */

	// we have to add the path of gopher ROOT, else the client can access at all dir of server.
	client_info->path_file = (char*) malloc( strlen(client_selector.selector) + strlen(S_ROOT_PATH) + 1 );
	strcpy( client_info->path_file, S_ROOT_PATH ); 
	strcat( client_info->path_file, client_selector.selector );	
	fprintf(stdout,"PATH+SELECTOR: %s\n",client_info->path_file);

	if ( strcmp( client_selector.selector , "") == 0  ){
		// if selector is empty we send the content of gophermap, who match with words
		
		// and the content of ROOT_PATH
		type = type_path( S_ROOT_PATH );

	}else{ /* if we have a selector, we check if is a dir or not.*/

		//	little check for avoid trasversal path	
		if( check_security_path( client_selector.selector ) ){
			fprintf ( stdout, "eh eh nice try where u wanna go?\n" );
			close(sd);
			pthread_exit(NULL);
		}// add the gopher root path at selector and check the type of file
		type = type_path( client_info->path_file );
	}
	// if is a dir we check the content if match with words 
	if( type == '1' ){
		send_content_of_dir( client_info, &client_selector);
		//pthread_exit( NULL );
	}else{ 
		if( type == '3' ){ // if is an error send the error message
			char temp[ ( strlen(client_selector.selector) + 6 ) ]; // 3 is for lenght of "3\t" + 1 per \n + 2 for last line + 1 \0
			strcpy( temp, "3\t" ); 
			strcat( temp, client_selector.selector );
			strcat( temp, "\n.\n" ); // senza \n non inviava rimaneva in pending nel buffer del socket senza inviare. non so perche
			send( sd, temp, strlen(temp), 0);
			// close socket and thread
			close(sd);
			pthread_exit(NULL);
		}else{ // if is only a file
			load_file_memory_and_send_posix( client_info );
		}
	}
	
}


int thread_management(client_args *client_info) {
	HANDLE  hThread;
	client_args * tData;
	print_client_args(client_info);
	LPDWORD lpThreadId;

	tData = (client_args*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(client_args));

	hThread = CreateThread( 
			NULL,                   // default security attributes
			0,                      // use default stack size  
			thread_function,       // thread function name
			tData,                  // argument to thread function 
			0,                      // use default creation flags 
			&lpThreadId);   // returns the thread identifier 


	return false;
}
