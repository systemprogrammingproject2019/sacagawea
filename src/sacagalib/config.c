#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

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
void read_conf_line(const char* line, settings_t* settings) {
	char* match = NULL;
	char matched = false;
	write_log(INFO, S_LINE_READ_FROM_CONF_FILE, line);

	// "mode [t/p]"
	match = do_regex(("^" S_MODE "[[:space:]]+([tp])"), line);
	if (strlen(match)) {
		matched = true;
		char mode = match[0];
		if (mode == S_MODE_THREADED) {
			write_log(DEBUG, "read_conf_line: new mode: 'multithread'");
			settings->mode = 't';
		}
		if (mode == S_MODE_MULTIPROCESS) {
			write_log(DEBUG, "read_conf_line: new mode: 'multiprocess'");
			settings->mode = 'p';
		}
		//fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
	}
	if (matched == true) free(match);
	matched = false;

	// homedir "home-directory [^\n\r]{1,259}"
	match = do_regex(("^" S_HOMEDIR "[[:space:]]+([^\n\r]{1,259})"), line);
	if (strlen(match)) {
		matched = true;
		// open dir
		DIR* folder = opendir(match);
		if (folder == NULL) {
			write_log(ERROR, "Cant find home-directory: %s", match);
			exit(1);
		}
		strncpy(settings->homedir, match, 259);
	#ifdef _WIN32
		if (settings->homedir[strlen(settings->homedir) - 1] != '\\') {
			settings->homedir[strlen(settings->homedir)] = '\\';
			settings->homedir[strlen(settings->homedir) + 1] = '\0';
		}
	#else
		if (settings->homedir[strlen(settings->homedir) - 1] != '/') {
			settings->homedir[strlen(settings->homedir)] = '/';
			settings->homedir[strlen(settings->homedir) + 1] = '\0';
		}
	#endif
		write_log(DEBUG, "read_conf_line: new homedir: \"%s\"", settings->homedir);
	}
	if (matched == true) free(match);
	matched = false;

	// "hostname [^\n\r]{1,255}"
	match = do_regex(("^" S_HOSTNAME "[[:space:]]+([^\n\r]{1,255})"), line);
	if (strlen(match)) {
		matched = true;
		strncpy(settings->hostname, match, 255);
		write_log(DEBUG, "read_conf_line: new hostname: \"%s\"", settings->hostname);
	}
	if (matched == true) free(match);
	matched = false;
}

// this function check if a line contain a new config, FINITA
// RETURN true if change SERVER_PORT, false in all other cases
int check_if_port_change(const char* line, settings_t* settings) {
	char* match = NULL;
	char matched = false;
	int port_change = false;

	// "port [0-9]{1,5}"
	match = do_regex(("^" S_PORT "[[:space:]]+([[:digit:]]{1,5})"), line);
	if ( strlen(match) ) {
		matched = true;
		// if line is "port XXX" with XXX a port number
		long int val = strtol(match, NULL, 10);

		if ((val != 0) && (val != settings->port) && (val < 65536)) {
			write_log(DEBUG, "check_if_port_change: new port: '%d'", val);
			settings->port = val;
			port_change = true;
		}
	}
	if (matched == true) free(match);
	matched = false;

	return port_change;
}

// WITHOUT REGEX
//int read_conf_line(const char* line) {
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
	char *r = calloc(BUFLEN, sizeof(char));
	pcre2_code *re;
	int err, rc;
	pcre2_match_data *match_data;
	PCRE2_SIZE erroroffset;

	/* Compile regular expression */
	re = pcre2_compile (
			(unsigned char *) pattern, /* the pattern */
			PCRE2_ZERO_TERMINATED, /* indicates pattern is zero-terminated */
			0,                     /* default options */
			&err,                  /* for error code */
			&erroroffset,          /* for error offset */
			NULL                   /* use default compile context */
	);

	/* Compilation failed: print the error message and exit. */
	if (re == NULL) {
		PCRE2_UCHAR buffer[256];
		pcre2_get_error_message(err, buffer, sizeof(buffer));
		write_log(ERROR, "PCRE2 compilation failed at offset %d: %s", (int)erroroffset, buffer);
		exit(1);
	}

	match_data = pcre2_match_data_create(20, NULL);
	rc = pcre2_match(re, (unsigned char *) str, -1, 0, 0, match_data, NULL);
	if (rc <= 0) {
		// printf("No match!\n");
	} else {
		pcre2_substring_copy_bynumber(match_data, 1, (unsigned char*) r, &BUFLEN);
	}
	pcre2_match_data_free(match_data);
	pcre2_code_free(re);

	return r;
#else
	// using posix regexes for the linux version
	regex_t regex;
	int reti;
	regmatch_t regmatch[2];

	/* Compile regular expression */
	reti = regcomp(&regex, pattern, REG_EXTENDED);
	if (reti) {
		write_log(ERROR, "Could not compile regex\n");
		exit(1);
	}

	/* Execute regular expression */
	reti = regexec(&regex, str, 2, regmatch, 0);
	if (reti == REG_NOMATCH) {
		// puts("No match");
		regfree(&regex);
		return "\0";
	}

	char* r = calloc(regmatch[1].rm_eo - regmatch[1].rm_so + 1, sizeof(char));
	memcpy(r, &str[regmatch[1].rm_so], regmatch[1].rm_eo - regmatch[1].rm_so);
	r[regmatch[1].rm_eo - regmatch[1].rm_so] = '\0';
	/* Free memory allocated to the pattern buffer by regcomp() */
	regfree(&regex);
	return r;
#endif
}

// this function read the sacagawea.conf line by line  FINITA
int read_and_check_conf(settings_t* settings, int called_from_handler) {
	// some declaretion 
	FILE *fp;
	const size_t max_line_size = 512;
	char line[max_line_size];
	int port_change = false;
	//open config file and check if an error occured
	fp = fopen(SACAGAWEACONF_PATH , "r");
	if (fp == NULL) {
		write_log(ERROR, S_ERROR_FOPEN, (char*) strerror(errno));
	 	exit(5);
	}

	//readline or max_line_size chars
	while (true) {
		if (fgets(line, max_line_size, fp) == NULL) {
			if (feof(fp)) {
				break;
			} else {
				write_log(ERROR, S_ERROR_FGETS, strerror(errno));
				exit(5);
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
		if (line_len <= max_line_size) {
			if (!called_from_handler) {
				// if this was not called from the handler, check 
				// other settings too
				read_conf_line(line, settings);
			}
			// check for port change
			int check_return = check_if_port_change(line, settings);
			if( !check_return ){
				port_change = true;
			}
		}
	}

	fclose(fp);
	return port_change;
}

