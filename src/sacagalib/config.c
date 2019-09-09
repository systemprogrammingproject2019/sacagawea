#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#define PCRE2_CODE_UNIT_WIDTH 8 // every char is 8 bits
#include <pcre2.h>
#else
#include <regex.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#endif

#include "sacagalib.h"

// this function check if a line contain a new config, FINITA
// RETURN true if change SERVER_PORT, false in all other cases
int check_if_conf(const char* line) {
	char* match;

	int port_change = false;
	write_log(INFO, S_LINE_READ_FROM_CONF_FILE, line);
	// if line is type "mode [t/p]"
	if ( strlen( match = do_regex( ("^" S_MODE "[[:space:]]+([tp])"), line) ) ) {
		char mode;
		mode = match[0];
		if (mode == S_MODE_THREADED) {
			MODE_CLIENT_PROCESSING = 0;
		}
		if (mode == S_MODE_MULTIPROCESS) {
			MODE_CLIENT_PROCESSING = 1;
		}
		//fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
	} else {
		if( strlen( match = do_regex(("^" S_PORT "[[:space:]]+([0-9]+)"), line) ) ) {
			// if line is "port XXX" with XXX a port number
			long int val;
			val = strtol(match, NULL, 10);
			fprintf(stdout,"%d  -  %s\n", val , match);

			if ( (val != SERVER_PORT) && (val < 65536) ) {
				fprintf(stdout,"here\n");
				SERVER_PORT = val;
				port_change = true;
			}
		}
	}
	return port_change;
}

// WITHOUT REGEX
//int check_if_conf(const char* line) {
	// fprintf(stdout, S_LINE_READ_FROM_CONF_FILE, line);
	// int port_change=false;
	// // if line is type "mode [t/p]"
	// if( strncmp(S_MODE ,line,4)==0 ){
	// 	char mode;
	// 	memcpy( &mode, &line[5], 1 );
	// 	if(mode == S_MODE_THREADED){
	// 		MODE_CLIENT_PROCESSING=0;
	// 	}
	// 	if(mode == S_MODE_MULTIPROCESS){
	// 		MODE_CLIENT_PROCESSING=1;
	// 	}
	// 	//fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
	// }
	
	// // if line is "port XXX" with XXX a port number
	// if( strncmp(S_PORT,line,4)==0 ){
	// 	long int val;
	// 	val=strtol( &line[5], NULL, 10 );
	// 	if( val != SERVER_PORT){
	// 		SERVER_PORT=val;
	// 		port_change=true;
	// 	}
	// }
	// return port_change;
//}

char* do_regex(const char* pattern, const char* str) {
#ifdef _WIN32
	PCRE2_SIZE BUFLEN = 256;
	char *r = malloc(BUFLEN);
	pcre2_code *re;
	int errorcode, rc;
	PCRE2_SIZE *ovector;
	pcre2_match_data *match_data;
	PCRE2_SIZE erroroffset;

	/* Compile regular expression */
	re = pcre2_compile (
			pattern,               /* the pattern */
			PCRE2_ZERO_TERMINATED, /* indicates pattern is zero-terminated */
			0,                     /* default options */
			&errorcode,            /* for error code */
			&erroroffset,          /* for error offset */
			NULL                   /* use default compile context */
	);

	/* Compilation failed: print the error message and exit. */
	if (re == NULL) {
		PCRE2_UCHAR buffer[256];
		pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
		write_log("PCRE2 compilation failed at offset %d: %s", (int)erroroffset, buffer);
		exit(1);
	}

	unsigned int offset = 0;
	unsigned int len = strlen(str);
	match_data = pcre2_match_data_create(20, NULL);
	rc = pcre2_match(re, str, -1, 0, 0, match_data, NULL);
	if (rc <= 0) {
		// printf("No match!\n");
	} else {
		ovector = pcre2_get_ovector_pointer(match_data);
		// printf("Match succeeded at offset %llu\n", ovector[0]);
		/* Use ovector to get matched strings */
		PCRE2_SPTR start = str + ovector[0];
		PCRE2_SIZE slen = ovector[2] - ovector[0];
		pcre2_substring_copy_bynumber(match_data, 1, r, &BUFLEN);
	}
	pcre2_match_data_free(match_data);
	pcre2_code_free(re);

	return r;
#else
	// using posix regexes for the linux version
	regex_t regex;
	int reti;
	char msgbuf[100];
	regmatch_t regmatch[2];

	/* Compile regular expression */
	reti = regcomp(&regex, pattern, 0);
	if (reti) {
		write_log(ERROR, "Could not compile regex\n");
		exit(1);
	}

	/* Execute regular expression */
	reti = regexec(&regex, str, 2, regmatch, 0);
	if (!reti) {
		// puts("Match");
	} else if (reti == REG_NOMATCH) {
		// puts("No match");
		return "\0";
	} else {
		// this one gets triggered when no match is found...?
		regerror(reti, &regex, msgbuf, sizeof(msgbuf));
		// write_log(ERROR, "Regex match failed: %s\n", msgbuf);
		return "\0";
		// exit(1);
	}


	char* r = malloc(regmatch[1].rm_eo - regmatch[1].rm_so + 1);
	memcpy(r, &str[regmatch[1].rm_so], regmatch[1].rm_eo - regmatch[1].rm_so);
	r[regmatch[1].rm_eo - regmatch[1].rm_so] = '\0';
	/* Free memory allocated to the pattern buffer by regcomp() */
	regfree(&regex);
	return r;
#endif
}

// this function read the sacagawea.conf line by line  FINITA
int read_and_check_conf() {
	// some declaretion 
	FILE *fp;
	const size_t max_line_size = 128;
	char line[max_line_size];
	int port_change = false;
	//open config file and check if an error occured
	fp = fopen(SACAGAWEACONF_PATH , "r");
	if (fp == NULL) {
		write_log(ERROR, S_ERROR_FOPEN, (char*) strerror(errno));
	 	close_socket_kill_process( SERVER_SOCKET, 5);
	}

	//readline or max_line_size chars
	while (true) {
		if (fgets(line, max_line_size, fp) == NULL) {
			if (feof(fp)) {
				break;
			} else {
				write_log(ERROR, S_ERROR_FGETS, strerror(errno));
				close_socket_kill_process( SERVER_SOCKET, 5);
			}
		}
		size_t line_len = strlen(line);
		if (line[line_len - 1] != '\n') {
			write_log(WARNING, "Config line too long: %s", line);
		} else {
			// replace '\n' with '\0'
			line[line_len - 1] = '\0';
		}
		// check if the line is a config line
		if ( (line_len <= max_line_size) && (check_if_conf(line)) ) {
			port_change = true;
		}
	};

	return port_change;
}

#ifndef _WIN32
// check this fuction
// this function is called when SIGHUP coming 
void config_handler(int signum) {
	/* Check sagacawea.conf, if the return's value is true the socket SERVER_PORT 
	change so we have to close the socket finish the instaured connection
	and restart the socket with the new SERVER_PORT */
	fprintf( stdout, "config file %d\n", read_and_check_conf() );
	if ( read_and_check_conf() ) {
		write_log(INFO, "SERVER_SOCKET CHANGE %d",SERVER_SOCKET);
		/* shutdown with SHUT_WR stop the socket response, he don't send data anymore on that socket.
		so if a new connection request ( SYN ) coming he don't answert ( SYN ACK ). */
		if (shutdown(SERVER_SOCKET, SHUT_WR) < 0) {
			write_log(ERROR, "shutdown() failed: %s", (char*) strerror(errno));
			close_socket_kill_process( SERVER_SOCKET, 5);
		}
		int EX_SERVER_SOCKET = SERVER_SOCKET;
		// Open the new listen socket at new PORT
		open_socket();
		// Add new socket at set of socket to select
		FD_SET(SERVER_SOCKET, &fds_set);
		// in case, set the new max descriptor 
		if (SERVER_SOCKET > max_num_s) {  
			max_num_s = SERVER_SOCKET;
		}

		// now we accept all remaining connected comunication which did 3WHS
		int new_s;
		unsigned int client_addr_len;
		// client_addr to take ip:port of client
		struct sockaddr_in client_addr;
		// client_info for save all info of client
		client_args *client_info;
		client_info = (client_args*) malloc(sizeof(client_args));
		memset(client_info, 0, sizeof(client_args));

		do {
			memset(&client_addr, 0, sizeof(client_addr));
			client_addr_len = sizeof(client_addr); // save sizeof sockaddr struct becouse accept need it
			new_s = accept(SERVER_SOCKET,
					(__SOCKADDR_ARG) &client_addr,
					&client_addr_len);
			if (new_s < 0){
				if (errno != EWOULDBLOCK){
					write_log(ERROR, "socket accept() failed: %s", strerror(errno) );
					close_socket_kill_process( SERVER_SOCKET, 5);
				}
				break;
			}
			/* we create a t/p for management the incoming connection, call the right function with (socket , client_addr) as argument */
			snprintf( client_info->client_addr, ADDR_MAXLEN, "%s:%d", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
			write_log(INFO, "New connection estabilished at fd - %d from %s", new_s, client_info->client_addr);
			client_info->socket = new_s;

			if (MODE_CLIENT_PROCESSING == 0) {
				thread_management(client_info);
			} else {
				if (MODE_CLIENT_PROCESSING == 1) {
					process_management(client_info);
				} else {
					write_log(ERROR, "WRONG MODE PLS CHECK: %d", MODE_CLIENT_PROCESSING );
					close_socket_kill_process( SERVER_SOCKET, 5);
				}
			}
		} while ( new_s != 0);

		// close definitely the listen server socket
		close(EX_SERVER_SOCKET);
		// Leave the closed socket from fds_set 
		FD_CLR(EX_SERVER_SOCKET, &fds_set);
		if( EX_SERVER_SOCKET == max_num_s) {
			while (FD_ISSET(max_num_s , &fds_set) == false) {
				max_num_s--;
			}
		}
	}
}
#endif
