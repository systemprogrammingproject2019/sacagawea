#ifndef SACAGAWEA_H
#define SACAGAWEA_H

#define true     1
#define false    0
#define PATH_MAX        4096 // in Linux the max path is 4096 chars
#define SACAGAWEACONF_PATH "sacagawea.conf"

int SERVER_PORT=7070;
int MODE_CLIENT_PROCESSING=0; // 0=thread 1=subProcess
int SERVER_SOCKET; // the socket descriptor of the server

int check_if_conf(char line[]);
void open_socket();
int read_and_check_conf();

/* Universal strings */
#define S_LINE_READ_FROM_CONF_FILE "Line read from conf file: %s"
#define S_MODE              "mode"
#define S_MODE_THREADED     "t"
#define S_MODE_MULTIPROCESS "p"
#define S_PORT "port"
#define S_ERROR_FOPEN "System call fopen() failed because of %s"
#define S_ERROR_FGETS "System call fgets() failed because of %s"

#endif