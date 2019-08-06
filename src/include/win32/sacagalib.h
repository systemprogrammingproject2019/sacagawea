#ifndef SACAGALIB_H
#define SACAGALIB_H

#include "sacagawea.h"

typedef struct client_args {
	char addr[ADDR_MAXLEN];
	SOCKET socket;
};

int thread_management(struct client_args *client_info);
int process_management(struct client_args *client_info);

#endif