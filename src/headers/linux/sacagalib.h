#ifndef SACAGALIB_H
#define SACAGALIB_H

fd_set fds_set;
int max_num_s;

void config_handler(int signum);
int listen_descriptor();
int load_file_memory_linux(char *path);

#endif