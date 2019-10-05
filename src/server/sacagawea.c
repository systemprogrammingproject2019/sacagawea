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

settings_t* settings;

#ifdef _WIN32
// info for log process
PROCESS_INFORMATION logProcess;
#else
/* A condition variable/mutex attribute object (attr) allows you to manage the characteristics
	of condition variables/mutex in your application by defining a set of values to be used for a
	condition variable/mutex during its creation.*/
// these need to be global so they can be closed from an external function
// - like close_all() - without unnecessary complications
int logProcess;
pthread_mutexattr_t mattr;
pthread_condattr_t cattr;
#endif

#ifndef _WIN32
void become_deamon() {
	int pid;
	pid = fork();
	if (pid < 0) {
		write_log(ERROR, "System call fork() failed because of %s", strerror(errno));
		exit(5);
	}
	if (pid > 0) {
		write_log(INFO, "Server become a deamon" );
		exit(5);
	}

	if( setsid() < 0) {
		write_log(ERROR, "System call setsid() failed because of %s", strerror(errno));
		exit(5);
	}

	/* Change the current working directory */
	if ((chdir( S_ROOT_PATH )) < 0) {
		write_log(ERROR, "System call chdir() failed because of %s", strerror(errno));
		exit(5);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/*resettign File Creation Mask .
	this set the process mode to 750 becouse umask need the complement*/
	umask(027);
}
#endif


void close_all() {
#ifdef _WIN32
	closesocket(settings->socket);
	TerminateThread(logProcess.hThread, 0);
	TerminateProcess(logProcess.hProcess, 0);
	WSACleanup();
#else
	// close socket
	close(settings->socket);

	// destroy the allocated attr for, condition variable
	pthread_condattr_destroy(&cattr);
	pthread_mutexattr_destroy(&mattr);

	// kill log process
	kill(logProcess, 15);

	// close write pipe.
	close(pipe_conf[1]);

	//unlink shared memory
	shm_unlink(SHARED_MUTEX_MEM);
	shm_unlink(SHARED_COND_MEM);
#endif
	free(settings);
	exit(1);
}

void universal_handler() {
	if (read_and_check_conf(settings, true)) {
		write_log(INFO, "settings->socket CHANGE %d", settings->socket);

		// close the old server socket --- it is still open on all children
		// threads/processes (until they die/close it) because
		// they were only given a copy of it.
		close(settings->socket);

		settings->socket = open_socket(settings);
	}
}

#ifdef _WIN32
BOOL WINAPI consoleEventHandler(DWORD fdwCtrlType) {
	// "return false" kills the process
	switch (fdwCtrlType)
	{
	case CTRL_BREAK_EVENT:
		write_log(DEBUG, "CTRL+BREAK PRESSED!");

		universal_handler();
		return TRUE; // dont close the process
	default:
		write_log(DEBUG, "Exiting!");
		close_all();
	}
	return FALSE;
}
#endif

#ifndef _WIN32
// check this fuction
// this function is called when a SIGHUP is received 
void sighup_handler(int signum) {
	/* Check sagacawea.conf, if the return's value is true the socket SERVER_PORT 
	change so we have to close the socket finish the instaured connection
	and restart the socket with the new SERVER_PORT */
	// fprintf( stdout, "config file %d\n", read_and_check_conf(&settings));
	universal_handler();
}
#endif

int main(int argc, char *argv[]) {

	//become_deamon();

	//fill default settings
	settings = calloc(1, sizeof(settings_t));
	settings->port = DEFAULT_SERVER_PORT;
	settings->mode = 't';
#ifdef _WIN32
	if (GetCurrentDirectoryA(sizeof(settings->homedir) - 1, settings->homedir) == 0) {
		write_log(ERROR, "GetCurrentDirectoryA failed: %d", strerror(errno));
		close_all();
	}
	settings->homedir[strlen(settings->homedir)] = '\\';
	settings->homedir[strlen(settings->homedir) + 1] = '\0';
#else
	if (getcwd(settings->homedir, sizeof(settings->homedir) - 1) == NULL) {
		write_log(ERROR, "getcwd failed: %d", strerror(errno));
		close_all();
	}
	settings->homedir[strlen(settings->homedir)] = '/';
	settings->homedir[strlen(settings->homedir) + 1] = '\0';
#endif

#ifdef _WIN32
	if (GetComputerNameA(settings->hostname,
			&(long unsigned int){sizeof(settings->hostname) - 1}) == 0) {
		write_log(WARNING, "GetComputerNameA failed with error: %I64d",
				GetLastError());
		strcpy(settings->hostname, "localhost");
	}
#else
	if (gethostname(settings->hostname, sizeof(settings->hostname) - 1) != 0) {
		write_log(WARNING, "gethostname failed: %d", strerror(errno));
		strcpy(settings->hostname, "localhost");
	}
#endif
	// check the sacagawea.conf
	read_and_check_conf(settings, false);

	// check if some variable are setted by command line
	int c;
	opterr = 0;
	while ((c = getopt(argc, argv, "ptP:")) != -1) {
		switch (c) {
			case 'p':
				settings->mode = 'p';
				write_log(INFO, "mode change: 'p'");
				break;

			case 'P':
				settings->port = atoi(optarg);
				write_log(INFO, "port change: %d", settings->port);
				break;

			case 't':
				settings->mode = 't';
				write_log(INFO, "mode change: 't'");
				break;

			case '?':
				write_log(INFO, "Usage: sacagawea [-P number_of_port][-p/-t for use subprocess/threads to process 1 client connection]" );
				exit(20);
				break;
		}
	}

	write_log(INFO, "Server port: %d, mode: %s",
			settings->port, (settings->mode == 'p')?"multiprocess":"multithreaded");

#ifndef _WIN32
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
		write_log(ERROR, "System call shm_open() failed because of %s", strerror(errno));
		exit(5);
	}
	if (ftruncate(mutex_d, sizeof(pthread_mutex_t)) != 0) {
		write_log(ERROR, "System call ftruncate() failed because of %s", strerror(errno));
		exit(5);
	}
	mutex = (pthread_mutex_t *) mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED, mutex_d, 0);
	if (mutex == MAP_FAILED) {
		write_log(ERROR, "System call mmap() failed because of %s", strerror(errno));
		exit(5);
	} 
	/* shm_open open the SHARED_COND_MEM or create it like a file, indeed cond_d is a descriptor */
	cond_d = shm_open(SHARED_COND_MEM , O_CREAT | O_RDWR | O_TRUNC, mode);
	if (cond_d < 0) {
		write_log(ERROR, "System call shm_open() failed because of %s", strerror(errno));
		exit(5);
	}
	if (ftruncate(cond_d, sizeof(pthread_mutex_t)) != 0) {
		write_log(ERROR, "System call ftruncate() failed because of %s", strerror(errno));
		exit(5);
	}
	cond = (pthread_cond_t *) mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED, cond_d, 0);
	if (cond == MAP_FAILED) {
		write_log(ERROR, "System call mmap() failed because of %s", strerror(errno));
		exit(5);
	}

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
#ifndef _WIN32
	if (pipe(pipe_conf) != 0) { 
		write_log(ERROR, "System call pipe() failed because of %s", strerror(errno));
		exit(5);
	}
	/* set NON-BLOCKING read pipe, becouse sacalogs don't have to go in blocked mode
	while try read pipe */
	if (fcntl(pipe_conf[0], F_SETFL, O_NONBLOCK) != 0) {
		write_log(ERROR, "System call fcntl() failed because of %s", strerror(errno));
		exit(5);
	}
#endif
	// now create the process
#ifdef _WIN32
	STARTUPINFO siStartInfo;
	int bLogger = false;

	// Set up members of the PROCESS_INFORMATION structure.
	ZeroMemory(&logProcess, sizeof(PROCESS_INFORMATION));
 
	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO); 

	// Create the child process.
	bLogger = CreateProcess(
			NULL, 
			"sacagawea-logger.exe",  // command line
			NULL,          // default security attributes
			NULL,          // primary thread security attributes
			TRUE,          // IMPORTANT: handles are inherited
			0,             // creation flags
			NULL,          // use parent's environment
			NULL,          // use parent's current directory
			&siStartInfo,  // STARTUPINFO pointer
			&logProcess    // receives PROCESS_INFORMATION
	);
	// If an error occurs, exit the application. 
	if (!bLogger) {
		write_log(ERROR, "CreateProcess failed with error: %I64d",
				GetLastError());
		close_all();
	}

#else
	logProcess = fork();
	if (logProcess < 0){
		write_log(ERROR, "System call fork() failed because of %s", strerror(errno));
		exit(5);
	}
	if (logProcess == 0) { /* child process */
		// close write pipe
		close(pipe_conf[1]);
		// call log management
		log_management();
	}
	/* server process "father" */
	// close read pipe
	close(pipe_conf[0]);
#endif

	// Creating sigaction for SIGHUP
#ifndef _WIN32
	struct sigaction new_action;
	/* Block other SIGHUP signals while handler runs. */
	sigset_t block_mask;
	sigemptyset (&block_mask);
	sigaddset (&block_mask, SIGHUP);
	/* Set up the structure to specify the new action. */
	new_action.sa_handler = (__sighandler_t) sighup_handler;
	new_action.sa_mask = block_mask;
	sigemptyset (&new_action.sa_mask);
	/* set SA_RESTART flag, so If a signal handler is invoked while a system call 
	is running like read/recv etc.. after the handler,
	the system call is restarted and can give EINTR error if fail */ 
	new_action.sa_flags = SA_RESTART;
	/* The sigaction() API change the action taken by a process on receipt of SIGHUP signal. */
	if (sigaction (SIGHUP, &new_action, NULL) < 0) {
		write_log(ERROR, "System call sigaction() failed because of %s", strerror(errno));
		exit(5);
	}
#endif

	// Creating sigaction for SIGINT
#ifndef _WIN32
	struct sigaction sigint_action;
	/* Block other SIGINT signals while handler runs. */
	sigset_t sigint_block_mask;
	sigemptyset (&sigint_block_mask);
	sigaddset (&sigint_block_mask, SIGINT);
	/* Set up the structure to specify the new action. */
	sigint_action.sa_handler = close_all;
	sigint_action.sa_mask = sigint_block_mask;
	sigemptyset (&sigint_action.sa_mask);
	/* set SA_RESTART flag, so If a signal handler is invoked while a system call 
	is running like read/recv etc.. after the handler,
	the system call is restarted and can give EINTR error if fail */ 
	sigint_action.sa_flags = SA_RESTART;
	/* The sigaction() API change the action taken by a process on receipt of SIGINT signal. */
	if (sigaction (SIGINT, &sigint_action, NULL) < 0) {
		write_log(ERROR, "System call sigaction() failed because of %s", strerror(errno));
		exit(5);
	}
#endif

#ifdef _WIN32
	if (!SetConsoleCtrlHandler(consoleEventHandler, true)) {
		write_log(ERROR, "SetConsoleCtrlHandler Failed with error: %lld", GetLastError());
		close_all();
	}
#endif

	// open socket call
	settings->socket = open_socket(settings);

	/* Loop waiting for incoming connects or for incoming data
		on any of the connected sockets.*/
	while (listen_descriptor(settings));

	close_all();
}
