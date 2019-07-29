#ifndef SACAGAWEA_H
#define SACAGAWEA_H

#define true     1
#define false    0
#define PATH_MAX        4096 // in Linux the max path is 4096 chars
#define SACAGAWEACONF_PATH "sacagawea.conf"

int SERVER_PORT=7070;
int MODE_CLIENT_PROCESSING=0; // 0=thread 1=subProcess
int SERVER_SOCKET; // the socket descriptor of the server

#endif