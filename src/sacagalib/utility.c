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
#else
	// open get file descriptor associated to file
	int fd = open(client_info->path_file, O_RDONLY, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		write_log(ERROR, "open() failed: %s\n", strerror(errno));
		exit(5);
	}
	// declare struct for 3th argument for fcntl and memset it to 0
	struct flock lck;
	if (memset(&lck, 0, sizeof(lck)) == NULL) {
		write_log(ERROR, "memset() failed: %s\n", strerror(errno));
		exit(5);
	}

	// F_WRLCK mean exclusive lock and not shared lock
	/* difference, first put lock for read and write, in the second one
	process if another is reading from the file can read simultanealy 
	but cant write, and if 1 is writing no other one can write or read */
	lck.l_type = F_WRLCK;
	// lock entire file
	lck.l_whence = SEEK_SET; // offset base is start of the file "SEEK_END mean start at end of file"
	lck.l_start = 0;		 // starting offset is zero
	lck.l_len = 0;			 // len is zero, which is a special value representing end
							 // of file (no matter how large the file grows in future)

	/* this version use SETLKW with associed lock at couple [i-node,process], so threads share the lock
	but forked process nope. */
	//fcntl(fd, F_SETLKW, &lck);
	fcntl(fd, F_OFD_SETLKW, &lck);
	// now we have the lock "load file in memory"
	
	// mapping file in memory using MMAP this means, more faster and more simpliest.
	struct stat stat_fd;
	if ( fstat(fd,&stat_fd) == -1 ) {
		write_log(ERROR, "fstat() failed on %s request, becouse: %s\n",client_info->client_addr, strerror(errno));
		// release lock with F_UNLCK flag and close FD
		lck.l_type = F_UNLCK;
		fcntl(fd, F_OFD_SETLK, &lck);
		close(fd);
		return 0;
	}
	client_info->len_file = stat_fd.st_size;
	client_info->file_to_send = mmap( NULL, (client_info->len_file+1), PROT_READ, MAP_PRIVATE, fd, 0);

	// old version with malloc
	//FILE *fp = fdopen(fd, "r");
	//if (fp == NULL) {
	//	write_log(ERROR, "fdopen() failed: %s\n", strerror(errno));
	//	exit(5);
	//}
	//fseek(fp, 0, SEEK_END);
	//client_info->len_file = ftell(fp);
	//fseek(fp, 0, SEEK_SET);

	// client_info->file_to_send = malloc((client_info->len_file + 1));
	// if ( fread(client_info->file_to_send, 1, client_info->len_file, fp) < client_info->len_file ) {
	//	write_log(ERROR, "fread() failed: %s\n", strerror(errno));
	//	exit(5);
	//}
	//client_info->file_to_send[client_info->len_file] = '\0';


	// release lock with F_UNLCK flag and FP FD
	lck.l_type = F_UNLCK;
	//fcntl(fd, F_SETLK, &lck);
	fcntl(fd, F_OFD_SETLK, &lck);

	//fclose(fp);
	close(fd);
	// create thread to send the file at client
	pthread_t tid;
	pthread_create(&tid, NULL, (void *) thread_sender, (void *)client_info);
	pthread_join( tid, NULL);
	return 0;
#endif
}


int check_security_path(char path[PATH_MAX]) {
	/* le cose qui son 2
	1 o facciamo un array di word globale con le parole non accettate
	2 lasciamo stare tanto giusto "/../" è da scartare, e teoricamente anche i ".." in UTF a 16 bits 32, non ricordo 
	quanti erano */
	int i = 0;

	for (i = 0; i < strlen( path ); ++i) {
		if ( strncmp( &path[i], "/../" , 4) == 0 ){
			return true;
		} 
	}
	return false;
}
