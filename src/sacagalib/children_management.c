#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <poll.h>

#ifdef _WIN32
#else
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif


#include "sacagalib.h"

// this fuction check if the input contain a selector or not and return it
selector request_to_selector( char *input ){

	int read_bytes;
	selector client_selector;
	memset( &client_selector, 0, sizeof(client_selector));
	client_selector.num_words=-1; // -1 mean 0 words, 0=1word .... n=(n-1)words. Like array index
	
	// check if input start with selector or not
	if( ( input[0] == '\t' ) || ( input[0] == ' ') ){ // if not, we don't take it
		strcpy( client_selector.selector, "" );
		read_bytes=0;
	}else{ // if contain it we take it
		sscanf( input, "%4096s", client_selector.selector);
		read_bytes = strlen(client_selector.selector);
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
			MAX_FILE_NAME è spazio sprecato per parole di piccole len_string */
			client_selector.words[i] = (char *) malloc( ( (MAX_FILE_NAME+1)*sizeof( char ) ) );
			sscanf( &input[read_bytes], "%255s", client_selector.words[i]);
			// upgrade read_bytes for check when we finish the client input
			read_bytes += ( strlen( client_selector.words[i] )  );
			fprintf( stdout, "WORD %d: %s,%ld bytes\n", i, client_selector.words[i] , strlen(client_selector.words[i]) );
			// upgrade the num of words, contained in client_selector
			client_selector.num_words=i;
			i++;

		}while( input[read_bytes] != '\n' );
	}

	return client_selector;
}

void send_content_of_dir( client_args *client_info, selector *client_selector){

	fprintf( stdout , "%s\n" , client_info->path_file);
	DIR *folder;
	struct dirent *subFile;
	int j=0;
	int len_responce;
	char type;
	int no_match;
	char* responce;
	char *path_of_subfile;
	char port_str[6]; // max ex "65000\0"
	// open dir 
	folder = opendir( client_info->path_file );
	if( folder == NULL ){

	}

	while (( subFile = readdir( folder )) != NULL) {
		// skip .. and . file
		if (  (strcmp( subFile->d_name , ".." ) == 0) || (strcmp( subFile->d_name , "." ) == 0) ){
			continue;
		}
		/* words are only strings and not regex, so i do that little check for take only 
		subfile who match all words, regexes would be useless and a waste of resources */
		// check word by word if match, if someone don't match we break the for, and don't send the gopher string of file
		for(j=0; j<=client_selector->num_words; j++){
			no_match=false;
			// check if not match
			if( check_not_match( subFile->d_name, client_selector->words[j] ) ){
				no_match=true;
				break;
			}
		}

		if(no_match){ // file don't match something, continue with next subfile
			continue;
		}else{
			// the file match all word we send the data
			printf("%s\n", subFile->d_name);
			path_of_subfile = (char*) malloc( ( strlen(client_info->path_file) + strlen(subFile->d_name) + 2 ) );
			if( client_info->path_file[ (strlen(client_info->path_file)-1) ] == '/' ){
				snprintf( path_of_subfile, ( strlen(client_info->path_file) + strlen(subFile->d_name) + 1 ), "%s%s", client_info->path_file, subFile->d_name);
			}else{
				snprintf( path_of_subfile, ( strlen(client_info->path_file) + strlen(subFile->d_name) + 1 ), "%s/%s", client_info->path_file, subFile->d_name);
			}

			type = type_path( path_of_subfile );
			// calculate lenght of responce. first 2 are for type char + \t
			len_responce = 2; 
			// for name of file +\t
			len_responce += strlen(subFile->d_name) + 1; 
			// for selector, used for serch file in gopher server +\t, ( selector + '/' + file_name + '\t' )
			len_responce += strlen(client_selector->selector) + strlen(subFile->d_name) + 2;
			// for IP of server +\t
			len_responce += strlen( SERVER_DOMAIN ) + 1;
			// for actualy opened SERVER_PORT
			sprintf( port_str, "%d", SERVER_PORT);
			len_responce += strlen( port_str );
			// \n + \0
			len_responce += 4;
			// declare and compile

			responce = (char*) malloc( len_responce*sizeof(char) );
			if( client_selector->selector[ (strlen(client_selector->selector)-1) ] != '/'){
				snprintf( responce, len_responce, "%c\t%s\t%s/%s\t%s\t%d\n", type, subFile->d_name, client_selector->selector, subFile->d_name, SERVER_DOMAIN, SERVER_PORT);
			}else{
				snprintf( responce, len_responce, "%c\t%s\t%s%s\t%s\t%d\n", type, subFile->d_name, client_selector->selector, subFile->d_name, SERVER_DOMAIN, SERVER_PORT);
			}

			fprintf( stdout, "responce at %d: %s\n", client_info->socket, responce);
			send( client_info->socket, responce, strlen(responce), 0);
			
			free(responce);
			free(path_of_subfile);
		}		
    }
	char end[] = ".\n";
	send( client_info->socket, end, strlen(end), 0);
    closedir(folder);

	close( client_info->socket);
	pthread_exit(NULL);
}


// this fuction is the real management of the client responce with thread as son
void *thread_function( void* c ){
	// declare a variable of STRUCT client_args
	client_args *client_info;
	client_info = (client_args*) c;
	
	char type;
	int check;
	int read_bytes=0;
	int sd = (*client_info).socket; // dato che (*client_info).socket era troppo lungo da riscrivere sempre ho usato sd 
	// becouse the request is a path (SELECTOR) and the max path is 4096, plus
	// eventualy some words which have to match with file name, wE put a MAX input = 4096
	char *input = malloc( PATH_MAX*sizeof(char) );

	/* Receive data on this connection until the recv \n of finish line.
	If any other failure occurs, we will close the connection.    */
	int stop=true;
	while( stop ){
		if(  (PATH_MAX-read_bytes) <= 0 ){
			// the client send a wrong input, or the lenght is > PATH_MAX, without a \n at end or send more bytes after \n
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

	client_selector = request_to_selector( input );
	/* if ( client_selector == NULL ){
		send( sd, S_ERROR_SELECTOR_REQUEST, strlen(S_ERROR_SELECTOR_REQUEST), 0);
	} */

	// we have to add the path of gopher ROOT, else the client can access at all dir of server.
	client_info->path_file = (char*) malloc( strlen(client_selector.selector) + strlen(S_ROOT_PATH) + 1 );
	strcpy( client_info->path_file, S_ROOT_PATH ); 
	strcat( client_info->path_file, client_selector.selector );	
	fprintf(stdout,"PATH+SELECTOR: %s\n",client_info->path_file);

	if ( strcmp( client_selector.selector , "") == 0  ){
		// if selector is empty we send the content of gophermap, who match with words
		
		// and the content of ROOT_PATH
		type = type_path( S_ROOT_PATH );

	}else{ /* if we have a selector, we check if is a dir or not.*/

		//	little check for avoid trasversal path	
		if( check_security_path( client_selector.selector ) ){
			fprintf ( stdout, "eh eh nice try where u wanna go?\n" );
			close(sd);
			pthread_exit(NULL);
		}// add the gopher root path at selector and check the type of file
		type = type_path( client_info->path_file );
	}
	// if is a dir we check the content if match with words 
	if( type == '1' ){
		send_content_of_dir( client_info, &client_selector);
		//pthread_exit( NULL );
	}else{ 
		if( type == '3' ){ // if is an error send the error message
			char temp[ ( strlen(client_selector.selector) + 6 ) ]; // 3 is for lenght of "3\t" + 1 per \n + 2 for last line + 1 \0
			strcpy( temp, "3\t" ); 
			strcat( temp, client_selector.selector );
			strcat( temp, "\n.\n" ); // senza \n non inviava rimaneva in pending nel buffer del socket senza inviare. non so perche
			send( sd, temp, strlen(temp), 0);
			// close socket and thread
			close(sd);
			pthread_exit(NULL);
		}else{ // if is only a file
			load_file_memory_and_send_posix( client_info );
		}
	}
	
}
 

// This fuction management the thread which have to send the FILE at client
void *thread_sender( void* c ){
	// declare a variable of STRUCT client_args, and cast the arguemnt into
	client_args *client_info;
	client_info = (client_args*) c;
	/* this cicle send the file at client and save the number of bytes sent */
	long bytes_sent = 0, temp;
	while( bytes_sent < client_info->len_file ){
		temp = send( client_info->socket, client_info->file_to_send, (client_info->len_file - bytes_sent) , 0 );
		if( temp < 0){
			fprintf( stderr,"Sending file at %s, with socket %d failed becouse of: %s\n",
					client_info->client_addr, client_info->socket, strerror(errno) );
			pthread_exit(NULL);
		}
		if( temp == 0){
			fprintf( stderr,"Client %s, with socket %d close the connection meanwhile sending file\n",
					client_info->client_addr, client_info->socket );
			pthread_exit(NULL);
		}
		bytes_sent += temp;
		fprintf( stdout, "sent %ld bytes\n", bytes_sent);
	}
	// at end we send a \n, useless for the fuction of gopher server
	char c2[]="\n";
	send( client_info->socket, c2, strlen(c2), 0 );

	/* allora qua sicuramente c'è una soluzione migliore, questa l'ho inventata io ma mi sembra veramente inteligente come cosa.
	allora curl legge finche il socket è aperto. quindi quando inviavo il file anche se inviato tutto
	lui leggeva aspettanto altri bytes. pertanto faccio lo shutdown ovvero chiudo il socket in scrittura
	dal lato server, cosi curl quando finisce di leggere i bytes inviati si blocca e chiude la comunicazione */ 
	if ( shutdown( client_info->socket, SHUT_WR) < 0 ){
		fprintf( stderr,"shutdown() failed: %s\n", strerror(errno) );
		pthread_exit(NULL);
	}
	/* come detto prima curl finisce la comunicazione quando legge tutto, ma noi non sappiamo quando ha finito
	quindi faccio questo while che cerca di fare una recv, quando la recv ritorna 0 vuol dire che la curl ha chiuso 
	la connessione e quindi ha finito di leggere il file. pertanto posso chiudere definitivamente il socket e il thread */
	char ciao[11];
	int check;
	while( ( check = recv( client_info->socket, ciao, 10, MSG_WAITALL) ) != 0 ){
	// ho fatto questo loop perche se il client manda più di 10 byte la recv termina anche se ha il MSG_WAITALL
		//fprintf(stdout, "- %d - %s\n" , check, ciao);
		sleep( 0.5 );
	}
	if( check < 0 ){
		fprintf( stderr,"recv() failed: %s\n", strerror(errno) );
		pthread_exit(NULL);
	}
	close( client_info->socket );
	
	// if we sent all the file without error we wake up logs_uploader
	fprintf( stdout, "%ld %ld\n", bytes_sent, client_info->len_file);
	if ( bytes_sent >= client_info->len_file ){
		// create line to write to the logs file ""
		// get time
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		// get string to write on logs file
		long len_logs_string;
		len_logs_string += 13; // for [AAAA-MM-GG] 
		len_logs_string += 11; // for [hh-mm-ss]
		len_logs_string += strlen( client_info->path_file ) + 1;
		len_logs_string += strlen( client_info->client_addr ) + 4;
		char *logs_string = malloc( sizeof(char)*len_logs_string );
		if(logs_string==NULL){
			fprintf( stderr,"malloc() failed: %s\n", strerror(errno));
			exit(5);
		}
		if( snprintf( logs_string, len_logs_string, "[%d-%d-%d] [%d:%d:%d] %s %s\n", tm.tm_year + 1900, tm.tm_mon + 1, 
					tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, client_info->path_file, client_info->client_addr) < 0 ){
			fprintf( stderr,"snprintf() failed: %s\n", strerror(errno));
			exit(5);
		}
		
		// lock mutex and wake up process for logs
		pthread_mutex_lock(mutex);
		pthread_cond_signal(cond);
		// sent logs string, first we sent the strlen after the string
		len_logs_string = strlen( logs_string );
		fprintf( stdout,"HERE %ld: %s\n", len_logs_string, logs_string);
		write(pipe_conf[1], &len_logs_string, sizeof(int));	
		write(pipe_conf[1], logs_string, len_logs_string);
		// unlock mutex and free 
		pthread_mutex_unlock(mutex);
		free(logs_string);
	}
	
	pthread_exit(NULL);
}


// this function spawn process to management the new client request 
int process_management( client_args *client_info ){
	int pid;
	if ( ( pid = fork() ) < 0){
		// failed fork on server
	}
	if( pid == 0){
		// child who have to management the connection
		// close server_socket
		close ( SERVER_SOCKET );
		printf("Son spawned ready to serv\n");
		thread_function( (void*) client_info );
		printf("Son finish to serv\n");
		exit(1);
	}else{
		// this is the server
		// close connection and deallocate resourses
		close( client_info->socket );
		free( client_info );
	}
}


// this function spawn thread to management the new client request 
int thread_management( client_args *client_info ){
	pthread_t tid;
	print_client_args( client_info );
	pthread_create(&tid, NULL, thread_function, (void *) client_info );
}

