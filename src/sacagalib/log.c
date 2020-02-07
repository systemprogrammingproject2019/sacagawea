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
#include <sys/prctl.h>
#include <signal.h>
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
	FILE* log;

	// receive SIGTERM when parent process dies.
	prctl(PR_SET_PDEATHSIG, SIGTERM);

	while (true) {

		if( pthread_mutex_lock(mutex) != 0 ){
			write_log(ERROR, "LOGS Process fail on lock mutex");
			pthread_mutex_destroy(mutex);
			pthread_cond_destroy(cond);
			shm_unlink(SHARED_MUTEX_MEM);
			shm_unlink(SHARED_COND_MEM);
			close( pipe_conf[0] );
			exit(1);
		}
		/* this while check if pipe is readable, or dont contain nothing.
		if is empty return error EWOULDBLOCK and go again in blocked mode. */
		
		while (true) {
			//fprintf(stdout, "read\n", check);
			check = read(pipe_conf[0] , &len_string, sizeof(int));
			write_log(INFO, "check bytes in logs pipe: %d", check);
			if (check < 0) {
				if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
					write_log(INFO, "LOGS Process nothing");
					if( pthread_cond_wait(cond, mutex) != 0){
						write_log(ERROR, "LOGS Process fail on cond_wait");
						pthread_mutex_destroy(mutex);
						pthread_cond_destroy(cond);
						shm_unlink(SHARED_MUTEX_MEM);
						shm_unlink(SHARED_COND_MEM);
						close( pipe_conf[0] );
						exit(1);
					}
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
				if( pthread_cond_wait(cond, mutex) != 0){
					write_log(ERROR, "LOGS Process fail on cond_wait");
					pthread_mutex_destroy(mutex);
					pthread_cond_destroy(cond);
					shm_unlink(SHARED_MUTEX_MEM);
					shm_unlink(SHARED_COND_MEM);
					close( pipe_conf[0] );
					exit(1);
				}
			}
		}

		log = fopen(SACAGAWEALOGS_PATH , "a");
		if (log == NULL) {
			write_log(INFO, "ERROR open logs file: %s", strerror(errno));
			exit(5);
		}
	
		// read pipe and write sacagawea.log, until we got \n
		read_line = (char*) malloc((len_string+1) * sizeof(char));
		if( read_line == NULL ){
			write_log(ERROR, "Malloc on logProcess failed becouse: %s", strerror(errno));
			exit(5);
		}
		if (read(pipe_conf[0], read_line, len_string) < 0) {
			fprintf(stderr, "read() fail becouse: %s\n", strerror(errno));
			free(read_line);
			fclose(log);
			exit(5);
		}
		read_line[len_string]='\0';
		write_log(INFO, "received: %d, %s",len_string, read_line);
		fprintf(log, "%s", read_line);
		
		fclose(log);
		free(read_line);
		if( pthread_mutex_unlock(mutex) != 0 ){
			write_log(ERROR, "LOGS Process fail on cond_wait");
			pthread_mutex_destroy(mutex);
			pthread_cond_destroy(cond);
			shm_unlink(SHARED_MUTEX_MEM);
			shm_unlink(SHARED_COND_MEM);
			close( pipe_conf[0] );
			exit(1);
		}
	}
}
#endif

void write_log(int log_lv, const char* error_string, ...) {
	va_list args;
	va_start(args, error_string);
	#define LOG_STR_LEN 1024

	char* log_string = malloc(LOG_STR_LEN);
	if( log_string == NULL ){
		write_log(ERROR, "Malloc on writelog failed becouse: %s", strerror(errno));
		exit(5);
	}
	char* ds = date_string();
	char* formatted_error_string = malloc(LOG_STR_LEN - strlen(ds));
	if( formatted_error_string == NULL ){
		write_log(ERROR, "Malloc on writelog failed becouse: %s", strerror(errno));
		exit(5);
	}

	vsnprintf(formatted_error_string, LOG_STR_LEN - strlen(ds),
			error_string, args);

	// make sure that the string ends with no trailing newlines ('\n')
	while(formatted_error_string[strlen(formatted_error_string)-1] == '\n') {
		formatted_error_string[strlen(formatted_error_string)-1] = '\0';
	}

	if (snprintf(log_string, LOG_STR_LEN, "%s %s: %s\n", ds, log_lv_name[log_lv],
			formatted_error_string) < 0) {
		fprintf(stderr, "ERROR: snprintf() failed: %s\n", strerror(errno));
	}
	free(ds);

	if (log_lv <= WARNING) {
		fprintf(stderr, "%s", log_string);
		fflush(stderr);
		
	} else if (log_lv <= LOG_LEVEL) {
		fprintf(stdout, "%s", log_string);
		fflush(stdout);
		// return;
	}
	free(formatted_error_string);
	free(log_string);
	va_end(args);
}

// returned string needs to be freed
char* date_string() {
	size_t len_str = 22; // for "[MMM DD YYYY hh:mm:ss]"
	char* r = malloc(len_str+1);
	if( r == NULL ){
		write_log(ERROR, "Malloc on date_string failed becouse: %s", strerror(errno));
		exit(5);
	}
	time_t rawtime;
	struct tm* timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	char* timestr = asctime(timeinfo);
	char month[4] = {timestr[4], timestr[5], timestr[6], 0};
	if (snprintf(r, len_str+1, "[%s %02d %04d %02d:%02d:%02d]",
			month, timeinfo->tm_mday, timeinfo->tm_year + 1900, 
			timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec) < 0) {
		free(timeinfo);
		write_log(ERROR, "snprintf() failed: %s\n", strerror(errno));
		exit(5);
	}
	return r;
}