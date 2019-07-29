#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <getopt.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "sacagaweaLinux.h"


struct struct_client_args{
		char path[PATH_MAX];
		char client_addr[16];
		int socket;
};

typedef struct struct_client_args client_args;

void print_client_args( client_args *client){
		fprintf( stdout, "SOCKET: %d\nIP:PORT: %s\n", client->socket, client->client_addr );
}

int load_file_memory_posix( char *path){
	
	// open get file descriptor associated to file
	int fd = open ( path , O_RDWR );
	if ( fd < 0 ){ 
		fprintf( stderr,"System call fdopen() failed because of %s", strerror(errno));
	 	exit(5);
	}
	// declare struct for 3th argument for fcntl and memset it to 0
	struct flock lck;
	if( memset(&lck, 0, sizeof(lck)) == NULL ){ 
		fprintf( stderr,"System call memset() failed because of %s", strerror(errno));
	 	exit(5);
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
	lck.l_pid = getppid(); // process holding the lock, we use PPID for all file lock

	/* this version use SETLKW with associed lock at couple [i-node,process], so threads share the lock
	but forked process nope, becouse they have differend PID. But all have the same DAD the PPID we use that
	for declare a only lock for file. */
	fcntl (fd, F_SETLKW, &lck);
	// now we have the lock "load file in memory"

	/* initialize the memory for load the file, 
	fseek put the FP at END ftell say the position ( file size ), we come back at start with SEEK_SET*/
	FILE* fp = fdopen(fd, "r");
	if ( fp == NULL ){ 
		fprintf( stderr,"System call fdopen() failed because of %s", strerror(errno));
	 	exit(5);
	}
	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *file_content = malloc( (len+1) );
	fread( file_content, 1, len, fp);
	file_content[len]= '\0';
	
	// we check the tipe or file with file bash command
	/* char command[ (strlen(path))+5 ]; 
	command = strcat( "file", path );
	FILE* popen_output_stream = popen( command , "r" )
	if ( popen_output_stream == NULL ){ 
		fprintf( stderr,"System call popen() failed because of %s", strerror(errno));
	 	exit(5);
	}
	char* popen_output = malloc( Pat );
	while ( fgets(popen_output, 100, popen_output_stream) != NULL)
		printf("%s", path); */

	// release lock with F_UNLCK flag and FP FD
	fclose(fp);
	close(fd);
	lck.l_type = F_UNLCK;
	fcntl (fd, F_SETLK, &lck);

	// qui generare thread che spedisce il file al momento lo stampo
	fprintf( stdout , "file: %s", file_content);
	free(file_content);
}

// VERSIONE LINUX, NON POSIX dovremmo chiedere al prof se si può usare ma non credo
int load_file_memory_linux( char *path){
	// open get file descriptor associated to file
	int fd = open ( path , O_RDWR );
	if ( fd < 0 ){ 
		fprintf( stderr,"System call fdopen() failed because of %s", strerror(errno));
	 	exit(5);
	}
	// declare struct for 3th argument for fcntl and memset it to 0
	struct flock lck;
	if( memset(&lck, 0, sizeof(lck)) == NULL ){ 
		fprintf( stderr,"System call memset() failed because of %s", strerror(errno));
	 	exit(5);
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

	/* OFD is a flag of Linux, not posix, more problably he become new standard in 
	POSIX 1. 
	The principal difference between OFD and non is that whereas
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
	FILE* fp = fdopen(fd, "r");
	if ( fp == NULL ){ 
		fprintf( stderr,"System call fdopen() failed because of %s", strerror(errno));
	 	exit(5);
	}
	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *file_content = malloc( (len+1) );
	fread( file_content, 1, len, fp);
	file_content[len]= '\0';

	// we check the tipe or file with file bash command
	/* char command[ (strlen(path))+5 ]; 
	command = strcat( "file", path );
	FILE* popen_output_stream = popen( command , "r" )
	if ( popen_output_stream == NULL ){ 
		fprintf( stderr,"System call popen() failed because of %s", strerror(errno));
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

	fprintf(stdout , "file: %s", file_content);
	free(file_content);
}

// this fuction open a listener socket
void open_socket(){
	int on=1;
	struct sockaddr_in serv_addr;
	/*The socket() API returns a socket descriptor, which represents an endpoint.
		The statement also identifies that the INET (Internet Protocol) 
		address family with the TCP transport (SOCK_STREAM) is used for this socket.*/
	if ( (SERVER_SOCKET = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
		fprintf( stderr,"System call socket() failed because of %s", strerror(errno));
	 	exit(5);
	}

	/*The ioctl() API allows the local address to be reused when the server is restarted 
	before the required wait time expires. In this case, it sets the socket to be nonblocking. 
	All of the sockets for the incoming connections are also nonblocking because they inherit that state from the listening socket. */
	if ( (ioctl(SERVER_SOCKET, FIONBIO, (char *)&on)) < 0 ){
		fprintf( stderr,"System call ioctl() failed because of %s", strerror(errno));
		close(SERVER_SOCKET);
		exit(5);
	}

	/*declare sockaddr_in */   
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons( SERVER_PORT );

	// bind to join the unamed socket with sockaddr_in and become named socket
	if( bind( SERVER_SOCKET , (struct sockaddr*)&serv_addr ,  sizeof(serv_addr)) == -1 ){
		fprintf( stderr,"System call bind() failed because of %s", strerror(errno) );
		exit(5);
	}

	/* listen allows the server to accept incoming client connection  */
	if ( (listen( SERVER_SOCKET, 32)) < 0){
		fprintf( stderr,"System call listen() failed because of %s", strerror(errno) );
		exit(5);
	}

}

// this function check if a line contain a new config
int check_if_conf(char line[]){

	fprintf(stdout,"linea letta da conf:\n%s", line);
	int port_change=false;
	// if line is type "mode [t/p]"
	if( strncmp("mode",line,4)==0 ){
		char mode;
		memcpy( &mode, &line[5], 1 );
		if(mode=='t'){
			MODE_CLIENT_PROCESSING=0;
		}
		if(mode=='p'){
			MODE_CLIENT_PROCESSING=1;
		}
		//fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
	}
	
	// if line is "port XXX" with XXX a port number
	if( strncmp("port",line,4)==0 ){
		long int val;
		val=strtol( &line[5], NULL, 10 );
		if( val != SERVER_PORT){
			SERVER_PORT=val;
			port_change=true;
		}
	}
	return port_change;
}

// this function read the sacagawea.conf line by line 
int read_and_check_conf(){
	// some declaretion 
	FILE *fp;
	const size_t max_line_size=100;
	char line[max_line_size];
	int end_while=true;
	int port_change=false;
	//open config file and check if an error occured
	fp = fopen( SACAGAWEACONF_PATH , "r");
	if(fp==NULL){
		fprintf( stderr,"System call fopen() failed because of %s", strerror(errno));
	 	exit(5);
	}

	//readline or 100 char
	do{
		if( fgets( line, max_line_size, fp)==NULL){
			if(feof(fp)){
				end_while=false;
			}else{
				fprintf( stderr,"System call fgets() failed because of %s", strerror(errno));
				exit(5);
			}
		}
		// check if the line is a config line
		if( (strlen(line)!=100) && (check_if_conf(line)) ){
			port_change=true;
		}
	}while(end_while);

	return port_change;
}

// this function is called when SIGHUP coming 
void config_handler(int signum){
	/* Check sagacawea.conf, if the return's value is true the socket SERVER_PORT 
	change so we have to close the socket finish the instaured connection
	and restart the socket with the new SERVER_PORT */
	if( read_and_check_conf() ){
		fprintf(stdout,"SERVER_SOCKET CHANGE %d\n",SERVER_SOCKET);
		/* shutdown with SHUT_WR stop the socket response, he don't send data anymore on that socket.
		so if a new connection request ( SYN ) coming he don't answert ( SYN ACK ). */
		if ( shutdown(SERVER_SOCKET, SHUT_WR) < 0 ){
			fprintf( stderr,"System call shutdown() failed because of %s\n", strerror(errno) );
			exit(5);
		}
		// now we accept all remaining connected comunication which did 3WHS
		int new_s;
		do{
			new_s = accept(SERVER_SOCKET, NULL, NULL);
			fprintf( stdout ,"new_s = %d\n", new_s );
			if (new_s < 0){
				/* remember, we do a NON BLOCK socket, so if we have finished the waiting connections,
				accept will return -1 with EWOULDBLOCK errno */
				if (errno != EWOULDBLOCK){
					fprintf( stderr,"System call accept() failed because of %s\n", strerror(errno) );
					exit(5);
				}else{
					break;
				}
			}else if(new_s > 0){
				/* add the descriptor associated at new connection at fds_set, then select can 
				controll when is readable */
				FD_SET(new_s, &fds_set);
			}
		} while ( new_s != 0);

		// close definitely the listen server socket
		close(SERVER_SOCKET);
		// Leave the closed socket from fds_set 
		FD_CLR( SERVER_SOCKET, &fds_set);
		if( SERVER_SOCKET==max_num_s){
			while ( FD_ISSET(max_num_s , &fds_set) == false ){
				max_num_s--;
			}
		}
		// Open the new listen socket at new PORT
		open_socket();
		// Add new socket at set of socket to select
		FD_SET(SERVER_SOCKET, &fds_set);
		// in case, set the new max descriptor 
		if (SERVER_SOCKET > max_num_s){  
			max_num_s = SERVER_SOCKET;
		}

	}
}

// this function spawn process to management the new client request 
int process_management( client_args *client_info ){

}


void *thread_function( void* c ){
	// declare a variable of STRUCT client_args
	client_args *client_info;
	client_info = (client_args*) c;
	
	int check;
	int read_byte=0;
	int sd = (*client_info).socket; // dato che (*client_info).socket era troppo lungo da riscrivere sempre ho usato sd 
	char input[PATH_MAX]; // becouse the request is a path and the max path is 4096 
						// char length we create a string of 4096 char

	/* Receive data on this connection until the recv \n of finish line.
	If any other failure occurs, we will close the connection.    */
	int stop=true;
	while( stop ){
		check = recv(sd, &input[read_byte], (PATH_MAX-read_byte), 0);
		if (check < 0){
			if (errno != EWOULDBLOCK){
				// if recv fail the error can be server side or client side so we close the connection and go on 
				fprintf( stderr,"System call recv() of sd - %d, failed because of %s we close that connection\n", sd, strerror(errno) );
				return;
			}
			fprintf( stderr,"System call recv() of sd - %d EWOULDBLOCK", sd );
			continue;
		}
		/* Check to see if the connection has been closed by the client, so recv return 0  */
		if (check == 0){
			printf("	Connection closed %d\n", sd );
			stop = false;
		}
		if( check > 0){
			fprintf( stdout, "READ: %s ,byte: %d\n", &input[read_byte] , check );
			read_byte += check;
			if(input[ (read_byte-1) ]=='\n'){
				stop = false;
			}
		}
	}

	// if we are there check is the number of bytes read from client, print that message
	printf("  %d bytes received\n", read_byte);
	// stampo indietro il messaggio sempre prova per vedere il funzionamento "non eliminare"
	check = send(sd, input, read_byte, 0);
	if (check < 0){
		// same of recv
		fprintf( stderr,"System call send() of sd - %d, failed because of %s", sd, strerror(errno) );
						//exit(5);
						// or can be a client error so we have only to close connection
						//close_conn = true;
	}

}
// this function spawn thread to management the new client request 
int thread_management( client_args *client_info ){
	pthread_t tid;
	print_client_args( client_info );
	pthread_create(&tid, NULL, thread_function, (void *) client_info );
}



// this function call the select() and check the FDS_SET if some socket is readable
int listen_descriptor(){
	// Some declaretion of usefull variable
	struct sockaddr_in client_addr;
	int i, num_fd_ready, check, client_addr_len;
	struct timeval timeout;
	fd_set working_set;
	int close_conn;
	char str_client_addr[ (12+3+1+5) ]; // max lenght of IP is 16 254.254.254.254 + 5 char for port 65000
	int new_s;
	// struct defined in sacagawea.h for contain client information
	client_args *client_info;
	client_info = (client_args*) malloc( sizeof(client_args));
	memset( client_info, 0, sizeof(client_info));
	/* Initialize the timeval struct to 13 minutes.  If no        
	activity after 13 minutes this program will end.           */
	timeout.tv_sec  = 13 * 60;
	timeout.tv_usec = 0;

	/* create a copy of fds_set called working_set, is a FD_SET to work on  */
	memcpy( &working_set, &fds_set, sizeof(fds_set));

	// start select and check if failed
	printf("Waiting on select()...\n");
	check = select( max_num_s+1, &working_set, NULL, NULL, &timeout);
	/* if errno==EINTR the select is interrupted becouse of sigaction 
	so we have to repeat select, not exit(5) */
	if ( (check < 0) && (errno != EINTR) ){
		fprintf( stderr,"System call select() failed because of %s", strerror(errno) );
		exit(5);
	}// Chek if select timed out
	if (check == 0){
		printf("select() timed out. End program.\n");
		return true;
	}
	/* 1 or more descriptors are readable we have to check which they are */
	num_fd_ready=check;
	
	// for, for check all ready FD in fds_set until, FD are finish or we check all the ready fd


	// qui ci sta un for perche avevo fatto una versione differente prima , devo sistemare don't worry
	// in realta il for non serve perche prima inserivo anche le nuove connessioni dentro FD_SET cosi
	// creavo il thread/processo solo quando era effettivamente leggibile, ma non cambiava nulla anzi
	// mi complicavo la vita a dover creare un dizionario per salvarmi informazioni ecc... dopo lo sistemo
	for (i=0;  i <= max_num_s && num_fd_ready > 0; ++i){
		close_conn = false;
		// Check to see if the i-esimo descriptor is ready
		if (FD_ISSET(i, &working_set)){
			/* if we come there, the descriptor is readable. */
			num_fd_ready -= 1;

			if (i==SERVER_SOCKET){
				printf("Listening socket is readable \n");
				/*Accept all incoming connections that are queued up on the listening socket before we
				loop back and call select again. */
				do{
					/*Accept each incoming connection.  If accept fails with EWOULDBLOCK,
					then we have accepted all of them.
					Any other failure on accept will cause us to end the server.  */
					memset(&client_addr, 0, sizeof(client_addr));
					client_addr_len=sizeof(client_addr);
					new_s = accept(SERVER_SOCKET, &client_addr, &client_addr_len);
					if (new_s < 0){
						if (errno != EWOULDBLOCK){
							fprintf( stderr,"System call socket accept() failed because of %s", strerror(errno) );
							exit(5);
						}
						break;
					}
					/* we create a t/p for management the incoming connection, call the right function with (socket , client_addr) as argument */
					snprintf( client_info->client_addr, 16, "%s:%d", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
					printf("New connection stabilished at fd - %d from %s\n", new_s, str_client_addr);
					client_info->socket=new_s;
					
					if ( MODE_CLIENT_PROCESSING == 0){
						thread_management( client_info );
					}else{
						process_management( client_info );
					}

				}while (new_s != -1);
					
			}
		} // End of select loop
	}
	return false; 
}
