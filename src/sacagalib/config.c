#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifndef _WIN32
#include <sys/socket.h>
#endif

#include "sacagalib.h"

// this function check if a line contain a new config, FINITA
// RETURN true if change SERVER_PORT, false in all other cases
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

// this function read the sacagawea.conf line by line  FINITA
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
		fprintf(stderr, S_ERROR_FOPEN, (char*) strerror(errno));
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

#ifndef _WIN32
// check this fuction
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
			fprintf( stderr,"shutdown() failed: %s\n", (char*) strerror(errno) );
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
					fprintf( stderr,"accept() failed: %s\n", (char*) strerror(errno) );
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
		FD_CLR(SERVER_SOCKET, &fds_set);
		if( SERVER_SOCKET == max_num_s){
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
#endif
