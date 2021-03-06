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
void become_daemon() {
	int pid;
	pid = fork();
	if (pid < 0) {
		write_log(ERROR, "System call fork() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	if(setsid() < 0) {
		write_log(ERROR, "System call setsid() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Fork again, allowing the parent process to terminate.
	if( signal(SIGHUP,SIG_IGN) == SIG_ERR ){
		write_log(ERROR, "Signal() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	pid = fork();
	if (pid < 0) {
		write_log(ERROR, "System call setsid() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		write_log(INFO, "Sacagawea daemon at pid: %d", pid);
		exit(EXIT_SUCCESS);
	}

	/* Change the current working directory */
	if ((chdir(S_ROOT_PATH)) < 0) {
		write_log(ERROR, "System call chdir() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/*resetting file Creation Mask .
	this sets the process mode to 750 because umask needs the complement*/
	umask(027);

	/* Close out the standard file descriptors */
	if( close(STDIN_FILENO) == -1 ){
		write_log(ERROR, "System call close() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if( close(STDOUT_FILENO) == -1 ){
		write_log(ERROR, "System call close() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if( close(STDERR_FILENO) == -1 ){
		write_log(ERROR, "System call close() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}


	if (open("/dev/null",O_RDONLY) == -1) {
		write_log(ERROR, "failed to reopen stdin while daemonising (errno=%d)",errno);
		exit(EXIT_FAILURE);
	}
	if (open("/dev/null",O_WRONLY) == -1) {
		write_log(ERROR, "failed to reopen stdout while daemonising (errno=%d)",errno);
		exit(EXIT_FAILURE);
	}
	if (open("/dev/null",O_RDWR) == -1) {
		write_log(ERROR, "failed to reopen stderr while daemonising (errno=%d)",errno);
		exit(EXIT_FAILURE);
	}
}
#endif


void close_all() {
	write_log(INFO, "Gracefully closing the server...");
#ifdef _WIN32
	if( closesocket(settings->socket) == SOCKET_ERROR ){
		write_log(ERROR, "closesocket failed with error: %d", WSAGetLastError());
		// exit(1); we check the error but dont close the program.
	}
	if( TerminateThread(logProcess.hThread, 0) == 0 ){
		write_log(ERROR, "TerminateThread failed with error: %d", GetLastError());
	}
	if( TerminateProcess(logProcess.hProcess, 0) == 0 ){
		write_log(ERROR, "TerminateThread failed with error: %d", GetLastError());
	}
	if( WSACleanup() == SOCKET_ERROR ){
		write_log(ERROR, "WSACleanup failed with error: %d", WSAGetLastError());
	}
#else
	// close socket
	if( close(settings->socket) == -1 ){
		write_log(ERROR, "System call close() failed because of %s", strerror(errno));
	}

	// kill log process
	if( kill(logProcess, 15) == -1 ){
		write_log(ERROR, "System call kill() failed because of %s", strerror(errno));
	}

	// close write pipe and send SIGTERM to logs process.
	if( close(condVar->pipe_conf[1]) == -1 ){
		write_log(ERROR, "System call close() failed because of %s", strerror(errno));
	}
	if( kill( logs_proces_pid, SIGTERM ) == -1 ){
		write_log(ERROR, "System call kill() failed because of %s", strerror(errno));
	}
	//unlink shared memory
	if( shm_unlink(SHARED_COND_VARIABLE_MEM) == -1 ){
		write_log(ERROR, "System call shm_unlink() failed because of %s", strerror(errno));
	}
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
	#ifdef _WIN32
		if (closesocket(settings->socket) == SOCKET_ERROR) {
			write_log(ERROR, "System call close() on %d (server socket) failed with error: %d. Socket change failed.", settings->socket, WSAGetLastError());
	#else
		if (close(settings->socket) == -1) {
			write_log(ERROR, "System call close() on %d (server socket) failed with error: %s. Socket change failed.", settings->socket, strerror(errno));
	#endif
		}
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

#ifndef _WIN32
	// Child becomes Zombie as parent is sleeping when child process exits. 
	// if we ignore SIGCHLD the father dont need to read "the Zombie" and it will be removed from process table.
	if( signal(SIGCHLD,SIG_IGN) == SIG_ERR ){
		write_log(ERROR, "Signal() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif

#ifdef _WIN32
	// create a job object and include both this process and the logger process
	// so that when we close this process, all processes in the job object are
	// closed automatically thanks to JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE flag
	HANDLE jobHandle;

	if ((jobHandle = CreateJobObjectA(NULL, NULL)) == NULL) {
		write_log(ERROR, "CreateJobObjectA failed: %lld", GetLastError());
		exit(EXIT_FAILURE);
	}

	// set JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE on the job object
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
	jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (SetInformationJobObject(jobHandle, 
								JobObjectExtendedLimitInformation,
								&jeli,
								sizeof(jeli)) == 0) 
	{
		write_log(ERROR, "SetInformationJobObject failed: error %lld", GetLastError() );
		exit(EXIT_FAILURE);
	}

	if (AssignProcessToJobObject(jobHandle, GetCurrentProcess()) == 0) {
		write_log(ERROR, "AssignProcessToJobObject failed: %lld",GetLastError());
		exit(EXIT_FAILURE);
	}

#endif

	//fill default settings
	settings = calloc(1, sizeof(settings_t));
	if( settings == NULL ){
		write_log(ERROR, "calloc of settings failed");
		close_all();
	}
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
						&(long unsigned int){sizeof(settings->hostname) - 1}) == 0) 
	{
		write_log(WARNING, "GetComputerNameA failed with error: %I64d",GetLastError());
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
#ifndef _WIN32
	int debug_mode = false;
#endif
	while ((c = getopt(argc, argv, "ptdP:")) != -1) {
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
			
		#ifndef _WIN32
			case 'd':
				write_log(INFO, "debug mode (AKA \"dont become a daemon\" mode) is on");
				debug_mode = true;
				break;
		#endif

			case '?':
			#ifdef _WIN32
				write_log(INFO, "Usage: sacagawea [-P number_of_port][-p/-t for use subprocess/threads to handle client connections]" );
			#else
				write_log(INFO, "Usage: sacagawea [-P number_of_port][-p/-t for use subprocess/threads to handle client connections][-d enable debug mode (don't become a daemon)]" );
			#endif
				exit(20);
				break;
		}
	}
#ifndef _WIN32
	if (debug_mode == false) {
		become_daemon();
	}
#endif
	write_log(INFO, "Server port: %d, mode: %s",
			settings->port, (settings->mode == 'p')?"multiprocess":"multithreaded");

#ifndef _WIN32
	/* condition variable and mutex are shared on the same process, but when we fork we create a new
	addressing for the child, so the global/local variable are not shared like threads.
	we have to create a shared memory for the 2 process and put on that the mutex and condition variable
	now these can be used by both process. */
	int condVar_descriptor;
	int mode = S_IRWXU | S_IRWXG;
	
	// we create a SHARED memory section for the conditionVariable struct
	condVar_descriptor = shm_open( SHARED_COND_VARIABLE_MEM , O_CREAT | O_RDWR | O_TRUNC, mode);
	if (condVar_descriptor < 0) {
		write_log(ERROR, "System call shm_open() failed because of %s", strerror(errno));
		exit(5);
	}
	if (ftruncate(condVar_descriptor, sizeof(conditionVariable)) != 0) {
		write_log(ERROR, "System call ftruncate() failed because of %s", strerror(errno));
		exit(5);
	}
	condVar = (conditionVariable *) mmap(NULL, sizeof(conditionVariable), PROT_READ | PROT_WRITE, MAP_SHARED, condVar_descriptor, 0);
	if (condVar == MAP_FAILED) {
		write_log(ERROR, "System call mmap() failed because of %s", strerror(errno));
		exit(5);
	}
	// initialize cont = 0;
	condVar->cont=0;

	/* initialize mutex and cond variable
	PTHREAD_PROCESS_SHARED
	Permits a condition variable/mutex to be operated upon by any thread that has access to the memory
	where the condition variable/mutex is allocated; even if the condition variable/mutex is allocated in memory
	that is shared by multiple processes.*/
	if( pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED) != 0){
		write_log(ERROR, "pthread_condattr_setpshared() failed ");
		exit(5);
	}
	if( pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED) != 0){
		write_log(ERROR, "pthread_muteattr_setpshared() failed ");
		exit(5);
	}
	// set and init the mutex and condition variable
	if( pthread_mutex_init( &(condVar->mutex) , &mattr) != 0){
		write_log(ERROR, "pthread_mutex_init() failed ");
		exit(5);
	}
	if( pthread_cond_init( &(condVar->cond) , &cattr) != 0){
		write_log(ERROR, "pthread_cond_init() failed ");
		exit(5);
	}
	// destroy the allocated attr for, condition variable
	int ret;
	ret = pthread_condattr_destroy(&cattr);
	if( ( ret == EBUSY ) || ( ret == EINVAL) ){
		write_log(ERROR, "pthread_condattr_destroy(&cattr); failed becouse %d", ret);
	}
	ret = pthread_mutexattr_destroy(&mattr);
	if( ret != 0){
		write_log(ERROR, "pthread_mutexattr_destroy(&mattr); failed becouse %d", ret);
	}
#endif

	// create the pipe for SERVER<->SACALOGS
#ifndef _WIN32
	if (pipe(condVar->pipe_conf) != 0) { 
		write_log(ERROR, "System call pipe() failed because of %s", strerror(errno));
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
		write_log(ERROR, "CreateProcess failed with error: %I64d",GetLastError());
		close_all();
	}

	if (AssignProcessToJobObject(jobHandle, logProcess.hProcess) == 0) {
		write_log(ERROR, "AssignProcessToJobObject failed: %lld",GetLastError());
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
		if( close(condVar->pipe_conf[1]) == -1 ){
		write_log(ERROR, "System call close() failed because of %s", strerror(errno));
		exit(EXIT_FAILURE);
		}
		// call log management
		log_management();
		return 0;
	}
	/* server process "father" */
	// close read pipe
	if( close(condVar->pipe_conf[0]) == -1 ){
		write_log(ERROR, "System call close() failed because of %s", strerror(errno));
		close_all();
	}
	logs_proces_pid = logProcess;
#endif

	// Creating sigaction for SIGHUP
#ifndef _WIN32
	struct sigaction new_action;
	/* Block other SIGHUP signals while handler runs. */
	sigset_t block_mask;
	if (sigemptyset (&block_mask) == -1) {
		write_log(ERROR, "sigemptyset() failed" );
		close_all();
	}
	if (sigaddset (&block_mask, SIGHUP) == -1) {
		write_log(ERROR, "sigaddset() for SIGHUP failed" );
		close_all();
	}
	/* Set up the structure to specify the new action. */
	new_action.sa_handler = (__sighandler_t) sighup_handler;
	new_action.sa_mask = block_mask;
	/* set SA_RESTART flag, so If a signal handler is invoked while a system call 
	is running like read/recv etc.. after the handler,
	the system call is restarted and can give EINTR error if fail */ 
	new_action.sa_flags = SA_RESTART;
	/* The sigaction() API change the action taken by a process on receipt of SIGHUP signal. */
	if (sigaction (SIGHUP, &new_action, NULL) < 0) {
		write_log(ERROR, "System call sigaction() failed because of %s", strerror(errno));
		close_all();
	}
#endif

	// Creating sigaction for all signals that would terminate
	// the process by default
#ifndef _WIN32
	struct sigaction term_signal_action;
	/* Block other signals while handler runs. */
	sigset_t term_signal_block_mask;
	if (sigemptyset (&term_signal_block_mask) == -1) {
		write_log(ERROR, "sigemptyset() failed" );
		close_all();
	}
	if (sigaddset (&term_signal_block_mask, SIGHUP) == -1) {
		write_log(ERROR, "sigaddset() for SIGHUP failed" );
		close_all();
	}
	if( sigaddset (&term_signal_block_mask, SIGINT) == -1) {
		write_log(ERROR, "sigaddset() failed" );
		close_all();
	}
	if (sigaddset (&term_signal_block_mask, SIGTERM) == -1) {
		write_log(ERROR, "sigaddset() for SIGTERM failed" );
		close_all();
	}
	if (sigaddset (&term_signal_block_mask, SIGALRM) == -1) {
		write_log(ERROR, "sigaddset() for SIGALRM failed" );
		close_all();
	}
	if (sigaddset (&term_signal_block_mask, SIGUSR1) == -1) {
		write_log(ERROR, "sigaddset() for SIGUSR1 failed" );
		close_all();
	}
	if (sigaddset (&term_signal_block_mask, SIGUSR2) == -1) {
		write_log(ERROR, "sigaddset() for SIGUSR2 failed" );
		close_all();
	}
	/* Set up the structure to specify the new action. */
	term_signal_action.sa_handler = close_all;
	term_signal_action.sa_mask = term_signal_block_mask;
	/* set SA_RESTART flag, so If a signal handler is invoked while a system call 
	is running like read/recv etc.. after the handler,
	the system call is restarted and can give EINTR error if fail */ 
	term_signal_action.sa_flags = SA_RESTART;
	/* The sigaction() API change the action taken by a process on receipt of these signals */
	if (sigaction (SIGINT, &term_signal_action, NULL) < 0) {
		write_log(ERROR, "System call sigaction() failed because of %s", strerror(errno));
		close_all();
	}
	if (sigaction (SIGTERM, &term_signal_action, NULL) < 0) {
		write_log(ERROR, "System call sigaction() failed because of %s", strerror(errno));
		close_all();
	}
	if (sigaction (SIGALRM, &term_signal_action, NULL) < 0) {
		write_log(ERROR, "System call sigaction() failed because of %s", strerror(errno));
		close_all();
	}
	if (sigaction (SIGUSR1, &term_signal_action, NULL) < 0) {
		write_log(ERROR, "System call sigaction() failed because of %s", strerror(errno));
		close_all();
	}
	if (sigaction (SIGUSR2, &term_signal_action, NULL) < 0) {
		write_log(ERROR, "System call sigaction() failed because of %s", strerror(errno));
		close_all();
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
