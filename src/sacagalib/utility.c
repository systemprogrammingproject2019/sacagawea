#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#ifdef _WIN32
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#endif

#include "sacagalib.h"

#ifndef _WIN32
int load_file_memory_and_send_posix(client_args *client_info)
{

	// open get file descriptor associated to file
	int fd = open(client_info->path_file, O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "open() failed: %s\n", strerror(errno));
		exit(5);
	}
	// declare struct for 3th argument for fcntl and memset it to 0
	struct flock lck;
	if (memset(&lck, 0, sizeof(lck)) == NULL)
	{
		fprintf(stderr, "memset() failed: %s\n", strerror(errno));
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
	fcntl(fd, F_SETLKW, &lck);
	// now we have the lock "load file in memory"
	/* initialize the memory for load the file, 
	fseek put the FP at END ftell say the position ( file size ), we come back at start with SEEK_SET*/
	FILE *fp = fdopen(fd, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "fdopen() failed: %s\n", strerror(errno));
		exit(5);
	}
	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	client_info->file_to_send = malloc((len + 1));
	client_info->len_file = len;
	fread(client_info->file_to_send, 1, len, fp);
	client_info->file_to_send[len] = '\0';

	// release lock with F_UNLCK flag and FP FD
	lck.l_type = F_UNLCK;
	fcntl(fd, F_SETLK, &lck);

	fclose(fp);
	close(fd);
	// create thread to send the file at client
	pthread_t tid;
	pthread_create(&tid, NULL, thread_sender, (void *)client_info);
	pthread_join( tid, NULL);
}

// VERSIONE LINUX, NON POSIX dovremmo chiedere al prof se si può usare ma non credo
int load_file_memory_linux(char *path)
{
	// open get file descriptor associated to file
	int fd = open(path, O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "fdopen() failed: %s\n", strerror(errno));
		exit(5);
	}
	// declare struct for 3th argument for fcntl and memset it to 0
	struct flock lck;
	if (memset(&lck, 0, sizeof(lck)) == NULL)
	{
		fprintf(stderr, "memset() failed: %s\n", strerror(errno));
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

	/* OFD is a flag of Linux, not posix, more problably he become new standard in 
	POSIX 1. 
	The principal difference between OFD and non is that where as
	traditional record locks are associated with a process, open file
	description locks(OFD) are associated with the open file description on
	which they are acquired, and are only automatically released on the last
	close of the open file description, instead of being released on any
	close of the file. 
	SETLKW mean is a blocked lock request*/

	//leva commento fcntl (fd, F_OFD_SETLKW, &lck);

	// now we have the lock "load file in memory"

	/* initialize the memory for load the file, 
	fseek put the FP at END ftell say the position ( file size ), we come back at start with SEEK_SET*/
	FILE *fp = fdopen(fd, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "fdopen() failed: %s\n", strerror(errno));
		exit(5);
	}
	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *file_content = malloc((len + 1));
	fread(file_content, 1, len, fp);
	file_content[len] = '\0';

	// we check the tipe or file with file bash command
	/* char command[ (strlen(path))+5 ]; 
	command = strcat( "file", path );
	FILE* popen_output_stream = popen( command , "r" )
	if ( popen_output_stream == NULL ){ 
		fprintf( stderr,"popen() failed: %s\n", strerror(errno));
	 	exit(5);
	}
	char* popen_output = malloc( Pat );
	while ( fgets(popen_output, 100, popen_output_stream) != NULL)
		printf("%s", path); */

	// release lock with F_UNLCK flag and FP FD
	fclose(fp);
	close(fd);
	lck.l_type = F_UNLCK;
	//leva commento  fcntl (fd, F_OFD_SETLKW, &lck);

	fprintf(stdout, "file: %s", file_content);
	free(file_content);
}

int check_security_path(char path[PATH_MAX])
{
	/* le cose qui son 2
	1 o facciamo un array di word globale con le parole non accettate
	2 lasciamo stare tanto giusto ".." è da scartare, e teoricamente anche i ".." in UTF a 16 bits 32, non ricordo 
	quanti erano */
	int i = 0;
	int cont = 0;
	for (i = 0; i < PATH_MAX; ++i)
	{
		if (path[i] == '.')
		{
			cont++;
		}
		else
		{
			cont = 0;
		}

		if (cont == 2)
		{
			return 1;
		}
	}
	return 0;
}
#endif
