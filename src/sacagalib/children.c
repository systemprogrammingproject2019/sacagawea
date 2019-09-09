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

#ifdef _WIN32
void close_socket_kill_thread(SOCKET sd, int errcode);
void close_socket_kill_process(SOCKET sd, int errcode);
#else
void close_socket_kill_thread(int sd, int errcode);
void close_socket_kill_process(int sd, int errcode);
#endif


#ifdef _WIN32
int read_request(SOCKET sd, char *input) {
#else
int read_request(int sd, char *input) {
#endif
	/* Receive data on this connection until the recv \n of finish line.
	If any other failure occurs, we will returt true.    */
	int check;
	#ifdef _WIN32
	int err;
	#endif
	int read_bytes = 0;
	int stopcheck = false;
	int keep_going = true;

	while (keep_going) {
		if ((MAX_REQUEST_LEN - read_bytes) <= 0) {
			// the client send a wrong input, or the lenght is > PATH_MAX, without a \n at end or send more bytes after \n
			write_log(ERROR, "recv() of sd - %d, failed because of wrong input. Closing the connection", sd, strerror(errno));
			return true;
		}
		check = recv(sd, &input[read_bytes], 1, 0);
		if (check < 0) {
		#ifdef _WIN32
			if ((err = WSAGetLastError()) != WSAEWOULDBLOCK) {
				// if recv fail the error can be server side or client side so we close the connection and go on 
				write_log(ERROR, "recv() of sd - %d, failed with error: %d. Closing the connection.", sd, err);
				return true;
			}
			write_log(WARNING, "recv() of sd - %d WSAEWOULDBLOCK", sd);
			continue;
		#else
			if (errno != EWOULDBLOCK) {
				// if recv fail the error can be server side or client side so we close the connection and go on 
				write_log(ERROR, "recv() of sd - %d, failed with error: %s. Closing the connection.", sd, strerror(errno));
				return true;
			}
			write_log(WARNING, "recv() of sd - %d EWOULDBLOCK", sd );
			continue;
		#endif
		}
		/* Check to see if the connection has been closed by the client, so recv return 0  */
		if (check == 0) {
			printf("	Connection closed %I64d\n", sd);
			// client close the connection so we can stop read and responce
			return true;
		}
		if (check > 0) {
			read_bytes += check;
			if( stopcheck ){
				if( input[(read_bytes-1)] == '\n' ) {
					keep_going = false;
				}else{
					stopcheck = false;
				}
			}
			if( input[(read_bytes-1)] == '\r' ) {
				stopcheck = true;
			}
		}
	}
	return false;
}
/* non serve thread_function funziona identicamente.
// this is the real process fuction which management the request, when mode is p
void process_fuction(client_args *client_info) {
#ifdef _WIN32
#else
	char type; // will containt the type of selector

	int sd = (*client_info).socket; // dato che (*client_info).socket era troppo lungo da riscrivere sempre ho usato sd 
	// becouse the request is a path (SELECTOR) and the max path is 4096, plus
	// eventualy some words which have to match with file name, wE put a MAX input = 4096
	char *input = malloc(PATH_MAX * sizeof(char));

	// read request from sd ( client socket ) and put in *input, if fail return true otherwise false
	if (read_request( sd, input)) {
		close(sd);
		pthread_exit(NULL);
	}
	// check if the input contain a selector or not
	selector client_selector;
	memset( &client_selector, '\0', sizeof(client_selector));

	client_selector = request_to_selector(input);


	// we have to add the path of gopher ROOT, else the client can access at all dir of server.
	client_info->path_file = (char*) malloc(strlen(client_selector.selector) + strlen(S_ROOT_PATH) + 1);
	strcpy(client_info->path_file, S_ROOT_PATH); 
	strcat(client_info->path_file, client_selector.selector);	
	fprintf(stdout,"PATH+SELECTOR %ld bytes: %s\n", strlen(client_info->path_file), client_info->path_file);

	if (client_selector.selector[0] == '\0') {
		// if selector is empty we send the content of gophermap, who match with words
		// and the content of ROOT_PATH
		type = type_path(S_ROOT_PATH);

	} else { // if we have a selector, we check if is a dir or not.

		//	little check for avoid trasversal path	
		if (check_security_path(client_selector.selector)) {
			write_log(INFO, "eh eh nice try where u wanna go?\n");
			close(sd);
			_exit(5);
		}// add the gopher root path at selector and check the type of file
		type = type_path(client_info->path_file);
	}
	// if is a dir we check the content if match with words 
	if (type == '1') {
		// TODO change return of send dir with true/false for close connection ecc
		send_content_of_dir(client_info, &client_selector);
		_exit(1);
		//pthread_exit( NULL );
	} else { 
		if (type == '3') { // if is an error send the error message
			char temp[(strlen(client_selector.selector) + 6 )]; // 3 is for lenght of "3\t" + 1 per \n + 2 for last line + 1 \0
			strcpy(temp, "3\t"); 
			strcat(temp, client_selector.selector);
			strcat(temp, "\n.\n"); // senza \n non inviava rimaneva in pending nel buffer del socket senza inviare. non so perche
			send( sd, temp, strlen(temp), 0);
			// close socket and thread
			close(sd);
			_exit(1);
		} else { // if is only a file
			// TODO  only that shit and is finish
			load_file_memory_and_send(client_info);
			_exit(1);
		}
	}
#endif
} */

// this function spawn process to management the new client request 
int process_management(client_args *client_info) {
#ifdef _WIN32
	// TODO: create a sacagalogs.exe
	TCHAR szCmdline[] = TEXT("sacagalogs.exe");
	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFO siStartInfo;
	int bSuccess = false;

	// Set up members of the PROCESS_INFORMATION structure.
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
 
	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO); 
	// siStartInfo.hStdError = g_hChildStd_OUT_Wr;
	// siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	// siStartInfo.hStdInput = g_hChildStd_IN_Rd;
	// siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 
	bSuccess = CreateProcess(
		NULL, 
		szCmdline,     // command line
		NULL,          // default security attributes
		NULL,          // primary thread security attributes
		TRUE,          // handles are inherited
		0,             // creation flags
		NULL,          // use parent's environment
		NULL,          // use parent's current directory
		&siStartInfo,  // STARTUPINFO pointer
		&piProcInfo    // receives PROCESS_INFORMATION
	);

	// If an error occurs, exit the application. 
	if (!bSuccess) {
		write_log(ERROR, "Failed to create subprocess.");
		exit(1);
	} else {
		// Close handles to the child process and its primary thread.
		// Some applications might keep these handles to monitor the status
		// of the child process, for example. 

		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);
	}
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
		management_function( client_info );
		printf("Son finish to serv\n");
		exit(1);
	} else {
		// this is the server
		// close connection and deallocate resourses
		close(client_info->socket);
		free(client_info);
	}

#endif
	return false;
}

// this fuction check if the input contain a selector or not and return it
selector request_to_selector(char *input){
	int read_bytes=0;
	selector client_selector;
	memset( &client_selector, '\0' , sizeof(client_selector));
	client_selector.num_words=-1; // -1 mean 0 words, 0=1word .... n=(n-1)words. Like array index
	
	// check if input start with selector or not
	if ( input[0] == '\t' ) { // if not, we don't take it
		client_selector.selector[0] = '\0' ;
		read_bytes=0;
	} else { // if contain it we take it

		while( true ){
			// if we arrive at \t we have finished the selector.	
			if( input[read_bytes] == '\t' ){
				break;
			}
			if( input[read_bytes] == '\r') {
				// if we got \r we check if is followed by \n.
				if( input[read_bytes+1] == '\n' ){
					// if yes, the input is finished
					break;
				}else{
					// if not, that means \r is in the file name.
					client_selector.selector[read_bytes] = input[read_bytes];
					read_bytes++;
				}
			}else{
				// we save the char in the selector becouse is an accettable characters
				client_selector.selector[read_bytes] = input[read_bytes];
				read_bytes++;
			}
		}
	}
	
	write_log(INFO, "SELECTOR: %s,%d bytes", client_selector.selector , read_bytes );

	// if the client send a tab \t, it means that the selector is followed by words 
	// that need to match with the name of the searched file 
	if (input[ read_bytes ] == '\t') {
		int i = 0;
		client_selector.words = (char **) malloc( 3*sizeof( char *) );
		while ( (input[read_bytes] != '\r') && (input[read_bytes] != '\n') ){
			// put this check, becouse if the request contain 2+ consecutive \t or 2+ consecutive ' ' the scanf don't read an empty word.
			if ((input[read_bytes] == '\t') || (input[read_bytes] == ' ')) {
				fprintf( stdout, "CHAR -%c-\n", input[read_bytes]);
				read_bytes++;
				continue;
			}
			// realloc check. we do a realloc every 3 words, just for limit overhead of realloc, and don't do every word
			// first call do a malloc(3) becouse words=NULL after do realloc( 6 ) ... 9, 12 ...
			if ((i % 3) == 0) {
				client_selector.words = (char **) realloc(  client_selector.words, (i+3)*sizeof( char *)  );
			}
			// declare a space for word and read it, OPPURE c'è l'opzione %m che passandogli  client_selector.words[i], senza 
			// fare prima la malloc la fa scanf in automatico della grandezza della stringa letta + 1, sarebbe piu efficente dato che 
			// MAX_FILE_NAME è spazio sprecato per parole di piccole len_string 
			client_selector.words[i] = (char *) malloc(((MAX_FILE_NAME+1) * sizeof(char)));
			sscanf(&input[read_bytes], "%255s", client_selector.words[i]);
			// upgrade read_bytes for check when we finish the client input
			read_bytes += (strlen( client_selector.words[i]));
			write_log(INFO, "WORD %d: %s,%llu bytes", i, client_selector.words[i] , strlen(client_selector.words[i]) );
			// upgrade the num of words, contained in client_selector
			client_selector.num_words=i;
			i++;

		}
	}

	return client_selector;
}

// this fuction is the real management of the client responce with thread as son
// needs to be "long unsigned int *" because win32 wants that, whereas
// linux has no preference (only needs a "void *")
long unsigned int *management_function(client_args* c) {
	// declare a variable of STRUCT client_args
	client_args *client_info = (client_args*) c;
	char type; // will containt the type of selector
	int check;

#ifdef _WIN32
	SOCKET sd = client_info->socket;
#else
	int sd = client_info->socket; // dato che (*client_info).socket era troppo lungo da riscrivere sempre ho usato sd 
#endif

	// becouse the request is a path (SELECTOR) and the max path is 4096, plus
	// eventualy some words which have to match with file name, wE put a MAX input = 4096
	char *input = calloc(PATH_MAX, sizeof(char));

	// read request from sd ( client socket ) and put in *input, if fail return true otherwise false
	if ((check = read_request(sd, input))) {
		if (MODE_CLIENT_PROCESSING == 0) {
			close_socket_kill_thread(sd, 0);
		}else{
			close_socket_kill_process(sd, 0);
		}
	}
	
	// if we are there, print that message
	write_log(DEBUG, "RECEIVED: \"%s\"\n%d bytes at %p\n", input, check, &input );

	// check if the input contain a selector or not
	selector client_selector;
	memset(&client_selector, '\0', sizeof(client_selector));

	client_selector = request_to_selector(input);
	/* if ( client_selector == NULL ){
		send( sd, S_ERROR_SELECTOR_REQUEST, strlen(S_ERROR_SELECTOR_REQUEST), 0);
	} */

	// we have to add the path of gopher ROOT, else the client can access at all dir of server.
	client_info->path_file = (char*) calloc(PATH_MAX + 1, sizeof(char));
#ifdef _WIN32
	GetCurrentDirectory(PATH_MAX, client_info->path_file);
	client_info->path_file[strlen(client_info->path_file)] = '\\';
#else
	strcpy(client_info->path_file, S_ROOT_PATH);
#endif
	strcat(client_info->path_file, client_selector.selector);
	write_log(INFO, "PATH+SELECTOR %d bytes: %s", strlen(client_info->path_file), client_info->path_file);

	// avoid trasversal path	
	if (check_security_path(client_info->path_file)) {
		write_log(INFO, "eh eh nice try where u wanna go?");
		if (MODE_CLIENT_PROCESSING == 0) {
			close_socket_kill_thread(sd, 0);
		}else{
			close_socket_kill_process(sd, 0);
		}
	}
	type = type_path(client_info->path_file);

	// if is a dir we check the content if match with words 
	if (type == '1') {
		send_content_of_dir(client_info, &client_selector);
	} else if (type == '3') { // if is an error send the error message
		char temp[(strlen(client_selector.selector) + 6)]; // 3 is for lenght of "3\t" + 1 per \n + 2 for last line + 1 \0
		strcpy(temp, "3\t");
		strcat(temp, client_selector.selector);
		strcat(temp, "\n.\n"); // senza \n non inviava rimaneva in pending nel buffer del socket senza inviare. non so perche
		send(sd, temp, strlen(temp), 0);
		// close socket and thread
	} else { // if is only a file
		load_file_memory_and_send(client_info);
	}

	if (MODE_CLIENT_PROCESSING == 0) {
		close_socket_kill_thread(sd, 0);
	}else{
		close_socket_kill_process(sd, 0);
	}
	return 0;
}

// this function spawn thread to management the new client request 
#ifdef _WIN32
HANDLE thread_management(client_args *client_info) {
#else
int thread_management(client_args *client_info) {
#endif

#ifdef _WIN32
	HANDLE tHandle;
	client_args *tData;
	print_client_args(client_info);
	LPDWORD lpThreadId = 0;

	tData = (client_args*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(client_args));

	memcpy(tData, client_info, sizeof(client_args));

	tHandle = CreateThread( 
			NULL,            // default security attributes
			0,               // use default stack size  
			thread_function, // thread function name
			tData,           // argument to thread function 
			0,               // use default creation flags 
			lpThreadId       // returns the thread identifier 
	);
	return tHandle;
#else
	pthread_t tid;
	print_client_args(client_info);
	pthread_create(&tid, NULL, (void *) management_function, (void *) client_info);
	return tid;
#endif
}

#ifdef _WIN32
void close_socket_kill_thread(SOCKET sd, int errcode) {
#else
void close_socket_kill_thread(int sd, int errcode) {
#endif
	#ifdef _WIN32
		closesocket(sd);
		ExitThread(errcode);
	#else
		close(sd);
		pthread_exit(&errcode);
	#endif
}
#ifdef _WIN32
void close_socket_kill_process(SOCKET sd, int errcode) {
#else
void close_socket_kill_process(int sd, int errcode) {
#endif
	#ifdef _WIN32
		closesocket(sd);
		_exit(errcode);
	#else
		close(sd);
		_exit(errcode);
	#endif
}