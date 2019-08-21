#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>

#ifdef _WIN32
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif


#include "sacagalib.h"

#ifndef _WIN32
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
 
// This fuction management the thread which have to send the FILE at client
void *thread_sender( void* c ){
	// declare a variable of STRUCT client_args, and cast the arguemnt into
	client_args *client_info;
	client_info = (client_args*) c;
	/* this cicle send the file at client and save the number of bytes sent */
	long bytes_sent = 0, temp;
	while( bytes_sent < client_info->len_file ){
		// MSG_NOSIGNAL means, if the socket be broken dont send SIGPIPE at process
		temp = send( client_info->socket, client_info->file_to_send, (client_info->len_file - bytes_sent) , MSG_NOSIGNAL );
		if( temp < 0){
			fprintf( stderr,"Sending file at %s, with socket %d failed becouse of: %s\n",
					client_info->client_addr, client_info->socket, strerror(errno) );
			close( client_info->socket );
			pthread_exit(NULL);
		}
		if( temp == 0){
			fprintf( stderr,"Client %s, with socket %d close the connection meanwhile sending file\n",
					client_info->client_addr, client_info->socket );
			close( client_info->socket );
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
		close( client_info->socket );
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
		close( client_info->socket );
		pthread_exit(NULL);
	}
	close( client_info->socket );
	
	// if we sent all the file without error we wake up logs_uploader
	fprintf( stdout, "file length %ld\n", bytes_sent, client_info->len_file);
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
		// sent logs string, first we sent the strlen after the string
		len_logs_string = strlen( logs_string );
		fprintf( stdout,"HERE %ld: %s\n", len_logs_string, logs_string);
		write(pipe_conf[1], &len_logs_string, sizeof(int));	
		write(pipe_conf[1], logs_string, len_logs_string);
		// unlock mutex and free
		pthread_cond_signal(cond);
		pthread_mutex_unlock(mutex);
		free(logs_string);
	}
	
	pthread_exit(NULL);
}

// this function take a path as argument and return the gopher char associated.
// in the Gopher.md u can see all gopher char and the translate
char type_path(char path[PATH_MAX]){

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
	
	fgets( &popen_output, 20, popen_output_stream);
	//fprintf( stdout, "%s\n", popen_output); 
	if( strncmp( popen_output, "cannot", 6 )==0 ){
		fprintf( stdout, "%s", popen_output);
		while ( fgets( &popen_output, 20, popen_output_stream) != NULL){
			fprintf( stdout, "%s", popen_output); 
		}
		fprintf( stdout, "\n"); 
	}
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

#endif
