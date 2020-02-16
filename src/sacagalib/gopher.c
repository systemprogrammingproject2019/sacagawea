#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>


#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <shlwapi.h>
#else
#include <pthread.h>
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


void send_content_of_dir(client_args* client_info, char* client_selector) {
	write_log(DEBUG, "send_content_of_dir: %s", client_info->path_file);

	// this fuction send each file in a directory which match "words" in the
	// gopher protocol format.
	DIR *folder;
	struct dirent *subFile;
	int check;
	int len_response;
	char type;
	char* response;
	char *path_of_subfile;
	char port_str[6]; // max ex "65535\0"
	// open dir 
	folder = opendir(client_info->path_file);
	if (folder == NULL) {
		write_log(ERROR, "failed open dir: %s", client_info->path_file);
		return;
	}

	while ((subFile = readdir(folder)) != NULL) {
		// skip .. and . file
		if ((strcmp(subFile->d_name , "..") == 0) || (strcmp( subFile->d_name , ".") == 0)) {
			continue;
		}
		// write_log(INFO, "%s", subFile->d_name);
		path_of_subfile = (char*) calloc( (strlen(client_info->path_file) + strlen(subFile->d_name) + 2), sizeof(char));
		if( path_of_subfile == NULL ){
			write_log(ERROR, "calloc of path_of_subfile failed");
			exit(1);
		}
	#ifdef _WIN32
		if (client_info->path_file[(strlen(client_info->path_file) - 1)] == '\\') {
	#else
		if (client_info->path_file[(strlen(client_info->path_file) - 1)] == '/') {
	#endif
			snprintf(path_of_subfile,
					(strlen(client_info->path_file) + strlen(subFile->d_name) + 1),
					"%s%s", client_info->path_file, subFile->d_name);
		} else {
			snprintf(path_of_subfile,
					(strlen(client_info->path_file) + strlen(subFile->d_name) + 2),
					"%s/%s", client_info->path_file, subFile->d_name);
		}

		type = type_path(path_of_subfile);
		// calculate lenght of response. first 2 are for type char + \t
		len_response = 2; 
		// for name of file +\t
		len_response += strlen(subFile->d_name) + 1; 
		// for selector, used for serch file in gopher server +\t, ( selector + '/' + file_name + '\t' )
		len_response += strlen(client_selector) + strlen(subFile->d_name) + 3;
		// for IP of server +\t
		len_response += strlen((client_info->settings).hostname) + 2;
		// for actualy opened (client_info->settings).port
		snprintf(port_str, 6, "%d", (client_info->settings).port);
		len_response += strlen(port_str);
		// \n + \0
		len_response += 2;
		// declare and compile

		response = (char*) malloc(len_response*sizeof(char));
		if( response == NULL ){
			write_log(ERROR, "malloc of response failed with error %s", strerror(errno));
			exit(1);
		}
		// SO dont care about path with double // but we used some gopher client for test the "server"
		// and putting all time a / at start of path, let it become more expansive, 
		// we got a path like C:/michele/Desktop/sacagawea/bin//////////////////0ciao
		// so we decided to put this check
		if (client_selector[(strlen(client_selector)-1)] != '/'){
			snprintf(response, len_response, "%c%s\t%s/%s\t%s\t%d\r\n",
					type, subFile->d_name, client_selector,
					subFile->d_name, (client_info->settings).hostname, (client_info->settings).port);
		} else {
			snprintf(response, len_response, "%c%s\t%s%s\t%s\t%d\r\n",
					type, subFile->d_name, client_selector,
					subFile->d_name, (client_info->settings).hostname, (client_info->settings).port);
		}

		// write_log(INFO, "send_content_of_dir response to socket %d: %s", client_info->socket, response);
	#ifdef _WIN32
		check = send(client_info->socket, response, strlen(response), 0);
		if( check == SOCKET_ERROR ){
			write_log(ERROR, "failed send %s to %s because: %s",response, client_info->addr, WSAGetLastError() );
		}
	#else
		//Requests not to send SIGPIPE on errors on stream oriented sockets when the other end breaks the connection. The EPIPE error is still returned.
		check = send(client_info->socket, response, strlen(response), MSG_NOSIGNAL);
		if( check < 0 ){
			if( errno == EPIPE ){
				write_log(ERROR, "failed send %s to %s because: %s",response, client_info->addr, strerror(errno));
				break;
			}else{
				write_log(ERROR, "failed send %s to %s because: %s",response, client_info->addr, strerror(errno));
			}
		}
	#endif
		free(response);
		free(path_of_subfile);
	}

	char end[] = ".\r\n";
#ifdef _WIN32	
	check = send(client_info->socket, end, strlen(end), 0);
	if( check == SOCKET_ERROR ){
		write_log(ERROR, "failed send end message to %s because: %s", client_info->addr, WSAGetLastError() );
	}
#else
	check = send(client_info->socket, end, strlen(end), MSG_NOSIGNAL);
	if( check < 0 ){
		write_log(ERROR, "failed send end message to %s because: %s", client_info->addr, strerror(errno));
	}
#endif
	write_log(DEBUG, "send_content_of_dir response to socket %d: SENT", client_info->socket);

	if ( closedir(folder) == -1 ){
		write_log(ERROR, "failed close dir: %s", client_info->path_file);
	}
}


// This fuction management the thread which have to send the FILE at client
void *thread_sender(client_args* c) {
	// this cicle send the file at client and save the number of bytes sent 
	size_t bytes_sent = 0;
	// ssize_t is used for the range [-1, SSIZE_MAX], to return a size in bytes or a negative error
	ssize_t temp;

#ifdef _WIN32
	SYSTEM_INFO sysnfo;
	GetSystemInfo(&sysnfo);

	// In WIN32, "send" has an "int" as its 3rd parameter (size) instead of a
	// size_t. Due to us needing to support the sending of files with size
	// greater than INT_MAX, we will need to do multiple "send"s.
	// Also, these dimensions need to be a multiple of the page size of the OS
	// (dwAllocationGranularity) because reads done in MapViewOfFile need to be
	// aligned to the page length. We have therefore chosen to use 
	// sysnfo.dwAllocationGranularity as our buffer length because trivially
	// it's a multiple of sysnfo.dwAllocationGranularity
#else
	// stop the SIGHUP
	if( signal(SIGHUP,SIG_IGN) == SIG_ERR ){
		write_log(ERROR, "Signal() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
	

	while (bytes_sent < c->len_file) {
		// logic for sending the file
	#ifdef _WIN32

		char* pBuf = (LPTSTR) MapViewOfFile(
				c->file_to_send,      // handle to map object
				FILE_MAP_READ,        // read permission
				HIDWORD(bytes_sent),
				LODWORD(bytes_sent),
				min(sysnfo.dwAllocationGranularity, c->len_file - bytes_sent)
		);
		if (pBuf == NULL) {
			write_log(ERROR, "MapViewOfFile on %d failed wirh error: %d",
					c->file_to_send, GetLastError());
			return false;
		}

		// In windows, pages read from MapViewOfFile need to be aligned
		// to the granularity of the pages in memory.
		// In order to comply to this restriction, we divide the sending
		// process into rounds. In each of these rounds, we either send one
		// page or, if less than one page remains, send all.
		temp = send(c->socket, pBuf,
				min(sysnfo.dwAllocationGranularity,
				c->len_file - bytes_sent), 0);
		if (temp == SOCKET_ERROR) {
			write_log(ERROR, "Sending file to %s, with socket %d failed with error: %d",
					c->addr, c->socket, WSAGetLastError());
			if( UnmapViewOfFile(pBuf) == 0 ){
				write_log(ERROR, "UnmapViewOfFile failed with error: %I64d",GetLastError());
			}else{
				write_log(DEBUG, "UnmapViewOfFile on %d", c->file_to_send);
			} 
			ExitThread(0);
		} else {
			bytes_sent += temp;
		}
	#else
		// MSG_NOSIGNAL means, if the socket be broken dont send SIGPIPE at process
		temp = send(c->socket, &(c->file_to_send[bytes_sent]) , (c->len_file - bytes_sent), MSG_NOSIGNAL);
		if (temp < 0) {
			write_log(ERROR, "Sending file to %s, with socket %d failed: %s",
					c->addr, c->socket, strerror(errno));
			pthread_exit(0);
		} else { 
			if (temp == 0) { // this if can be deleted
				write_log(ERROR, "Client %s, with socket %d close the connection meanwhile sending file\n",
						c->addr, c->socket);
				pthread_exit(0);
			} else {
				bytes_sent += temp;
			}
		}
	#endif
	#ifdef _WIN32
		if( UnmapViewOfFile(pBuf) == 0 ){
			write_log(ERROR, "UnmapViewOfFile failed with error: %I64d",GetLastError());
		}else{
			write_log(DEBUG, "UnmapViewOfFile on %d", c->file_to_send);
		}
	#endif
		write_log(DEBUG, "sent %lld/%lld bytes\n", bytes_sent, c->len_file);
	}
	
	char buff;
	// close the sockets gracefully
#ifdef _WIN32
	if (shutdown(c->socket, SD_SEND) != 0) {
		write_log(ERROR, "shutdown on socket failed with error: %I64d",
				WSAGetLastError());
		ExitThread(0);
	}

	int ret;
	do {
		ret = recv(c->socket, &buff, sizeof(buff), MSG_WAITALL);
		if (ret < 0) {
			write_log(ERROR, "recv() failed: %s", WSAGetLastError());
			ExitThread(0);
		}
	} while(ret != 0);

#else
	/* allora qua sicuramente c'è una soluzione migliore, questa l'ho inventata io ma sembra veramente inteligente come cosa.
	allora curl legge finche il socket è aperto. quindi quando inviavo il file anche se inviato tutto
	lui leggeva aspettanto altri bytes. pertanto faccio lo shutdown ovvero chiudo il socket in scrittura
	dal lato server, cosi curl quando finisce di leggere i bytes inviati si blocca e chiude la comunicazione */ 
	if (shutdown(c->socket, SHUT_WR) < 0) {
		write_log(ERROR, "shutdown() failed: %s\n", strerror(errno));
		pthread_exit(0);
	}
	/* come detto prima curl finisce la comunicazione quando legge tutto, ma noi non sappiamo quando ha finito
	quindi faccio questo while che cerca di fare una recv, quando la recv ritorna 0 vuol dire che la curl ha chiuso 
	la connessione e quindi ha finito di leggere il file. pertanto posso chiudere definitivamente il socket e il thread */
	int ret;
	do {
		ret = recv(c->socket, &buff, sizeof(buff), MSG_WAITALL);
		if (ret < 0) {
			write_log(ERROR, "recv() failed: %s", strerror(errno));
			pthread_exit(0);
		}
	} while(ret != 0);
#endif

	// prepare the message for the logging process
	if (bytes_sent == c->len_file) {
		// get string to write on logs file
		long len_logs_string = 0;
		char* ds = date_string();
		len_logs_string += strlen(ds) + 1; // for date string + 1 space
		len_logs_string += strlen(c->path_file) + 1;
		// 18.446.744.073.709.551.616 is the max lenght of size_t ( win/ unix) 64 bit
		// we just allocate 21 bytes for the max case plus 3 for unit of measure
		len_logs_string += 24;
		len_logs_string += strlen(c->addr) + 2;
		
		char* logs_string = malloc(sizeof(char) * len_logs_string);
		if (logs_string == NULL) {
		#ifdef _WIN32
			write_log(ERROR, "malloc() failed with error: %d", GetLastError());
			ExitThread(0);
		#else
			write_log(ERROR, "malloc() failed: %s", strerror(errno));
			pthread_exit(NULL);
		#endif
		}

		#ifdef _WIN32
		if (snprintf(logs_string, len_logs_string, "%s %s %I64u B %s\n", ds,
				c->path_file, c->len_file, c->addr) < 0) {
			write_log(ERROR, "snprintf() failed with error: %d", GetLastError());
			ExitThread(0);
		#else
		if (snprintf(logs_string, len_logs_string, "%s %s %zu B %s\n", ds,
				c->path_file, c->len_file, c->addr) < 0) {
			write_log(ERROR, "snprintf() failed: %s", strerror(errno));
			pthread_exit(NULL);
		#endif
		}
		free(ds);
		// sent logs string, first we sent the strlen after the string
		len_logs_string = strlen(logs_string);

	#ifdef _WIN32
		HANDLE hPipe; 
		BOOL   fSuccess = FALSE; 
		DWORD  cbWritten, dwMode; 

		// Try to open the named pipe;
		hPipe = CreateFile(
			WIN32_PIPE_NAME,   // pipe name
			GENERIC_READ |  // read and write access
			GENERIC_WRITE,
			0,              // no sharing
			NULL,           // default security attributes
			OPEN_EXISTING,  // opens existing pipe
			0,              // default attributes
			NULL            // no template file
		);
		// Break if the pipe handle is valid.
		if (hPipe == INVALID_HANDLE_VALUE && GetLastError() != ERROR_PIPE_BUSY) {
			write_log(ERROR, "Could not open pipe. CreateFile failed with error: %ld\n",GetLastError());
			ExitThread(0);
		}

		// All pipe instances are busy, so wait for 10 seconds.
		if (!WaitNamedPipe(WIN32_PIPE_NAME, 10000)) {
			write_log(ERROR, "Could not open pipe: 10 second wait timed out.");
			ExitThread(0);
		}
		// The pipe connected; change to message-read mode.
		dwMode = PIPE_READMODE_MESSAGE;
		fSuccess = SetNamedPipeHandleState(
				hPipe,    // pipe handle
				&dwMode,  // new pipe mode
				NULL,     // don't set maximum bytes
				NULL      // don't set maximum time
		);
		if (!fSuccess) {
			write_log(ERROR, "SetNamedPipeHandleState failed with error: %ld\n",
					GetLastError());
			ExitThread(0);
		}

		fSuccess = WriteFile(
				hPipe,      // pipe handle
				logs_string, // message
				len_logs_string,  // message length
				&cbWritten, // bytes written
				NULL        // not overlapped
		);
		if (!fSuccess) {
			write_log(ERROR, "WriteFile to pipe failed with error: %ld", GetLastError()); 
			ExitThread(0);
		}

		if( CloseHandle(hPipe) == 0 ){
			write_log(ERROR, "Close hPipe failed wirh error: %d", GetLastError());
		}
	#else
		// lock mutex and wake up process for logs
		if( pthread_mutex_lock( &(condVar->mutex) ) != 0 ){
			write_log(ERROR, "pthread_mutex_lock(mutex) fail on thread_sender, dont saved on logs");
			free(logs_string);
			pthread_exit(NULL);
		}
		// upgrate signal cont
		condVar->cont++;

		if (write(condVar->pipe_conf[1], &len_logs_string, sizeof(int)) < 0) {
			write_log(ERROR, "System call write() failed because of %s", strerror(errno));
	 		pthread_exit(NULL);
		}	
		if (write(condVar->pipe_conf[1], logs_string, len_logs_string) < 0) {
			write_log(ERROR, "System call write() failed because of %s", strerror(errno));
	 		pthread_exit(NULL);
		}
		// unlock mutex and free
		if( pthread_cond_signal( &(condVar->cond) ) != 0 ){
			write_log(ERROR, "pthread_cond_signal(cond) fail on thread_sender");
		}
		if( pthread_mutex_unlock( &(condVar->mutex) ) != 0 ){
			write_log(ERROR, "pthread_mutex_unlock(mutex) fail on thread_sender"); // but thread will be closed so the mutex released
		}
	#endif
		free(logs_string);
	}
#ifdef _WIN32
	ExitThread(0);
#else
	pthread_exit(NULL);
#endif
}

// this function take a path as argument and return the gopher char associated.
// in the Gopher.md u can see all gopher char and the translate
char type_path(char* path) {

#ifdef _WIN32
	DWORD f;
	if ((f = GetFileAttributes(path)) == INVALID_FILE_ATTRIBUTES) {
		write_log(WARNING, "Path %s doesn't exist", path);
		return '3';
	}

	if (PathIsDirectoryA(path)) {
		// write_log(INFO, "Path %s is a directory", path);
		return '1';
	}

	LPCSTR extension = PathFindExtensionA(path);
	if (!strcmp(extension, ".exe") || !strcmp(extension, ".bin")
			|| !strcmp(extension, ".dll") || !strcmp(extension, ".so")
			|| !strcmp(extension, ".out") || !strcmp(extension, ".lib")
			|| !strcmp(extension, ".zip") || !strcmp(extension, ".gz")
			|| !strcmp(extension, ".tgz") || !strcmp(extension, ".bzip2")
			|| !strcmp(extension, ".a")   || !strcmp(extension, ".iso")
			|| !strcmp(extension, ".7z")  || !strcmp(extension, ".pdf")) {
		return '9';
	}
	if (!strcmp(extension, ".htm")) {
		return 'h';
	}
	if (!strcmp(extension, ".gif")) {
		return 'g';
	}
	if (!strcmp(extension, ".mp3") || !strcmp(extension, ".ogg")
			|| !strcmp(extension, ".wav") || !strcmp(extension, ".aiff")
			|| !strcmp(extension, ".mp4") || !strcmp(extension, ".mpg")
			|| !strcmp(extension, ".avi") || !strcmp(extension, ".webm")) {
		return 's';
	}
	if (!strcmp(extension, ".jpg") || !strcmp(extension, ".jpeg")
			|| !strcmp(extension, ".png") || !strcmp(extension, ".tiff")) {
		return 'I';
	}
	// write_log(INFO, "Path %s is a file", path);
	return '0';
#else
	char* sanitized_path = sanitize_path(path);

	// we check the tipe or file with file bash command
	char command[(strlen(sanitized_path) + 12)];  // 10 for "file -bi '" + 2 for "'\0"
	// file with -b option: 
	strcpy(command, "file -bi '");
	strcat(command, sanitized_path);
	free(sanitized_path); // free as soon as we dont need it anymore
	strcat(command, "'");
	FILE* popen_output_stream = popen(command , "r");
	if (popen_output_stream == NULL) { 
		write_log(ERROR,"popen() failed: %s\n", strerror(errno));
	 	exit(5);
	}
	char popen_output[64]; // is useless read all output, i need only the first section and the max is "application/gopher"

	if (fgets((char *) &popen_output, 64, popen_output_stream) == NULL) {
		if (!feof(popen_output_stream)) {
			write_log(ERROR, S_ERROR_FGETS, strerror(errno));
			exit(5);
		}
	}
	//fprintf( stdout, "%s\n", popen_output); 
	if (strncmp(popen_output, "cannot", 6) == 0) {
		write_log(DEBUG, "%s", popen_output);
		while (fgets((char *) &popen_output, 64, popen_output_stream) != NULL) {
			write_log(DEBUG, "%s", popen_output); 
		}
	}
	pclose(popen_output_stream);


	if( strncmp(popen_output, DIR_1, strlen(DIR_1)) == 0 ) {
		return '1';
	}
	if( strncmp(popen_output, GOPHER_1, strlen(GOPHER_1)) == 0 ) {
		return '7';
	}
	if( strncmp(popen_output, MULTIPART_M, strlen(MULTIPART_M)) == 0) {
		return 'M';
	}
	if( strncmp(popen_output, APPLICATION_9, strlen(APPLICATION_9)) == 0) {
		return '9';
	}
	if( strncmp(popen_output, AUDIO_s, strlen(AUDIO_s)) == 0 ) {
		return 's';
	}
	if( strncmp(popen_output, HTML_h, strlen(HTML_h)) == 0 ) {
		return 'h';
	}
	if( (strncmp(popen_output, TEXT_0, strlen(TEXT_0)) == 0) || (strncmp(popen_output, EMPTY_0, strlen(EMPTY_0)) == 0)) {
		return '0';
	}
	if( strncmp(popen_output, GIF_g, strlen(GIF_g)) == 0) {
		return 'g';
	}
	if( strncmp(popen_output, IMAGE_I, strlen(IMAGE_I)) == 0) {
		return 'I';
	}
	if( strncmp(popen_output, MAC_4, strlen(MAC_4)) == 0) {
		return '4';
	}
	return '3';
#endif
}

#ifndef _WIN32
// In this function we replace every single ' with '\''
char* sanitize_path(const char* input) {
	int count = 0;
	// count how many ' are in the input
	for(const char* p = input; *p; count += (*p++ == '\''));

	int pathlen = strlen(input);
	// final len is pathlen + (count*3) + 1 because for each ' 
	// in the input we are adding another 3 chars, plus 1 char for
	// the final nil char
	char* ret = calloc(pathlen + (count*3) + 1, sizeof(char));
	if( ret == NULL ) {
		write_log(ERROR,"calloc() failed: %s\n", strerror(errno));
	 	exit(5);
	}
	int ret_index = 0;
	for (int i = 0; i < pathlen; i++) {
		if (input[i] == '\'') {
			ret[ret_index+0] = '\'';
			ret[ret_index+1] = '\\';
			ret[ret_index+2] = '\'';
			ret[ret_index+3] = '\'';
			ret_index += 4;
		} else {
			ret[ret_index] = input[i];
			ret_index += 1;
		}
	}
	// write_log(DEBUG, "SANITIZED = %s", ret);
	return ret;
}
#endif
