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

int read_request( int sd, char *input){

	/* Receive data on this connection until the recv \n of finish line.
	If any other failure occurs, we will returt true.    */
	int check;
	int read_bytes=0;
	int stop=true;

	while( stop ){
		if ((PATH_MAX-read_bytes) <= 0 ) {
			// the client send a wrong input, or the lenght is > PATH_MAX, without a \n at end or send more bytes after \n
			fprintf( stderr,"recv() of sd - %d, failed: Becouse wrong input, we close that connection\n", sd, strerror(errno) );
			return true;
		}
		check = recv(sd, &input[read_bytes], (PATH_MAX-read_bytes), 0);
		if (check < 0){
			if (errno != EWOULDBLOCK){
				// if recv fail the error can be server side or client side so we close the connection and go on 
				fprintf( stderr,"recv() of sd - %d, failed: %s we close that connection\n", sd, strerror(errno) );
				return true;
			}
			fprintf( stderr,"recv() of sd - %d EWOULDBLOCK", sd );
			continue;
		}
		/* Check to see if the connection has been closed by the client, so recv return 0  */
		if (check == 0){
			printf("	Connection closed %d\n", sd );
			// client close the connection so we can stop read and responce
			return true;
		}
		if( check > 0){
			read_bytes += check;
			if(input[ (read_bytes-1) ]=='\n'){
				stop = false;
			}
		}
	}
	return false;
}

// this is the real process fuction which management the request, when mode is p
void process_fuction(client_args *client_info){
#ifdef _WIN32
#else
	char type; // will containt the type of selector

	int sd = (*client_info).socket; // dato che (*client_info).socket era troppo lungo da riscrivere sempre ho usato sd 
	// becouse the request is a path (SELECTOR) and the max path is 4096, plus
	// eventualy some words which have to match with file name, wE put a MAX input = 4096
	char *input = malloc( PATH_MAX*sizeof(char) );

	// read request from sd ( client socket ) and put in *input, if fail return true otherwise false
	if( read_request( sd, input) ){
		close(sd);
		pthread_exit(NULL);
	}
	// check if the input contain a selector or not
	selector client_selector;
	memset( &client_selector, '\0', sizeof(client_selector));

	client_selector = request_to_selector( input );
	/* if ( client_selector == NULL ){
		send( sd, S_ERROR_SELECTOR_REQUEST, strlen(S_ERROR_SELECTOR_REQUEST), 0);
	} */

	// we have to add the path of gopher ROOT, else the client can access at all dir of server.
	client_info->path_file = (char*) malloc( strlen(client_selector.selector) + strlen(S_ROOT_PATH) + 1 );
	strcpy( client_info->path_file, S_ROOT_PATH ); 
	strcat( client_info->path_file, client_selector.selector );	
	fprintf(stdout,"PATH+SELECTOR %d bytes: %s\n", strlen(client_info->path_file), client_info->path_file);

	if ( client_selector.selector[0] == '\0' ){
		// if selector is empty we send the content of gophermap, who match with words
		// and the content of ROOT_PATH
		type = type_path( S_ROOT_PATH );

	}else{ /* if we have a selector, we check if is a dir or not.*/

		//	little check for avoid trasversal path	
		if( check_security_path( client_selector.selector ) ){
			fprintf ( stdout, "eh eh nice try where u wanna go?\n" );
			close(sd);
			_exit(5);
		}// add the gopher root path at selector and check the type of file
		type = type_path( client_info->path_file );
	}
	// if is a dir we check the content if match with words 
	if( type == '1' ){
		// TODO change return of send dir with true/false for close connection ecc
		send_content_of_dir( client_info, &client_selector);
		_exit(1);
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
			_exit(1);
		}else{ // if is only a file
			// TODO  only that shit and is finish
			load_file_memory_and_send_posix( client_info );
			_exit(1);
		}
	}
#endif
}

// this function spawn process to management the new client request 
int process_management(client_args *client_info) {
	#ifdef _WIN32
	#else

	int pid;
	if ((pid = fork()) < 0) {
		// failed fork on server
	}
	if (pid == 0) {
		// child who have to management the connection
		// close server_socket
		close(SERVER_SOCKET);
		printf("Son spawned ready to serv\n");
		process_fuction( client_info );
		printf("Son finish to serv\n");
		exit(1);
	} else {
		// this is the server
		// close connection and deallocate resourses
		close(client_info->socket);
		free(client_info);
	}

#endif
}

// this fuction check if the input contain a selector or not and return it
selector request_to_selector(char *input){
	int read_bytes;
	selector client_selector;
	memset( &client_selector, '\0' , sizeof(client_selector));
	client_selector.num_words=-1; // -1 mean 0 words, 0=1word .... n=(n-1)words. Like array index
	
	// check if input start with selector or not
	if ((input[0] == '\t') || (input[0] == ' ') || (input[0] == '\n')) { // if not, we don't take it
		client_selector.selector[0] = '\0' ;
		read_bytes=0;
	} else { // if contain it we take it
		sscanf( input, "%4096s", client_selector.selector);
		read_bytes = strlen(client_selector.selector);
	}
	
	fprintf(stdout, "\nSELECTOR: %s,%d bytes\n", client_selector.selector , read_bytes );

	/* if the client send a tab \t, it means that the selector is followed by words 
	that need to match with the name of the searched file */
	if (input[ read_bytes ] == '\t') {
		int i = 0;
		client_selector.words = (char **) malloc( 3*sizeof( char *) );
		do {
			// put this check, becouse if the request contain 2+ consecutive \t or 2+ consecutive ' ' the scanf don't read an empty word.
			if ((input[read_bytes] == '\t') || (input[read_bytes] == ' ')) {
				//fprintf( stdout, "CHAR %c\n", input[read_bytes]);
				read_bytes++;
				continue;
			}
			// realloc check. we do a realloc every 3 words, just for limit overhead of realloc, and don't do every word
			// first call do a malloc(3) becouse words=NULL after do realloc( 6 ) ... 9, 12 ...
			if ((i % 3) == 0) {
				client_selector.words = (char **) realloc(  client_selector.words, (i+3)*sizeof( char *)  );
			}
			/* declare a space for word and read it, OPPURE c'è l'opzione %m che passandogli  client_selector.words[i], senza 
			fare prima la malloc la fa scanf in automatico della grandezza della stringa letta + 1, sarebbe piu efficente dato che 
			MAX_FILE_NAME è spazio sprecato per parole di piccole len_string */
			client_selector.words[i] = (char *) malloc(((MAX_FILE_NAME+1) * sizeof(char)));
			sscanf(&input[read_bytes], "%255s", client_selector.words[i]);
			// upgrade read_bytes for check when we finish the client input
			read_bytes += (strlen( client_selector.words[i]));
			fprintf(stdout, "WORD %d: %s,%llu bytes\n", i, client_selector.words[i] , strlen(client_selector.words[i]) );
			// upgrade the num of words, contained in client_selector
			client_selector.num_words=i;
			i++;

		} while (input[read_bytes] != '\n');
	}

	return client_selector;
}

// this fuction is the real management of the client responce with thread as son
void *thread_function(void* c) {
#ifdef _WIN32
	return;
#else
	// declare a variable of STRUCT client_args
	client_args *client_info;
	client_info = (client_args*) c;

	char type; // will containt the type of selector

	int sd = (*client_info).socket; // dato che (*client_info).socket era troppo lungo da riscrivere sempre ho usato sd 
	// becouse the request is a path (SELECTOR) and the max path is 4096, plus
	// eventualy some words which have to match with file name, wE put a MAX input = 4096
	char *input = malloc(PATH_MAX*sizeof(char));

	// read request from sd ( client socket ) and put in *input, if fail return true otherwise false
	if (read_request( sd, input)) {
		close(sd);
		pthread_exit(NULL);
	}
	
	// if we are there, print that message
	//fprintf( stdout, "READ: %s%d bytes at %p\n", input, check, &input );

	// check if the input contain a selector or not
	selector client_selector;
	memset(&client_selector, '\0', sizeof(client_selector));

	client_selector = request_to_selector(input);
	/* if ( client_selector == NULL ){
		send( sd, S_ERROR_SELECTOR_REQUEST, strlen(S_ERROR_SELECTOR_REQUEST), 0);
	} */

	// we have to add the path of gopher ROOT, else the client can access at all dir of server.
	client_info->path_file = (char*) malloc(strlen(client_selector.selector) + strlen(S_ROOT_PATH) + 1);
	strcpy(client_info->path_file, S_ROOT_PATH); 
	strcat(client_info->path_file, client_selector.selector);
	write_log(INFO, "PATH+SELECTOR %d bytes: %s", strlen(client_info->path_file), client_info->path_file);

	if (client_selector.selector[0] == '\0') {
		// if selector is empty we send the content of gophermap, who match with words
		// and the content of ROOT_PATH
		type = type_path(S_ROOT_PATH);

	} else { /* if we have a selector, we check if is a dir or not.*/

		//	little check for avoid trasversal path	
		if (check_security_path( client_selector.selector)) {
			write_log(INFO, "eh eh nice try where u wanna go?");
			close(sd);
			pthread_exit(NULL);
		}// add the gopher root path at selector and check the type of file
		type = type_path(client_info->path_file);
	}
	// if is a dir we check the content if match with words 
	if (type == '1') {
		send_content_of_dir(client_info, &client_selector);
		pthread_exit(NULL);
	}else{ 
		if (type == '3') { // if is an error send the error message
			char temp[(strlen(client_selector.selector) + 6)]; // 3 is for lenght of "3\t" + 1 per \n + 2 for last line + 1 \0
			strcpy(temp, "3\t"); 
			strcat(temp, client_selector.selector);
			strcat(temp, "\n.\n"); // senza \n non inviava rimaneva in pending nel buffer del socket senza inviare. non so perche
			send(sd, temp, strlen(temp), 0);
			// close socket and thread
			close(sd);
			pthread_exit(NULL);
		} else { // if is only a file
			load_file_memory_and_send_posix(client_info);
		}
	}
#endif
}

// this function spawn thread to management the new client request 
int thread_management(client_args *client_info) {
#ifdef _WIN32
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

#else
	pthread_t tid;
	print_client_args(client_info);
	pthread_create(&tid, NULL, thread_function, (void *) client_info);
#endif
}
