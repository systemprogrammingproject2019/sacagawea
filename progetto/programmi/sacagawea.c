#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h> // for close

#include "sacagawea.h"


int main(int argc, char *argv[])
{

  load_file_memory_posix( "conf/sacagawea.conf");
  // check the sacagawea.conf
  read_and_check_conf();
  // check if some variable are setted by command line
  int c;
  opterr=0;
	while ( ( c = getopt(argc, argv, "ptP:") ) != -1 ) {
		switch (c){
			case 'p':
				MODE_CLIENT_PROCESSING=1;
        fprintf(stdout,"mode change p: %d\n", MODE_CLIENT_PROCESSING);
	      break;

	    case 'P':
				SERVER_PORT = atoi(optarg);
        fprintf(stdout,"port change: %d\n", SERVER_PORT);
	    	break;

	    case 't':
			  MODE_CLIENT_PROCESSING=0;
        fprintf(stdout,"mode change t: %d\n", MODE_CLIENT_PROCESSING);
	    	break;

			case '?':
        fprintf( stdout,"Usage: sacagawea [-P number_of_port][-p/-t for use subprocess/threads to process 1 client connection]" );
				exit(20);
				break;
		}
	}

  fprintf( stdout, "Server port: %d, mode: %d\n", SERVER_PORT , MODE_CLIENT_PROCESSING);

  // Creating sigaction for SIGHUP
  struct sigaction new_action;
  /* Block other SIGHUP signals while handler runs. */
  sigset_t block_mask;
  sigemptyset (&block_mask);
  sigaddset (&block_mask, SIGHUP);
  /* Set up the structure to specify the new action. */
  new_action.sa_handler = config_handler;
  new_action.sa_mask = block_mask;
  sigemptyset (&new_action.sa_mask);
  /* set SA_RESTART flag, so If a signal handler is invoked meanwhile a system call 
  is running like read/recv etc.. after the handler,
  the system call is restarted and can give EINTR error if fail */ 
  new_action.sa_flags = SA_RESTART;
  /* The sigaction() API change the action taken by a process on receipt of SIGHUP signal. */
  if( sigaction (SIGHUP, &new_action, NULL) < 0 )
  {
    fprintf( stderr,"System call sigaction() failed because of %s", strerror(errno));
   	exit(5);
  }
    
  // open socket call
  open_socket();

  int i;
  
  /* declare FD_SET and initialize it */
  FD_ZERO(&fds_set);
  max_num_s = SERVER_SOCKET;
  FD_SET( SERVER_SOCKET, &fds_set);
  //per controlare roba da me "non eliminare"
  for (i=0;  i <= max_num_s ; ++i)
  {
    fprintf( stdout,"i: %d  is set:  %d\n",i,FD_ISSET(i, &fds_set));
  }

  /* Loop waiting for incoming connects or for incoming data
    on any of the connected sockets.   */
  do
  {
    if( listen_descriptor() ){
      break;
    }
  }while(true);

  // we are out of select loop so we have to close all connection
  for (i=0; i <= max_num_s; ++i)
  {
    if (FD_ISSET(i, &fds_set))
    {
      close(i);
    }
  }

}
