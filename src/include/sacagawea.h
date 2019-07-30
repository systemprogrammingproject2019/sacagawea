#ifndef SACAGAWEA_H
#define SACAGAWEA_H

/* This is needed in order to export functions to the DLL
 * (Linux doesn't need an equivalent for the .so file because)
 * all the functions are imported by default
 */
#pragma once // exported.h
#ifdef _WIN32
# ifdef WIN_EXPORT
#   define EXPORTED  __declspec(dllexport)
# else
#   define EXPORTED  __declspec(dllimport)
# endif
#else
# define EXPORTED
#endif

#define true     1
#define false    0
#define PATH_MAX 4096 // in Linux the max path is 4096 chars
#define SACAGAWEACONF_PATH "sacagawea.conf"
#define DEFAULT_SERVER_PORT 7070

extern int SERVER_PORT;
char MODE_CLIENT_PROCESSING = 0; // 0=thread 1=subProcess
int SERVER_SOCKET; // the socket descriptor of the server

EXPORTED int check_if_conf(char line[]);
EXPORTED void open_socket();
EXPORTED int read_and_check_conf();

/* Universal strings */
#define S_LINE_READ_FROM_CONF_FILE "Line read from conf file: %s"
#define S_MODE              "mode"
#define S_MODE_THREADED     't'
#define S_MODE_MULTIPROCESS 'p'
#define S_PORT "port"
#define S_ERROR_FOPEN "System call fopen() failed because of %s"
#define S_ERROR_FGETS "System call fgets() failed because of %s"

#endif