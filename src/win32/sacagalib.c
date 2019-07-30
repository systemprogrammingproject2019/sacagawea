#include <stdlib.h>
#include <stdio.h>

#include "sacagawea.h"
#include "win32/sacagalib.h"

// this function check if a line contain a new config
int check_if_conf(char line[]) {
	fprintf(stdout, S_LINE_READ_FROM_CONF_FILE, line);
	int port_change = false;
	// if line is type "mode [t/p]"
	if(strncmp(S_MODE, line,4) == 0) {
		char mode;
		memcpy(&mode, &line[5], 1);
		if(mode == S_MODE_THREADED) {
			MODE_CLIENT_PROCESSING = 0;
		}
		if(mode == S_MODE_MULTIPROCESS) {
			MODE_CLIENT_PROCESSING = 1;
		}
		//fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
	}

	// if line is "port XXX" with XXX a port number
	if(strncmp(S_PORT, line, 4) == 0) {
		long int val;
		val = strtol(&line[5], NULL, 10);
		if(val != SERVER_PORT) {
			SERVER_PORT = val;
			port_change = true;
		}
	}
	return port_change;
}

// this fuction open a listener socket
void open_socket() {

}

// this function read the sacagawea.conf line by line 
int read_and_check_conf() {
	// some declaretion 
	FILE *fp;
	const size_t max_line_size = 100;
	char line[max_line_size];
	int keep_going = true;
	int port_change = false;
	//open config file and check if an error occured
	fp = fopen(SACAGAWEACONF_PATH, "r");
	if(fp == NULL) {
		fprintf(stderr, S_ERROR_FOPEN, strerror(errno));
	 	exit(5);
	}

	//readline or 100 char
	do {
		if(fgets(line, max_line_size, fp) == NULL) {
			if(feof(fp)) {
				keep_going = false;
			} else {
				fprintf(stderr, S_ERROR_FGETS, strerror(errno));
				exit(5);
			}
		}
		// check if the line is a config line
		if((strlen(line) != 100) && (check_if_conf(line))) {
			port_change = true;
		}
	} while(keep_going);

	return port_change;
}

