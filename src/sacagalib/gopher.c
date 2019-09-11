#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#ifdef _WIN32
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

void send_content_of_dir(client_args *client_info, selector *client_selector) {
	write_log(INFO, "%s", client_info->path_file);
// #ifdef _WIN32

// #else
// this fuction send each file in a directory which match "words" in the gopher protocol format.

	DIR *folder;
	struct dirent *subFile;
	int j = 0;
	int len_response;
	char type;
	int no_match = false;
	char* response;
	char *path_of_subfile;
	char port_str[6]; // max ex "65000\0"
	// open dir 
	folder = opendir(client_info->path_file);
	if (folder == NULL) {
		close_socket_kill_process( SERVER_SOCKET, 5);
	}

	while ((subFile = readdir(folder)) != NULL) {
		// skip .. and . file
		if ((strcmp(subFile->d_name , "..") == 0) || (strcmp( subFile->d_name , ".") == 0)) {
			continue;
		}
		/* words are only strings and not regex, so i do that little check for take only 
		subfile who match all words, regexes would be useless and a waste of resources */
		// check word by word if match, if someone don't match we break the for, and don't send the gopher string of file
		for (j = 0; j <= client_selector->num_words; j++) {
			no_match = false;
			// check if not match
			if (check_not_match( subFile->d_name, client_selector->words[j])) {
				no_match = true;
				break;
			}
		}

		if (no_match) { // file don't match something, continue with next subfile
			continue;
		} else {
			// the file match all word we send the data
			write_log(INFO, "%s", subFile->d_name);
			path_of_subfile = (char*) calloc((strlen(client_info->path_file) + strlen(subFile->d_name) + 2), sizeof(char));
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
						(strlen(client_info->path_file) + strlen(subFile->d_name) + 1),
						"%s/%s", client_info->path_file, subFile->d_name);
			}

			type = type_path(path_of_subfile);
			// calculate lenght of response. first 2 are for type char + \t
			len_response = 2; 
			// for name of file +\t
			len_response += strlen(subFile->d_name) + 1; 
			// for selector, used for serch file in gopher server +\t, ( selector + '/' + file_name + '\t' )
			len_response += strlen(client_selector->selector) + strlen(subFile->d_name) + 2;
			// for IP of server +\t
			len_response += strlen(SERVER_DOMAIN) + 1;
			// for actualy opened (client_info->settings).port
			snprintf(port_str, 6, "%d", (client_info->settings).port);
			len_response += strlen( port_str );
			// \n + \0
			len_response += 4;
			// declare and compile

			response = (char*) malloc(len_response*sizeof(char));
			if (client_selector->selector[(strlen(client_selector->selector)-1)] != '/'){
				snprintf(response, len_response, "%c%s\t%s/%s\t%s\t%d\n",
						type, subFile->d_name, client_selector->selector,
						subFile->d_name, SERVER_DOMAIN, (client_info->settings).port);
			} else {
				snprintf(response, len_response, "%c%s\t%s%s\t%s\t%d\n",
						type, subFile->d_name, client_selector->selector,
						subFile->d_name, SERVER_DOMAIN, (client_info->settings).port);
			}

			write_log(INFO, "response at %d: %s", client_info->socket, response);
			send(client_info->socket, response, strlen(response), 0);

			free(response);
			free(path_of_subfile);
		}
	}
	char end[] = ".\n";
	send(client_info->socket, end, strlen(end), 0);
	closedir(folder);

	close(client_info->socket);
// #endif
}


// This fuction management the thread which have to send the FILE at client
void *thread_sender(client_args* c) {
	/* this cicle send the file at client and save the number of bytes sent */
	long bytes_sent = 0, temp;
	while (bytes_sent < c->len_file) {
		// logic for sending the file
	#ifdef _WIN32
		write_log(DEBUG, "c->file_to_send: %s", c->file_to_send);
		HANDLE hMapFile = OpenFileMappingA(
				FILE_MAP_READ,   // read/write access
				FALSE,           // do not inherit the name
				c->file_to_send  // name of mapping object
		);
		if (hMapFile == NULL) {
			write_log(ERROR, "OpenFileMappingA failed wirh error: %d",
					GetLastError());
			return false;
		}
		char* pBuf = (LPTSTR) MapViewOfFile(
				hMapFile,      // handle to map object
				FILE_MAP_READ, // read/write permission
				0,
				HIDWORD(c->len_file),
				0
		);
		if (pBuf == NULL) {
			write_log(ERROR, "MapViewOfFile failed wirh error: %d",
					GetLastError());
			return false;
		}
		temp = send(c->socket, pBuf, (c->len_file - bytes_sent), 0);
		if (temp == SOCKET_ERROR) {
			write_log(ERROR, "Sending file at %s, with socket %d failed with error: %d\n",
					c->addr, c->socket, WSAGetLastError());
		}
		UnmapViewOfFile(pBuf);
		CloseHandle(hMapFile);
	#else
		// MSG_NOSIGNAL means, if the socket be broken dont send SIGPIPE at process
		temp = send(c->socket, c->file_to_send, (c->len_file - bytes_sent), MSG_NOSIGNAL);
		if (temp < 0) {
			write_log(ERROR, "Sending file at %s, with socket %d failed because of: %s\n",
					c->addr, c->socket, strerror(errno));
			close_socket_kill_thread(c->socket, 5);
		}
		if (temp == 0) {
			write_log(ERROR, "Client %s, with socket %d close the connection meanwhile sending file\n",
					c->addr, c->socket);
			close_socket_kill_thread(c->socket, 5);
		}
	#endif
		bytes_sent += temp;
		write_log(INFO, "sent %ld bytes\n", bytes_sent);
	}

	// at end we send a \n, useless for the fuction of gopher server
	char c2[] = "\n";
	send(c->socket, c2, strlen(c2), 0);

	// write to sacagawea.log through the pipe
#ifdef _WIN32
	closesocket(c->socket);
#else
	/* allora qua sicuramente c'è una soluzione migliore, questa l'ho inventata io ma mi sembra veramente inteligente come cosa.
	allora curl legge finche il socket è aperto. quindi quando inviavo il file anche se inviato tutto
	lui leggeva aspettanto altri bytes. pertanto faccio lo shutdown ovvero chiudo il socket in scrittura
	dal lato server, cosi curl quando finisce di leggere i bytes inviati si blocca e chiude la comunicazione */ 
	if (shutdown(c->socket, SHUT_WR) < 0) {
		write_log(ERROR, "shutdown() failed: %s\n", strerror(errno));
		close_socket_kill_thread(c->socket, 5);
	}
	/* come detto prima curl finisce la comunicazione quando legge tutto, ma noi non sappiamo quando ha finito
	quindi faccio questo while che cerca di fare una recv, quando la recv ritorna 0 vuol dire che la curl ha chiuso 
	la connessione e quindi ha finito di leggere il file. pertanto posso chiudere definitivamente il socket e il thread */
	char ciao[11];
	int check;
	while ((check = recv(c->socket, ciao, 10, MSG_WAITALL)) != 0) {
	// ho fatto questo loop perche se il client manda più di 10 byte la recv termina anche se ha il MSG_WAITALL
		//fprintf(stdout, "- %d - %s\n" , check, ciao);
		sleep(0.5);
	}
	if (check < 0) {
		write_log(ERROR, "recv() failed: %s\n", strerror(errno));
		close_socket_kill_thread(c->socket, 5);
	}
	close(c->socket);
	
	// if we sent all the file without error we wake up logs_uploader
	write_log(INFO, "file length %ld\n", bytes_sent, c->len_file);

	if (bytes_sent >= c->len_file) {
		// get string to write on logs file
		long len_logs_string;
		char* ds = date_string();
		len_logs_string += strlen(ds) + 1; // for date string + 1 space
		len_logs_string += strlen(c->path_file) + 1;
		len_logs_string += strlen(c->addr) + 4;
		char *logs_string = malloc(sizeof(char) * len_logs_string);

		if (logs_string == NULL) {
			write_log(ERROR, "malloc() failed: %s\n", strerror(errno));
			pthread_exit(NULL);
		}
		if (snprintf(logs_string, len_logs_string, "%s %s %s\n", ds,
				c->path_file, c->addr) < 0) {
			write_log(ERROR, "snprintf() failed: %s\n", strerror(errno));
			pthread_exit(NULL);
		}
		free(ds);

		// lock mutex and wake up process for logs
		pthread_mutex_lock(mutex);
		// sent logs string, first we sent the strlen after the string
		len_logs_string = strlen( logs_string );
		write_log(INFO, "HERE %ld: %s\n", len_logs_string, logs_string);
		if (write(pipe_conf[1], &len_logs_string, sizeof(int)) < 0) {
			write_log(ERROR, "System call write() failed because of %s", strerror(errno));
	 		exit(5);
		}	
		if (write(pipe_conf[1], logs_string, len_logs_string) < 0) {
			write_log(ERROR, "System call write() failed because of %s", strerror(errno));
	 		exit(5);
		}	
		// unlock mutex and free
		pthread_cond_signal(cond);
		pthread_mutex_unlock(mutex);
		free(logs_string);
	}
	
	pthread_exit(NULL);
#endif
}

// this function take a path as argument and return the gopher char associated.
// in the Gopher.md u can see all gopher char and the translate
char type_path(char path[PATH_MAX]) {

#ifdef _WIN32
	DWORD f;
	if ((f = GetFileAttributes(path)) == INVALID_FILE_ATTRIBUTES) {
		write_log(WARNING, "Path %s doesn't exist", path);
		return '3';
	}

	if (PathIsDirectoryA(path)) {
		write_log(INFO, "Path %s is a directory", path);
		return '1';
	}
	write_log(INFO, "Path %s is a file", path);
	return '0';
#else
	// we check the tipe or file with file bash command
	char command[(strlen(path) + 10)];
	// file with -b option: 
	strcpy(command, "file -bi "); // 9 for "file -bi " +1 for \0 at end
	strcat(command, path);
	FILE* popen_output_stream = popen(command , "r");
	if (popen_output_stream == NULL) { 
		write_log(ERROR,"popen() failed: %s\n", strerror(errno));
	 	close_socket_kill_process( SERVER_SOCKET, 5);
	}
	char popen_output[20]; // is useless read all output, i need only the first section and the max is "application/gopher"

	if (fgets((char *) &popen_output, 20, popen_output_stream) == NULL) {
		if ( !feof(popen_output_stream)) {
			write_log(ERROR, S_ERROR_FGETS, strerror(errno));
			close_socket_kill_process( SERVER_SOCKET, 5);
		}
	}
	//fprintf( stdout, "%s\n", popen_output); 
	if (strncmp(popen_output, "cannot", 6) == 0) {
		write_log(INFO, "%s", popen_output);
		while (fgets((char *) &popen_output, 20, popen_output_stream) != NULL) {
			fprintf(stdout, "%s", popen_output); 
		}
		fprintf(stdout, "\n"); 
	}
	pclose(popen_output_stream);
	

	if ((strncmp(popen_output, DIR_1, strlen(DIR_1)) == 0) || (strncmp(popen_output, GOPHER_1, strlen(GOPHER_1)) == 0)) {
		return '1';
	}
	if (strncmp(popen_output, MULTIPART_M, strlen(MULTIPART_M)) == 0) {
		return 'M';
	}
	if (strncmp(popen_output, APPLICATION_9, strlen(APPLICATION_9)) == 0) {
		return '9';
	}
	if (strncmp(popen_output, AUDIO_s, strlen(AUDIO_s)) == 0 ) {
		return 's';
	}
	if (strncmp(popen_output, HTML_h, strlen(HTML_h)) == 0 ) {
		return 'h';
	}
	if ((strncmp(popen_output, TEXT_0, strlen(TEXT_0)) == 0) || (strncmp(popen_output, EMPTY_0, strlen(EMPTY_0)) == 0)) {
		return '0';
	}
	if (strncmp(popen_output, GIF_g, strlen(GIF_g)) == 0) {
		return 'g';
	}
	if (strncmp(popen_output, IMAGE_I, strlen(IMAGE_I)) == 0) {
		return 'I';
	}
	if (strncmp(popen_output, MAC_4, strlen(MAC_4)) == 0) {
		return '4';
	}
	return '3';
#endif
}
