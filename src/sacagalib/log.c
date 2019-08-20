#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

#ifndef _WIN32
#include <pthread.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#endif

#include "sacagalib.h"

#ifdef _WIN32
HFILE* log_file;
#else
FILE* log_file;
#endif

void log_management() {
	printf("Process for sacagawea.log created\n");
	char *read_line;
	int len_string, check;

	// open logs file and check if an error occured
	#ifdef _WIN32
	log_file = CreateFileA (
		SACAGAWEALOGS_PATH,     // file name
		FILE_APPEND_DATA,       // open for appending
		FILE_SHARE_READ,        // share for reading only
		NULL,                   // default security
		OPEN_ALWAYS,            // open existing file or create new file 
		FILE_ATTRIBUTE_NORMAL,  // normal file
		NULL                    // no attr. template
	);
	if (log_file == INVALID_HANDLE_VALUE) {
		write_log(ERROR, "CreateFileA failed. Unable to open \"%s\".\n", SACAGAWEALOGS_PATH);
		return;
	}
	#else
	log_file = fopen(SACAGAWEALOGS_PATH , "a");
	if(log_file == NULL){
		fprintf(stderr, S_ERROR_FOPEN, strerror(errno));
		exit(5);
	}
	#endif

	while (true) {
#ifdef _WIN32
		// TODO
		break;
#else
		pthread_mutex_lock(mutex);
		// this while check if pipe is readable (contains something). if == 1 is readable else we go sleep
		/*while (poll(&(struct pollfd){ .fd = pipe_conf[0], .events = POLLIN }, 1, 0) != 1) {
			pthread_cond_wait(cond, mutex);
		}*/
		/* this while check if pipe is readable, or dont contain nothing.
		if is empty return error EWOULDBLOCK and go again in blocked mode. */
		while( true ){
			fprintf(stdout, "read\n", check);
			check = read(pipe_conf[0] , &len_string, sizeof(int));
			fprintf(stdout, "check: %d\n", check);
			if( check < 0){
				if( errno == EWOULDBLOCK ){
					pthread_cond_wait(cond, mutex);
					fprintf(stdout, "LOGS Process nothing\n");
					sleep(1);
				}else{
					fprintf(stdout, "LOGS Process terminate\n");
					pthread_mutex_destroy(mutex);
					pthread_cond_destroy(cond);
					shm_unlink(SHARED_MUTEX_MEM);
					shm_unlink(SHARED_COND_MEM);
					close( pipe_conf[0] );
					exit(1);
				}
			}
			if( check > 0){
				fprintf(stdout, "LOGS Process receive something\n");
				break;
			}
			if( check == 0){
				pthread_mutex_unlock(mutex);
				exit(1);
			}
		}

		// read pipe and write sacagawea.log, until we got \n
		read_line = (char*) malloc( (len_string+1)*sizeof(char) );
		read(pipe_conf[0] , read_line , len_string );
		read_line[len_string]='\0';
		fprintf(stdout, "received: %d, %s",len_string, read_line);
		fprintf(log_file, "%s", read_line);
		
		free(read_line);
		pthread_mutex_unlock(mutex);
#endif
	}
#ifdef _WIN32
	CloseHandle(log_file);
#else
	fclose(log_file);
#endif
}

void write_log(int log_lv, const char* error_string, ...) {
	va_list args;
	va_start(args, error_string);
	#ifdef _WIN32
	DWORD dwBytesToWrite;
	DWORD dwBytesWritten;
	#else
	#endif

	if (log_lv <= LOG_LEVEL) {
		char* log_string = malloc(1024);
		time_t rawtime;
		struct tm* timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);

		char* timestr = asctime(timeinfo);
		char month[4] = {timestr[0], timestr[1], timestr[2], 0};

		snprintf(log_string, 2048, "%s %02d %02d:%02d:%02d %s: %s\n",
				month, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min,
				timeinfo->tm_sec, "sagacawea", error_string);
		#ifdef _WIN32
		dwBytesToWrite = strlen(log_string);
		BOOL bErrorFlag = WriteFile( 
				log_file,        // open file handle
				log_string,      // start of data to write
				dwBytesToWrite,  // number of bytes to write
				&dwBytesWritten, // number of bytes that were written
				NULL             // no overlapped structure
		);
		if (FALSE == bErrorFlag) {
			printf("ERROR: Unable to write to log file.\n");
		} else if (dwBytesWritten != dwBytesToWrite) {
				// This is an error because a synchronous write that results in
				// success (WriteFile returns TRUE) should write all data as
				// requested. This would not necessarily be the case for
				// asynchronous writes.
				printf("Error: dwBytesWritten != dwBytesToWrite\n");
		}
		#else
		vfprintf(stderr, log_string, args);
		#endif
		free(log_string);
	}
	va_end(args);
}