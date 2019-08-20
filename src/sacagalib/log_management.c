#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifndef _WIN32
#include <pthread.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#endif

#include "sacagalib.h"


void log_management(){

	printf("Process for sacagawea.log created\n");
	FILE *fp;
	char *read_line;
	int len_string, check;

	while(true){
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

		// open logs file and check if an error occured
		fp = fopen( SACAGAWEALOGS_PATH , "a");
		if(fp==NULL){
			fprintf(stderr, S_ERROR_FOPEN, strerror(errno));
			exit(5);
		}
		// read pipe and write sacagawea.log, until we got \n
		read_line = (char*) malloc( (len_string+1)*sizeof(char) );
		read(pipe_conf[0] , read_line , len_string );
		read_line[len_string]='\0';
		fprintf(stdout, "received: %d, %s",len_string, read_line);
		fprintf(fp, "%s", read_line);
		
		fclose(fp);
		free(read_line);
		pthread_mutex_unlock(mutex);
	}
	
}