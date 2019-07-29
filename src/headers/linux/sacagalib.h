#ifndef SACAGALIB_H
#define SACAGALIB_H

fd_set fds_set;
int max_num_s;

void open_socket();
int check_if_conf(char line[]);
int read_and_check_conf();
void config_handler(int signum);
int listen_descriptor();
int load_file_memory_linux( char *path);
int load_file_memory_posix( char *path);

#endif