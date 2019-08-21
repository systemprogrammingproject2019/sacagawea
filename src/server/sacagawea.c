#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h> // for close

#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#endif

#include "sacagalib.h"

int main(int argc, char *argv[]){

	// chiamo load file in memory per testare se funzionava
	//load_file_memory_posix( "conf/sacagawea.conf");

	// check the sacagawea.conf
	read_and_check_conf();
	// check if some variable are setted by command line
	int c;
	opterr = 0;
	while ((c = getopt(argc, argv, "ptP:")) != -1) {
		switch (c) {
			case 'p':
				MODE_CLIENT_PROCESSING = 1;
				fprintf(stdout,"mode change p: %d\n", MODE_CLIENT_PROCESSING);
				break;

			case 'P':
				SERVER_PORT = atoi(optarg);
				fprintf(stdout, "port change: %d\n", SERVER_PORT);
				break;

			case 't':
				MODE_CLIENT_PROCESSING = 0;
				fprintf(stdout, "mode change t: %d\n", MODE_CLIENT_PROCESSING);
				break;

			case '?':
				fprintf(stdout, "Usage: sacagawea [-P number_of_port][-p/-t for use subprocess/threads to process 1 client connection]" );
				exit(20);
				break;
		}
	}

	fprintf(stdout, "Server port: %d, mode: %s\n",
			SERVER_PORT, MODE_CLIENT_PROCESSING?"multiprocess":"multithreaded");

#ifndef _WIN32
	//spawn process who manages the logs file
	int pid; /* process identifier */

	/* condition variable and mutex are shared on the same process, but when we fork we create a new
	addressing for the child, so the global/local variable are not shared like threads.
	we have to create a shared memory for the 2 process and put on that the mutex and condition variable
	now these can be used by both process. */
	int mutex_d, cond_d;
	int mode = S_IRWXU | S_IRWXG;
	/* non ho capito perche se con shm_open di una SHARED_MEM qualsiasi gli davo dimensione con ftruncate
	pari a un mutex e un cond, quando facevo i 2 mmap non funzionavano. non erano sharati ( non da errore
	semplicemente funzionava come se il mutex fosse sharato ma non la cond), con 2 open diversi funziona.
	non eliminare devo chiederlo al prof so curioso. non ho trovato nulla al riguardo su internet o sul man */
	/* shm_open open the SHARED_MUTEX_MEM or create it like a file, indeed mutex_d is a descriptor */
	mutex_d = shm_open( SHARED_MUTEX_MEM , O_CREAT | O_RDWR | O_TRUNC, mode);
	if (mutex_d < 0) {
		fprintf( stderr,"System call shm_open() failed because of %s", strerror(errno));
	 	exit(5);
	}
	if (ftruncate(mutex_d, sizeof(pthread_mutex_t)) == -1) {
		fprintf( stderr,"System call ftruncate() failed because of %s", strerror(errno));
	 	exit(5);
	}
	mutex = (pthread_mutex_t *)mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED, mutex_d, 0);
	if (mutex == MAP_FAILED) {
		fprintf( stderr,"System call mmap() failed because of %s", strerror(errno));
	 	exit(5);
	} 
	/* shm_open open the SHARED_COND_MEM or create it like a file, indeed cond_d is a descriptor */
	cond_d = shm_open( SHARED_COND_MEM , O_CREAT | O_RDWR | O_TRUNC, mode);
	if (cond_d < 0) {
		fprintf( stderr,"System call shm_open() failed because of %s", strerror(errno));
	 	exit(5);
	}
	if (ftruncate(cond_d, sizeof(pthread_mutex_t)) == -1) {
		fprintf( stderr,"System call ftruncate() failed because of %s", strerror(errno));
	 	exit(5);
	}
	cond = (pthread_cond_t *)mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED, cond_d, 0);
	if (cond == MAP_FAILED) {
		fprintf( stderr,"System call mmap() failed because of %s", strerror(errno));
	 	exit(5);
	}
	/* A condition variable/mutex attribute object (attr) allows you to manage the characteristics
	of condition variables/mutex in your application by defining a set of values to be used for a
	condition variable/mutex during its creation.*/
	pthread_mutexattr_t mattr;
	pthread_condattr_t cattr;
	/* PTHREAD_PROCESS_SHARED
	Permits a condition variable/mutex to be operated upon by any thread that has access to the memory
	where the condition variable/mutex is allocated; even if the condition variable/mutex is allocated in memory
	that is shared by multiple processes.*/
	pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
	pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
	// set and init the mutex and condition variable
	pthread_mutex_init(mutex, &mattr);
	pthread_cond_init(cond, &cattr);
#endif

	// create the pipe for SERVER<->SACALOGS
#ifdef _WIN32
#else
	if ( pipe(pipe_conf)==-1 ){ 
		fprintf( stderr,"System call pipe() failed because of %s", strerror(errno));
	 	exit(5);
	}
	/* set NON-BLOCKING read pipe, becouse sacalogs don't have to go in blocked mode
	while try read pipe */
	if (fcntl(pipe_conf[0], F_SETFL, O_NONBLOCK) < 0){
		fprintf( stderr,"System call fcntl() failed because of %s", strerror(errno));
	 	exit(5);
	}
#endif
	// now create the process
#ifdef _WIN32
#else
	pid=fork();
	if (pid < 0){
		fprintf( stderr,"System call fork() failed because of %s", strerror(errno));
	 	exit(5);
	}
	if (pid == 0){ /* child process */
		// close write pipe
		close( pipe_conf[1] );
		// call log management
		log_management();
	}
	/* server process "father" */
	// close read pipe
	close( pipe_conf[0] );
	// Creating sigaction for SIGHUP
	struct sigaction new_action;
	/* Block other SIGHUP signals while handler runs. */
	sigset_t block_mask;
	sigemptyset (&block_mask);
	sigaddset (&block_mask, SIGHUP);
	/* Set up the structure to specify the new action. */
	new_action.sa_handler = config_handler;
	new_action.sa_mask = block_mask;
	sigemptyset (&new_action.sa_mask);
	/* set SA_RESTART flag, so If a signal handler is invoked meanwhile a system call 
	is running like read/recv etc.. after the handler,
	the system call is restarted and can give EINTR error if fail */ 
	new_action.sa_flags = SA_RESTART;
	/* The sigaction() API change the action taken by a process on receipt of SIGHUP signal. */
	if( sigaction (SIGHUP, &new_action, NULL) < 0 ){
		fprintf( stderr,"System call sigaction() failed because of %s", strerror(errno));
	 	exit(5);
	}
#endif

	// open socket call
	SERVER_SOCKET = open_socket();

#ifndef _WIN32
	/* declare FD_SET and initialize it */
	FD_ZERO(&fds_set);
	max_num_s = SERVER_SOCKET;
	FD_SET( SERVER_SOCKET, &fds_set);
	//per controlare roba da me "non eliminare"
	for (int i=0; i <= max_num_s ; ++i){
		fprintf( stdout,"i: %d  is set:  %d\n",i,FD_ISSET(i, &fds_set));
	}
#endif

#ifdef _WIN32
	/* Loop waiting for incoming connects or for incoming data
		on any of the connected sockets.   */
	do {
		if (listen_descriptor(SERVER_SOCKET)) {
			break;
		}
	} while(true);
#else
	do {
		if (listen_descriptor()) {
			break;
		}
	} while(true);
#endif

	// we are out of select loop so we have to close all sockets
#ifdef _WIN32
	for(int i = 0; i < MAX_CLIENTS; i++)  {
		SOCKET s = client_socket[i];
		if(s > 0) {
			closesocket(s);
		}
	}
	WSACleanup();
#else
	for (int i=0; i <= max_num_s; ++i){
		if (FD_ISSET(i, &fds_set)){
			close(i);
		}
	}
	// destroy the allocated attr for, condition variable
	pthread_condattr_destroy(&cattr);
	pthread_mutexattr_destroy(&mattr);
	// close write pipe.
	close( pipe_conf[1] );
#endif
}
