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

const char log_lv_name[][10] = {"ERROR", "WARNING", "INFO", "DEBUG"};

void log_management() {
	write_log(INFO, "Process for sacagawea.log created");
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
		write_log(ERROR, "CreateFileA failed. Unable to open \"%s\"", SACAGAWEALOGS_PATH);
		return;
	}
	#else
	FILE *fp;
	
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
		while (true) {
			//fprintf(stdout, "read\n", check);
			check = read(pipe_conf[0] , &len_string, sizeof(int));
			write_log(INFO, "check bytes in logs pipe: %d", check);
			if (check < 0) {
				if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
					write_log(INFO, "LOGS Process nothing");
					pthread_cond_wait(cond, mutex);
				} else {
					write_log(INFO, "LOGS Process terminate");
					pthread_mutex_destroy(mutex);
					pthread_cond_destroy(cond);
					shm_unlink(SHARED_MUTEX_MEM);
					shm_unlink(SHARED_COND_MEM);
					close( pipe_conf[0] );
					exit(1);
				}
			}
			if (check > 0) {
				write_log(INFO, "LOGS Process receive something");
				break;
			}
			if (check == 0) {
				if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
					write_log(INFO, "LOGS Process nothing");
					pthread_cond_wait(cond, mutex);
				} else {
					pthread_mutex_unlock(mutex);
					return;
				}
				write_log(INFO, "ATTENZIONE\nATTENZIONE\nATTENZIONE\nricorda che a volte da 0 come return di read() della pipe del processo di logs e dovevamo capire se fosse EPIPE (Broken pipe) oppure qualcosa che indica che semplicemente ha letto 0 byte. ::   %s\n", strerror(errno));
			}
		}

		log_file = fopen(SACAGAWEALOGS_PATH , "a");
		if (log_file == NULL) {
			fprintf(stderr, S_ERROR_FOPEN, strerror(errno));
			exit(5);
		}

		// read pipe and write sacagawea.log, until we got \n
		read_line = (char*) malloc((len_string+1) * sizeof(char));
		read(pipe_conf[0], read_line, len_string);
		read_line[len_string]='\0';
		write_log(INFO, "received: %d, %s",len_string, read_line);
		fprintf(log_file, "%s", read_line);
		
		fclose(log_file);
		free(read_line);
		pthread_mutex_unlock(mutex);
#endif
	}
#ifdef _WIN32
	CloseHandle(log_file);
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

	char* log_string = malloc(1024);
	char* formatted_error_string = malloc(960);
	time_t rawtime;
	struct tm* timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	char* timestr = asctime(timeinfo);
	char month[4] = {timestr[4], timestr[5], timestr[6], 0};

	vsnprintf(formatted_error_string, 960, error_string, args);

	char* ds = date_string();
	if (snprintf(log_string, 1024, "%s %s: %s\n", ds, log_lv_name[log_lv],
			formatted_error_string) < 0) {
		write_log(ERROR, "snprintf() failed: %s\n", strerror(errno));
	}
	free(ds);

	if (log_lv <= WARNING) {
		fprintf(stderr, log_string);
	} else if (log_lv <= LOG_LEVEL) {
		fprintf(stdout, log_string);
		// return;
	}

	free(log_string);
	
	va_end(args);
}

// returned string needs to be freed
char* date_string() {
	int len_str = 22; // for "[MMM DD YYYY hh:mm:ss]"
	char* r = malloc(len_str+1);

	time_t rawtime;
	struct tm* timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	char* timestr = asctime(timeinfo);
	char month[4] = {timestr[4], timestr[5], timestr[6], 0};
	if (snprintf(r, len_str+1, "[%s %02d %04d %02d:%02d:%02d]",
			month, timeinfo->tm_mday, timeinfo->tm_year + 1900, 
			timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec) < 0) {
		write_log(ERROR, "snprintf() failed: %s\n", strerror(errno));
		exit(5);
	}
	return r;
}