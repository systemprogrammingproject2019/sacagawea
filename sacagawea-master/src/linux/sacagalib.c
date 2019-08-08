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
#include <dirent.h>

#include "sacagawea.h"
#include "linux/sacagalib.h"

#define MAX_FILE_NAME 255 // in Linux the max file name is 255 bytes

int SERVER_PORT = DEFAULT_SERVER_PORT;
char MODE_CLIENT_PROCESSING = 0; // 0=thread 1=subProcess

struct struct_client_args{
		char client_addr[16];
		int socket;
		char *file_to_send;
		long len_file;
};

typedef struct struct_client_args client_args;

struct struct_selector{
		char selector[PATH_MAX];
		int num_words;
		char **words;
};

typedef struct struct_selector selector;


void print_client_args( client_args *client){
		fprintf( stdout, "SOCKET: %d\nIP:PORT: %s\n", client->socket, client->client_addr );
}


void *thread_sender( void* c ){
	// declare a variable of STRUCT client_args
	client_args *client_info;
	client_info = (client_args*) c;

	char c1[]="ciao\n";
	send( client_info->socket, c1, strlen(c1), 0 );

	long bytes_sent = 0;
    while( bytes_sent < client_info->len_file )
    {
		bytes_sent += send( client_info->socket, client_info->file_to_send, (client_info->len_file - bytes_sent) , 0 );
		fprintf( stdout, "sent %ld bytes\n", bytes_sent);
    }
	char c2[]="\n";
	send( client_info->socket, c2, strlen(c2), 0 );

	// qui spedisce il file al momento lo stampo
	/*if ( fwrite( client_info->file_to_send, client_info->len_file, 1, stdout ) < 0 ){
		fprintf( stderr,"fwrite() failed: %s\n", strerror(errno));
	 	exit(5);
	}*/ 
	// this work
	/* G__RUNTIME_PSEUDO_RELOC_LIST_END____imp_GetCurrentProcess.refptr.__xc_z___crt_xt_end____imp_open_socket__security_cookie */
	//fprintf(stdout,"finito\n");
}

int load_file_memory_posix( client_args *client_info , char *path){
	
	// open get file descriptor associated to file
	int fd = open ( path , O_RDWR );
	if ( fd < 0 ){ 
		fprintf( stderr,"open() failed: %s\n", strerror(errno));
	 	exit(5);
	}
	// declare struct for 3th argument for fcntl and memset it to 0
	struct flock lck;
	if( memset(&lck, 0, sizeof(lck)) == NULL ){ 
		fprintf( stderr,"memset() failed: %s\n", strerror(errno));
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
		fprintf( stderr,"fdopen() failed: %s\n", strerror(errno));
	 	exit(5);
	}
	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	client_info->file_to_send = malloc( (len+1) );
	client_info->len_file = len;
	fread( client_info->file_to_send, 1, len, fp);
	client_info->file_to_send[len]= '\0';
	

	// release lock with F_UNLCK flag and FP FD
	fclose(fp);
	close(fd);
	lck.l_type = F_UNLCK;
	fcntl (fd, F_SETLK, &lck);

	
	// create thread to send the file at client
	pthread_t tid;
	pthread_create(&tid, NULL, thread_sender, (void *) client_info );

	//free( client_info->file_to_send );
}

// VERSIONE LINUX, NON POSIX dovremmo chiedere al prof se si può usare ma non credo
int load_file_memory_linux( char *path){
	// open get file descriptor associated to file
	int fd = open ( path , O_RDWR );
	if ( fd < 0 ){ 
		fprintf( stderr,"fdopen() failed: %s\n", strerror(errno));
	 	exit(5);
	}
	// declare struct for 3th argument for fcntl and memset it to 0
	struct flock lck;
	if( memset(&lck, 0, sizeof(lck)) == NULL ){ 
		fprintf( stderr,"memset() failed: %s\n", strerror(errno));
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
	FILE* fp = fdopen(fd, "r");
	if ( fp == NULL ){ 
		fprintf( stderr,"fdopen() failed: %s\n", strerror(errno));
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

	fprintf(stdout , "file: %s", file_content);
	free(file_content);
}

// this fuction opens a listening socket
int open_socket(){
	int on=1;
	struct sockaddr_in serv_addr;
	/*The socket() API returns a socket descriptor, which represents an endpoint.
		The statement also identifies that the INET (Internet Protocol) 
		address family with the TCP transport (SOCK_STREAM) is used for this socket.*/
	if ( (SERVER_SOCKET = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
		fprintf( stderr,"socket failed: %s\n", strerror(errno));
	 	exit(5);
	}

	/*The ioctl() API allows the local address to be reused when the server is restarted 
	before the required wait time expires. In this case, it sets the socket to be nonblocking. 
	All of the sockets for the incoming connections are also nonblocking because they inherit that state from the listening socket. */
	if ( (ioctl(SERVER_SOCKET, FIONBIO, (char *)&on)) < 0 ){
		fprintf( stderr,"ioctl failed: %s\n", strerror(errno));
		close(SERVER_SOCKET);
		exit(5);
	}

	/* Set max recvbuf to match windows version's */
	if (setsockopt(SERVER_SOCKET, SOL_SOCKET, SO_RCVBUF, S_SOCK_RECVBUF_LEN, sizeof(S_SOCK_RECVBUF_LEN))) {
		fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
	}

	/*declare sockaddr_in */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons( SERVER_PORT );

	// bind to join the unamed socket with sockaddr_in and become named socket
	if( bind( SERVER_SOCKET , (struct sockaddr*)&serv_addr ,  sizeof(serv_addr)) == -1 ){
		fprintf( stderr,"bind failed: %s\n", strerror(errno) );
		exit(5);
	}

	/* listen allows the server to accept incoming client connection  */
	if ( (listen( SERVER_SOCKET, 32)) < 0){
		fprintf( stderr,"listen failed: %s\n", strerror(errno) );
		exit(5);
	}
	return SERVER_SOCKET;
}

// this function check if a line contain a new config
int check_if_conf(char line[]){

	fprintf(stdout, S_LINE_READ_FROM_CONF_FILE, line);
	int port_change=false;
	// if line is type "mode [t/p]"
	if( strncmp(S_MODE ,line,4)==0 ){
		char mode;
		memcpy( &mode, &line[5], 1 );
		if(mode == S_MODE_THREADED){
			MODE_CLIENT_PROCESSING=0;
		}
		if(mode == S_MODE_MULTIPROCESS){
			MODE_CLIENT_PROCESSING=1;
		}
		//fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
	}
	
	// if line is "port XXX" with XXX a port number
	if( strncmp(S_PORT,line,4)==0 ){
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
	int keep_going=true;
	int port_change=false;
	//open config file and check if an error occured
	fp = fopen( SACAGAWEACONF_PATH , "r");
	if(fp==NULL){
		fprintf(stderr, S_ERROR_FOPEN, strerror(errno));
	 	exit(5);
	}

	//readline or 100 char
	do{
		if( fgets( line, max_line_size, fp)==NULL){
			if(feof(fp)){
				keep_going=false;
			}else{
				fprintf(stderr, S_ERROR_FGETS, strerror(errno));
				exit(5);
			}
		}
		// check if the line is a config line
		if( (strlen(line)!=100) && (check_if_conf(line)) ){
			port_change=true;
		}
	}while(keep_going);

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
			fprintf( stderr,"shutdown() failed: %s\n", strerror(errno) );
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
					fprintf( stderr,"accept() failed: %s\n", strerror(errno) );
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


// this fuction check if the input contain a selector or not and return it
selector request_to_selector( char *input ){

	int read_bytes;
	selector client_selector;
	memset( &client_selector, 0, sizeof(client_selector));
	// if contain it we take it
	if( (input[0] != '\t') || (input[0] != ' ') ){
		sscanf( input, "%4096s", client_selector.selector);
		read_bytes = strlen(client_selector.selector);
	}else{ // if not, we don't take it
		strcpy( client_selector.selector, "" );
		read_bytes=0;
	}
	
	fprintf( stdout, "\nSELECTOR: %s,%d bytes\n", client_selector.selector , read_bytes );

	/* if the client send a tab \t, it means that the selector is followed by words 
	that need to match with the name of the searched file */
	if( input[ read_bytes ] == '\t' ){
		int i=0;
		client_selector.words = (char **) malloc( 3*sizeof( char *) );
		do{
			// put this check, becouse if the request contain 2+ consecutive \t or 2+ consecutive ' ' the scanf don't read an empty word.
			if( (input[read_bytes] == '\t') || (input[read_bytes] == ' ') ){
				//fprintf( stdout, "CHAR %c\n", input[read_bytes]);
				read_bytes++;
				continue;
			}
			// realloc check. we do a realloc every 3 words, just for limit overhead of realloc, and don't do every word
			// first call do a malloc(3) becouse words=NULL after do realloc( 6 ) ... 9, 12 ...
			if( (i % 3) == 0){
				client_selector.words = (char **) realloc(  client_selector.words, (i+3)*sizeof( char *)  );
			}
			/* declare a space for word and read it, OPPURE c'è l'opzione %m che passandogli  client_selector.words[i], senza 
			fare prima la malloc la fa scanf in automatico della grandezza della stringa letta + 1, sarebbe piu efficente dato che 
			MAX_FILE_NAME è spazio sprecato per parole di piccole dimensione */
			client_selector.words[i] = (char *) malloc( ( (MAX_FILE_NAME+1)*sizeof( char ) ) );
			sscanf( &input[read_bytes], "%255s", client_selector.words[i]);
			// upgrade read_bytes for check when we finish the client input
			read_bytes += ( strlen( client_selector.words[i] )  );
			fprintf( stdout, "WORD %d: %s,%d bytes\n", i, client_selector.words[i] , strlen(client_selector.words[i]) );
			// upgrade the num of words, contained in client_selector
			client_selector.num_words=i;
			i++;

		}while( input[read_bytes] != '\n' );
	}

	return client_selector;
}

int check_security_path( char path[PATH_MAX]){
	/* le cose qui son 2
	1 o facciamo un array di word globale con le parole non accettate
	2 lasciamo stare tanto giusto ".." è da scartare, e teoricamente anche i ".." in UTF a 16 bits 32, non ricordo 
	quanti erano */
	int i=0;
	int cont=0;
	for( i=0; i<PATH_MAX; ++i){
		if( path[i] == '.' ){
			cont++;
		}else{
			cont=0;
		}

		if( cont == 2){
			return 1;
		}
	}
	return 0;
}

// this function take a path as argument and return the gopher char associated.
// in the Gopher.md u can see all gopher char and the translate
char type_path( char path[PATH_MAX] ){

	// we check the tipe or file with file bash command
	char command[ (strlen(path)+9) ];
	// file with -b option: 
	strcpy( command, "file -bi "); // 9 for "file -b " + \0 at end
	strcat( command, path );
	FILE* popen_output_stream = popen( command , "r" );
	if ( popen_output_stream == NULL ){ 
		fprintf( stderr,"popen() failed: %s\n", strerror(errno));
	 	exit(5);
	}
	char popen_output[20]; // is useless read all output, i need only the first section and the max is "application/gopher"
	//while ( fgets( &popen_output, 20, popen_output_stream) != NULL){
	fgets( &popen_output, 20, popen_output_stream);
	fprintf( stdout, "%s\n", popen_output); 
	//}
	close( popen_output_stream );
	

	if( (strncmp( popen_output, DIR_1, strlen(DIR_1))==0 ) || (strncmp( popen_output, GOPHER_1, strlen(GOPHER_1))==0) ){
		return '1';
	}
	if( strncmp( popen_output, MULTIPART_M, strlen(MULTIPART_M))==0 ){
		return 'M';
	}
	if( strncmp( popen_output, APPLICATION_9, strlen(APPLICATION_9))==0 ){
		return '9';
	}
	if( strncmp( popen_output, AUDIO_s, strlen(AUDIO_s)) ==0 ){
		return 's';
	}
	if( strncmp( popen_output, HTML_h, strlen(HTML_h))==0 ){
		return 'h';
	}
	if( ( strncmp( popen_output, TEXT_0, strlen(TEXT_0))==0 ) || ( strncmp( popen_output, EMPTY_0, strlen(EMPTY_0))==0 ) ){
		return '0';
	}
	if( strncmp( popen_output, GIF_g, strlen(GIF_g))==0 ){
		return 'g';
	}
	if( strncmp( popen_output, IMAGE_I, strlen(IMAGE_I))==0 ){
		return 'I';
	}
	if( strncmp( popen_output, MAC_4, strlen(MAC_4))==0 ){
		return '4';
	}
	return '3';
}


void send_content_of_dir( char *temp, int sd , selector client_selector){
	fprintf( stdout , "%s\n" , temp);
	DIR *folder;
	struct dirent *subFile;
	folder = opendir( temp );
	if( folder == NULL ){

	}
	while (( subFile = readdir( folder )) != NULL) {
      printf("%s\n", subFile->d_name);
    }
    closedir(folder);

}


// this function spawn process to management the new client request 
int process_management( client_args *client_info ){

}


void *thread_function( void* c ){
	// declare a variable of STRUCT client_args
	client_args *client_info;
	client_info = (client_args*) c;
	
	char type;
	int check;
	int read_bytes=0;
	int sd = (*client_info).socket; // dato che (*client_info).socket era troppo lungo da riscrivere sempre ho usato sd 
	char *input = malloc( PATH_MAX*sizeof(char) ); // becouse the request is a path (SELECTOR) and the max path is 4096, plus
											// eventualy some words which have to match with file name, wE put a MAX input = 4096

	/* Receive data on this connection until the recv \n of finish line.
	If any other failure occurs, we will close the connection.    */
	int stop=true;
	while( stop ){
		if(  (PATH_MAX-read_bytes) <= 0 ){
			// the client send a wrong input, or the lenght is > PATH_MAX without a \n at end or send more bytes after \n
			close(sd);
			pthread_exit(NULL);
		}
		check = recv(sd, &input[read_bytes], (PATH_MAX-read_bytes), 0);
		if (check < 0){
			if (errno != EWOULDBLOCK){
				// if recv fail the error can be server side or client side so we close the connection and go on 
				fprintf( stderr,"recv() of sd - %d, failed: %s we close that connection\n", sd, strerror(errno) );
				return;
			}
			fprintf( stderr,"recv() of sd - %d EWOULDBLOCK", sd );
			continue;
		}
		/* Check to see if the connection has been closed by the client, so recv return 0  */
		if (check == 0){
			printf("	Connection closed %d\n", sd );
			// client close the connection so we can stop the thread
			close(sd);
			pthread_exit(NULL);
		}
		if( check > 0){
			read_bytes += check;
			if(input[ (read_bytes-1) ]=='\n'){
				stop = false;
			}
		}
	}
	// if we are there, print that message
	//fprintf( stdout, "READ: %s%d bytes at %p\n", input, check, &input );

	// check if the input contain a selector or not
	selector client_selector;
	memset( &client_selector, 0, sizeof(client_selector));

	//request_to_selector( input , client_selector);
	client_selector = request_to_selector( input );
	/* if ( client_selector == NULL ){
		send( sd, S_ERROR_SELECTOR_REQUEST, strlen(S_ERROR_SELECTOR_REQUEST), 0);
	} */
	

	// we have to add the path of gopher ROOT, else the client can access at all dir of server.
	char path_selector[ ( strlen(client_selector.selector) + strlen(S_ROOT_PATH) + 1 ) ];
	strcpy( path_selector, S_ROOT_PATH ); 
	strcat( path_selector, client_selector.selector );	

	
	fprintf(stdout,"%s\n",path_selector);

	if ( strcmp( client_selector.selector , "") == 0  ){
		// if selector is empty we send the content of gophermap, who match with words
		type = type_path( S_ROOT_PATH );
	}else{ // if we have a selector, we check if is a dir or not 
		// little check for avoid trasversal path
		if( check_security_path( client_selector.selector ) ){
			fprintf ( stdout, "eh eh nice try where u wanna go?\n" );
			close(sd);
			pthread_exit(NULL);
		}// add the gopher root path at selector and check the type of file
		type = type_path( path_selector );
	}
	// if is a dir we check the content if match with words 
	if( type == '1' ){
		send_content_of_dir( path_selector, sd, client_selector);
		//pthread_exit( NULL );
	}else{ 
		if( type == '3' ){ // if is an error send the error message
			char temp[ ( strlen(client_selector.selector) + 4 ) ]; // 3 is for lenght of "3\t" + 1 per \n + 1 
			strcpy( temp, "3\t" ); 
			strcat( temp, client_selector.selector );
			strcat( temp, "\n" ); // senza /n non inviava rimaneva in pending nel buffer del socket senza inviare. non so perche
			send( sd, temp, strlen(temp), 0);
			// close socket and thread
			close(sd);
			pthread_exit(NULL);
		}else{ // if is only a file
			load_file_memory_posix( client_info, path_selector );
			// creao thread per spedire il file
			// avviso il processo che carica il file di log
		}
	}
	//fprintf( stdout, "%d\n", sd);
	fprintf( stdout, "%c\n", type);

}

// this function spawn thread to management the new client request 
int thread_management( client_args *client_info ){
	pthread_t tid;
	print_client_args( client_info );
	pthread_create(&tid, NULL, thread_function, (void *) client_info );
}



// this function call the select() and check the FDS_SET if some socket is readable
int listen_descriptor( int useless ){
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
		fprintf( stderr,"select() failed: %s\n", strerror(errno) );
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
							fprintf( stderr,"socket accept() failed: %s\n", strerror(errno) );
							exit(5);
						}
						break;
					}
					/* we create a t/p for management the incoming connection, call the right function with (socket , client_addr) as argument */
					snprintf( client_info->client_addr, 16, "%s:%d", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
					printf("New connection stabilished at fd - %d from %s\n", new_s, client_info->client_addr);
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
