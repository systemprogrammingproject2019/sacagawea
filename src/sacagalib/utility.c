#ifndef _WIN32
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#ifdef _WIN32
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#endif

#include "sacagalib.h"

int load_file_memory_and_send(client_args *client_info) {
#ifdef _WIN32
	HANDLE hFile = CreateFileA(
			client_info->path_file,
			GENERIC_READ,
			0, // Security arrtibutes: 0 means the file is locked
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_READONLY,
			NULL
	);
	if (hFile == INVALID_HANDLE_VALUE) {
		write_log(ERROR, "Failed to CreateFile %s, with error: %d",
				client_info->path_file, GetLastError());
		return false;
	}

	if( GetFileSizeEx(hFile, (PLARGE_INTEGER) &(client_info->len_file)) == 0){
		write_log(ERROR, "Failed to GetFileSizeEx on %s, with error: %d",
				client_info->path_file, GetLastError());
		return false;
	}
	write_log(DEBUG, "Sending %lld bytes file %s", client_info->len_file, client_info->path_file);
	// parent's security attributes, with bInheritHandle set to TRUE
	// so the FIle Mapping gets inherited by the child process
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	HANDLE hMapFile = CreateFileMappingA(
			hFile,
			&sa,
			PAGE_READONLY,
			HIDWORD(client_info->len_file), // these macros convert a normal number into the kind
			LODWORD(client_info->len_file), // of numbers required for this kind of functions
			NULL
	);
	if (hMapFile == NULL) {
		write_log(ERROR, "CreateFileMappingA failed wirh error: %d",
				GetLastError());
		return false;
	}

	// we pass the name of the file mapping to the sender thread
	// instead of the actual file, as the linux version does 
	client_info->file_to_send = hMapFile;
		write_log(DEBUG, "CreateFileMappingA on %d", client_info->file_to_send);
	LPDWORD lpThreadId = 0;
	HANDLE tHandle = CreateThread(
			NULL,           // default security attributes
			0,              // use default stack size  
			(LPTHREAD_START_ROUTINE) thread_sender,  // thread function name
			client_info,    // argument to thread function 
			0,              // use default creation flags 
			lpThreadId      // returns the thread identifier 
	);
	if( tHandle == NULL ){
		write_log(ERROR, "CreateThread sender failed wirh error: %d",
				GetLastError());
		return false;
	}
	// the file handle is not needed by the file map, so we can close it
	// before waiting for the thread to finish. This way, the thread can
	// write onto the log file sacagawea.log if sacagawea.log is the file
	// that's being requested
	if( CloseHandle(hFile) == 0 ){
		write_log(ERROR, "Close hFile failed wirh error: %d",
				GetLastError());
		return false;
	}
	if( WaitForSingleObject(tHandle, INFINITE) == WAIT_FAILED ){
		write_log(ERROR, "WaitThread sender failed wirh error: %d",
				GetLastError());
		return false;
	}
	if( CloseHandle(tHandle) == 0 ){
		write_log(ERROR, "Close tHandle failed wirh error: %d",
				GetLastError());
		return false;
	}
	if( CloseHandle(hMapFile) == 0 ){
		write_log(ERROR, "Close hMapFile failed wirh error: %d",
				GetLastError());
		return false;
	}

	return true;
#else
	// open get file descriptor associated to file
	int fd = open(client_info->path_file, O_RDONLY);
	if (fd < 0) {
		write_log(ERROR, "open() failed: %s\n", strerror(errno));
		return false;
	}
	// declare struct for 3th argument for fcntl and memset it to 0
	struct flock lck;
	if (memset(&lck, 0, sizeof(lck)) == NULL) {
		write_log(ERROR, "memset() failed: %s\n", strerror(errno));
		return false;
	}

	// F_WRLCK mean exclusive lock and not shared lock
	/* difference, first put lock for read and write, in the second one
	process if another is reading from the file can read simultanealy 
	but cant write, and if 1 is writing no other one can write or read */
	lck.l_type = F_WRLCK;
	// lock entire file
	lck.l_whence = SEEK_SET; // offset base is start of the file "SEEK_END mean start at end of file"
	lck.l_start = 0;         // starting offset is zero
	lck.l_len = 0;           // len is zero, which is a special value representing end
	                         // of file (no matter how large the file grows in future)

	/* this version use SETLKW with associed lock at couple [i-node,process], so threads share the lock
	but forked process nope. */
	//fcntl(fd, F_SETLKW, &lck);
	if( fcntl(fd, F_OFD_SETLKW, &lck) < 0 ){
		write_log(ERROR, "fcntl() failed on %s request, becouse: %s\n", client_info->addr, strerror(errno));
		return 0;
	}
	// now we have the lock "load file in memory"
	
	// mapping file in memory using MMAP this means, more faster and more simpliest.
	struct stat stat_fd;
	if (fstat(fd,&stat_fd) == -1) {
		write_log(ERROR, "fstat() failed on %s request, becouse: %s\n", client_info->addr, strerror(errno));
		// release lock with F_UNLCK flag and close FD
		lck.l_type = F_UNLCK;
		if( fcntl(fd, F_OFD_SETLK, &lck) < 0 ){
			write_log(ERROR, "fcntl() failed on %s request, becouse: %s\n", client_info->addr, strerror(errno));
		}
		close(fd);
		return 0;
	}
	client_info->len_file = stat_fd.st_size;
	client_info->file_to_send = mmap(NULL, client_info->len_file, PROT_READ, MAP_PRIVATE, fd, 0);

	if ( client_info->file_to_send == MAP_FAILED){
		write_log(ERROR, "mmap() failed on %s request, becouse: %s\n", client_info->addr, strerror(errno));
		// release lock with F_UNLCK flag and close FD
		lck.l_type = F_UNLCK;
		if( fcntl(fd, F_OFD_SETLK, &lck) < 0 ){
			write_log(ERROR, "fcntl() failed on %s request, becouse: %s\n", client_info->addr, strerror(errno));
		}
		close(fd);
		return 0;
	}

	// release lock with F_UNLCK flag and FP FD
	lck.l_type = F_UNLCK;
	//fcntl(fd, F_SETLK, &lck);
	if( fcntl(fd, F_OFD_SETLK, &lck) == -1 ){
		write_log(ERROR, "fcntl failed becouse: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	//fclose(fp);
	if( close(fd) == -1 ){
		write_log(ERROR, "close failed becouse: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// create thread to send the file to the client
	pthread_t tid;
	if( pthread_create(&tid, NULL, (void *) thread_sender, (void *) client_info) != 0 ){
		write_log(ERROR, "pthread_create failed") ;
		exit(EXIT_FAILURE);
	}
	if( pthread_join(tid, NULL) != 0 ){
		write_log(ERROR, "pthread_create failed") ;
		exit(EXIT_FAILURE);
	}
	// munmap is the free for mmap
	if( munmap(client_info->file_to_send, (client_info->len_file + 1)) == -1 ){
		write_log(ERROR, "close failed becouse: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	return true;
#endif
}


int check_security_path(char path[PATH_MAX]) {
	/* le cose qui son 2
	1 o facciamo un array di word globale con le parole non accettate
	2 lasciamo stare tanto giusto "/../" è da scartare, e teoricamente anche i ".." in UTF a 16 bits 32, non ricordo 
	quanti erano */
	if (strstr(path, "../") != NULL || strstr(path, "..\\") != NULL) {
		return true;
	}
	return false;
}
