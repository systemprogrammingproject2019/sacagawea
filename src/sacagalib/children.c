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



void free_client_args(client_args* c) {
	free(c->path_file);
	free(c);
}



int read_request(sock_t sd, char* buf, int buflen) {
	/* Receive data on this connection until the recv \n of finish line.
	If any other failure occurs, we will returt true.*/
	int check;
	int read_bytes = 0;
	int keep_going = true;

	while (keep_going) {
		if (read_bytes >= buflen) {
			// the client send a wrong input, or the lenght is > PATH_MAX, without a \n at end or send more bytes after \n
			write_log(ERROR, "sd - %d, failed because of wrong input. Closing the connection",sd);
			return true;
		}
		int i=0;
		#ifdef _WIN32
		check = recv(sd, &buf[read_bytes], buflen, 0);
		#else
		check = recv(sd, &buf[read_bytes], buflen, MSG_DONTWAIT);
		#endif
		if (check > 0) {
			for( i=read_bytes; i<read_bytes+check; i++ ){
				// the standard of gopher request is "the end of message is /r/n"
				// so, all the message after /r/n will be discarded.
				if( (buf[i]=='\r') && (buf[i+1]=='\n') ){
					keep_going = false;
					// memset give a warning, but is a fake warning becouse (read_bytes+check-i) 
					// cant never be negative. look the upper for.
					memset( &buf[i] , '\0' , (read_bytes+check-i) );
					check = i; // we take until /n
				}
			}
			read_bytes += check;
		} else if (check == 0) {
			write_log(DEBUG, "Connection closed %lld", sd);
			// client close the connection so we can stop read and responce
			return true;
		} else {
		#ifdef _WIN32
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				// if recv fail the error can be server side or client side so we close the connection and go on 
				write_log(ERROR, "recv() of sd - %d, failed with error: %d. Closing the connection.",
						sd, WSAGetLastError());
				return true;
			}
			// write_log(WARNING, "recv() of sd - %d WSAEWOULDBLOCK", sd);
			continue;
		#else
			if (errno != EWOULDBLOCK) {
				// if recv fail the error can be server side or client side so we close the connection and go on 
				write_log(ERROR, "recv() of sd - %d, failed with error: %s. Closing the connection.",
						sd, strerror(errno));
				return true;
			}
			// write_log(WARNING, "recv() of sd - %d EWOULDBLOCK", sd );
			continue;
		#endif
		}
	}
	buf = realloc( buf , (read_bytes+1) );
	if( buf == NULL){
		write_log(ERROR, "realloc failed on reading input, close connection");
		return true;
	}
	return false;
}

// this function spawn process to management the new client request 
int process_management(client_args *client_info) {
#ifdef _WIN32
	char szCmdline[64];
	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFO siStartInfo;
	int bSuccess = false;

	// Set up members of the PROCESS_INFORMATION structure.
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
 
	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO); 

	// parent's security attributes, with bInheritHandle set to TRUE
	// so the FIle Mapping gets inherited by the child process
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES); 
	sa.bInheritHandle = TRUE; 
	sa.lpSecurityDescriptor = NULL;

	// creating an anonymous (nameless) File Mapping
	HANDLE hMapFile = CreateFileMappingA(
			INVALID_HANDLE_VALUE,
			&sa,
			PAGE_READWRITE,
			0,
			sizeof(client_args),
			NULL
	);
	if (hMapFile == NULL) {
		write_log(ERROR, "CreateFileMappingA failed wirh error: %d",
				GetLastError());
		exit(1);
	}

	// open file mapping
	LPCTSTR pBuf = (LPTSTR) MapViewOfFile(hMapFile,   // handle to map object
			FILE_MAP_ALL_ACCESS, // read/write permission
			0,
			0,
			sizeof(client_args));
	if (pBuf == NULL) {
		write_log(ERROR, "MapViewOfFile failed wirh error: %d",
				GetLastError());
		exit(1);
	}
	// write client_info into the file mapping
	memcpy((PVOID)pBuf, client_info, sizeof(client_args));

	// pass the handle number as the 1st argument
	// (this is why we dont need a named file mapping, we just use the handle)
	sprintf(szCmdline, "sacagawea-mp.exe %I64d", (long long) hMapFile);

	// Create the child process.
	bSuccess = CreateProcess(
			NULL, 
			szCmdline,     // command line
			NULL,          // default security attributes
			NULL,          // primary thread security attributes
			TRUE,          // IMPORTANT: handles are inherited
			DETACHED_PROCESS,  // creation flags
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

		if( CloseHandle(piProcInfo.hProcess) == 0 ){
			write_log(ERROR, "Close piProcInfo.hProcess failed wirh error: %d",GetLastError());
		}
		if( CloseHandle(piProcInfo.hThread) == 0 ){
			write_log(ERROR, "Close piProcInfo.hThread failed wirh error: %d",GetLastError());
		}
	}

	if( UnmapViewOfFile(pBuf) == 0 ){
		write_log(ERROR, "UnmapViewOfFile failed with error: %I64d",GetLastError());
	}
	if( CloseHandle(hMapFile) == 0 ){
		write_log(ERROR, "Close hMapFile failed wirh error: %d",GetLastError());
	}
	// need to close the socket here too, else it stays open in this process
	closesocket(client_info->socket);
	free(client_info);
#else
	int pid;
	pid = fork();
	if (pid < 0) {
		// failed fork on server
		write_log(ERROR, "fork for management the connection failed");
	}else{
		if (pid == 0) {
			// child who have to management the connection
			// close settings->socket
			//close(settings->socket);
			management_function(client_info);
			exit(1);
		} else {
			// this is the server
			// close connection and deallocate resourses
			close(client_info->socket);
			free(client_info);
		}
	}
#endif
	return false;
}

// this fuction is the real management of the client responce with thread as son
// needs to be "long unsigned int *" because win32 wants that, whereas
// linux has no preference (only needs a "void *")
long unsigned int* management_function(client_args* c) {
	char type; // will containt the type of selector
	int check;

#ifdef _WIN32
	WSADATA wsaData;
	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) == SOCKET_ERROR) {
		write_log(ERROR, "WSAStartup failed with error: %d", WSAGetLastError());
		close_socket_kill_child(c, 0);
	}
#endif

	// becouse the request is a path (SELECTOR) and the max path is 4096, plus
	// eventualy some words which have to match with file name, we put a MAX input = 4096
	char* input = calloc(MAX_REQUEST_LEN, sizeof(char));
	if( input == NULL ){
		write_log(ERROR, "calloc of input failed");
		exit(1);
	}
	// read request from client->socket ( client socket ) and put in *input, 
	// if fail will returned true otherwise false,
	// the selctor is append to root directory so root+selector < MAX_REQUEST_LEN 
	if ((check = read_request(c->socket, input, (MAX_REQUEST_LEN-strlen((c->settings).homedir)) ))) {
		close_socket_kill_child(c, 0);
	}

	// if we are there, print that message
	write_log(DEBUG, "RECEIVED: \"%s\"\n%d bytes at %p\n",
			input, check, &input);

	// we have to add the path of gopher ROOT, else the client can access at all dir of server.
	c->path_file = (char*) calloc(PATH_MAX + 1, sizeof(char));
	if( c->path_file == NULL ){
		write_log(ERROR, "calloc of c->path_file failed");
		exit(1);
	}
	strcpy(c->path_file, (c->settings).homedir);
	// security check to avoid memory corruption
	
	int len_check = strlen(c->path_file) + strlen(input);
	if (len_check >= PATH_MAX) {
		write_log(ERROR, "Exceeded PATH_MAX length: requested path is %d chars long",
				len_check);
		close_socket_kill_child(c, 0);
	}
	strcat(c->path_file, input);
	write_log(DEBUG, "PATH+SELECTOR %d bytes: %s",
			strlen(c->path_file), c->path_file);

	// avoid trasversal path
	if (check_security_path(c->path_file)) {
		write_log(WARNING, "Path traversal detected in client's request: %s", c->path_file);
		close_socket_kill_child(c, 0);
	}
	type = type_path(c->path_file);

	// if is a dir we check the content if match with words 
	if (type == '1') {
		send_content_of_dir(c, input);
	} else {
		if (type == '3') { // if is an error send the error message
			char temp[(strlen(input) + 6)]; // 3 is for lenght of "3\t" + 1 per \n + 2 for last line + 1 \0
			strcpy(temp, "3\t");
			strcat(temp, input);
			strcat(temp, "\n.\n");
	#ifdef _WIN32	
			check = send(c->socket, temp, strlen(temp), 0);
			if( check == SOCKET_ERROR ){
				write_log(ERROR, "failed send ERROR message to %s becouse: %s", c->addr, WSAGetLastError());
			}
	#else
			check = send(c->socket, temp, strlen(temp), MSG_NOSIGNAL);
			if( check < 0 ){
				write_log(ERROR, "failed send ERROR message to %s becouse: %s", c->addr, strerror(errno));
			}
	#endif
		// close socket and thread
		} else { // if is only a file
			load_file_memory_and_send(c);
		}
	}
	free(input);
	close_socket_kill_child(c, 0);
	return 0;
}

// this function spawn thread to management the new client request 
void thread_management(client_args *client_info) {
#ifdef _WIN32
	HANDLE tHandle;
	print_client_args(client_info);
	LPDWORD lpThreadId = 0;

	tHandle = CreateThread( 
			NULL,            // default security attributes
			0,               // use default stack size  
			(LPTHREAD_START_ROUTINE) management_function, // thread function name
			client_info,     // argument to thread function 
			0,               // use default creation flags 
			lpThreadId       // returns the thread identifier 
	);
	
	if( CloseHandle(tHandle) == 0 ){
		write_log(ERROR, "Close tHandle in thread_managmente failed wirh error: %d",GetLastError());
	}
#else
	pthread_t tid;
	print_client_args(client_info);
	if( pthread_create(&tid, NULL, (void *) management_function, (void *) client_info) != 0 ){
		write_log(ERROR, "pthread_create( management_function ) failed ");
	}
	// we need to call pthread_detach so the thread will release its
	// resources (memory) on exit
	if( pthread_detach(tid) != 0 ){
		write_log(ERROR, "pthread_detach( %d ) failed ", tid);
	}
#endif
}

void close_socket(sock_t sd) {
#ifdef _WIN32
	closesocket(sd);
#else
	close(sd);
#endif
	write_log(DEBUG, "Closed socket %lld", sd);
}

void kill_thread(sock_t sd, int errcode) {
#ifdef _WIN32
	ExitThread(errcode);
#else
	pthread_exit(&errcode);
#endif
}

void close_socket_kill_child(client_args* c, int errcode) {
	char mode = (c->settings).mode;
	sock_t s = c->socket;
	free_client_args(c);
	if (mode == 't') {
		close_socket( s );
		kill_thread(s, 0);
	} else {
		exit(0);
	}
}
