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

#ifndef _WIN32
void log_management() {
	write_log(INFO, "Process for sacagawea.log created");

	char *read_line;
	int len_string, check;

	// open logs file and check if an error occured
	while (true) {
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
				write_log(INFO, "ATTENZIONE\nATTENZIONE\nATTENZIONE\nricorda che a volte da 0 come return di read() della pipe del processo di logs e dovevamo capire se fosse EPIPE (Broken pipe) oppure qualcosa che indica che semplicemente ha letto 0 byte. ::   %s", strerror(errno));
			}
		}

		log_file = fopen(SACAGAWEALOGS_PATH , "a");
		if (log_file == NULL) {
			fprintf(stderr, S_ERROR_FOPEN, strerror(errno));
			exit(5);
		}

		// read pipe and write sacagawea.log, until we got \n
		read_line = (char*) malloc((len_string+1) * sizeof(char));
		if (read(pipe_conf[0], read_line, len_string) < 0) {
			fprintf(stderr, "read() fail becouse: %s\n", strerror(errno));
			exit(5);
		}
		read_line[len_string]='\0';
		write_log(INFO, "received: %d, %s",len_string, read_line);
		fprintf(log_file, "%s", read_line);
		
		fclose(log_file);
		free(read_line);
		pthread_mutex_unlock(mutex);
	}
}
#endif

void write_log(int log_lv, const char* error_string, ...) {
	va_list args;
	va_start(args, error_string);
	#define LOG_STR_LEN 1024

	char* log_string = malloc(LOG_STR_LEN);
	char* ds = date_string();
	char* formatted_error_string = malloc(LOG_STR_LEN - strlen(ds));

	vsnprintf(formatted_error_string, LOG_STR_LEN - strlen(ds),
			error_string, args);

	// make sure that the string ends with no trailing newlines ('\n')
	while(formatted_error_string[strlen(formatted_error_string)-1] == '\n') {
		formatted_error_string[strlen(formatted_error_string)-1] = '\0';
	}

	if (snprintf(log_string, LOG_STR_LEN, "%s %s: %s\n", ds, log_lv_name[log_lv],
			formatted_error_string) < 0) {
		write_log(ERROR, "snprintf() failed: %s\n", strerror(errno));
	}
	free(ds);

	if (log_lv <= WARNING) {
		fprintf(stderr, "%s", log_string);
	} else if (log_lv <= LOG_LEVEL) {
		fprintf(stdout, "%s", log_string);
		// fflush(stdout);
		// return;
	}

	free(log_string);

	va_end(args);
}

// returned string needs to be freed
char* date_string() {
	size_t len_str = 22; // for "[MMM DD YYYY hh:mm:ss]"
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