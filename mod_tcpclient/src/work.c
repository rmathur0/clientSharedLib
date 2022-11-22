#include "../include/headers.h"
#include "../include/module.h"
#include "../include/task.h"
#include "../include/work.h"
#define FIFO "/tmp/myfifo"

#define BLOCK_SIZE 1024

configurator *ref_gcfg;
con_t *gcl;
msgq_t *gmq;

/* Signal handler function (defined below). */
static void sighandler(int signal);

/**
 *  * Set a socket to non-blocking mode.
 *   */
static int setnonblock(int fd) {
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0) return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) return -1;
	return 0;
}


void *monitor_thread(void *arg) {
        int err = 0, rc, i;
	int keepalive = 1, keepcnt = 5, keepidle = 30, keepintvl = 120;
	struct sockaddr_in server_addr;
	socklen_t len = sizeof (err);
        puts("\nInside monitor thread\n");
        while(1)
        {
		printf ("\nmonitoring TCP connections.\n");
		monitor_conn(ref_gcfg);
		sleep(3);
		/* Check MsgQ and tally with IDQ to choose best connection to send out the msg below */
        }
        pthread_exit(NULL);
}


void *worker_thread(void *arg) {
	con_t *c = (con_t*)arg;
	char* message;
	int rc = 0,fd = 0, activity = -1, max_fd = 0;
        fd_set read_fds, write_fds, except_fds;

        puts("\nInside worker thread\n");
        while(1)
        {
		build_fd_sets(c->fd, &read_fds, &write_fds, &except_fds);
                int activity = select(max_fd + 1, &read_fds, NULL, &except_fds, NULL);
                switch (activity)
                {
                case -1:
                case 0:
                        perror("\nSelect failed.Exiting.\n");
                        exit(1);
                default:
                        if (FD_ISSET(c->fd, &read_fds))
                        {
                                pbuf = receive_from_fd(pip, &rc);
                                switch(rc)
                                {
                                case 0:
                                        goto fifo;
                                default:
                                        printf("\n Received string: [%s]\n",pbuf);
                                        /* TBD on what to do with the received msg and clean the buffer */
                                }
                        }

                }
                printf ("\nInside manage_thread loop.\n");
        }
        pthread_exit(NULL);
}

int manage(configurator *cfg)
{
	int i, rc, conn, keepalive = 1, keepcnt = 5, keepidle = 30, keepintvl = 120;
	pthread_t manager_t, monit_t, *worker_t, pipe_t;
	struct sockaddr_in server_addr;

	ref_gcfg = cfg;
	gcl = create_peers(ref_gcfg);
	printf("\nTCP connections up\n");
	
	/* Thread to manage queues */
	printf ("\nCreating qmanager thread\n");
	rc = pthread_create(&manager_t, NULL, qmanager_thread, NULL);
	if (rc != 0)
	{
		perror("pthread_create failed\n");
		exit (1);
	}
	pthread_detach(manager_t);

	/* Thread for far right peers communication */
	printf("\nCreating worker threads.\n");
	worker_t = (pthread*)calloc(ref_gcfg->num_peers, sizeof(pthread_t));
	for (i = 0; i < ref_gcfg->num_peers; i++)
	{
		rc = pthread_create(&worker_t[i], NULL, worker_thread, &gcl[i]);
		if (rc != 0)
		{
			perror("pthread_create failed\n");
			exit(1);
		}
		pthread_detach(worker_t[i]);
	}

	/* Thread for monitoring the connections to far right peers */
	printf ("\nCreating monitor thread\n");
        rc = pthread_create(&monit_t, NULL, monitor_thread, NULL);
        if (rc != 0)
        {
                perror("pthread_create failed\n");
                exit (1);
        }
        pthread_detach(monit_t);

	/* Thread for PIPE communication */
	printf ("\nCreating pipe thread\n");
        rc = pthread_create(&pipe_t, NULL, pipe_thread, NULL);
        if (rc != 0)
        {
                perror("pthread_create failed\n");
                exit (1);
        }
        pthread_detach(pipe_t);
	return 0;
}

void *pipe_thread(void *arg)
{
	int rc = 0,pip = 0, activity = -1, max_fd = 0 ;
	fd_set pread_fds, pwrite_fds, pexcept_fds;
	char *pbuf;
	
	puts("\nInside pipe_threads\n");
	/* Create the FIFO if it does not exist */
fifo:	mknod(FIFO, S_IFIFO|0640, 0);
	pip = open(FIFO, O_RDONLY| O_NDELAY);
	setnonblock(pip);
	if (max_fd < pip)
		max_fd = pip;
	while(1)
	{
		build_fd_sets(pip, &pread_fds, &pwrite_fds, &pexcept_fds);
		int activity = select(max_fd + 1, &pread_fds, NULL, &pexcept_fds, NULL);
		switch (activity)
		{
		case -1:
		case 0:
			perror("\nSelect failed.Exiting.\n");
			exit(1);
		default:
			if (FD_ISSET(pip, &pread_fds))
			{
				pbuf = generic_receive_from_fd(pip, &rc);
				switch(rc)
				{
				case 0:
					goto fifo;
				default:
					printf("\n Received string: [%s]\n",pbuf);
					/* TBD adding into  tsidque_t and  msgq_t */
				}
			}
		
		}
		printf ("\nInside manage_thread loop.\n");
		sleep(3);
	}
	pthread_exit(NULL);
}

int build_fd_sets(int fd, fd_set *read_fds, fd_set *write_fds, fd_set *except_fds)
{
	FD_ZERO(read_fds);
	FD_SET(fd, read_fds);
  
	FD_ZERO(write_fds);
	/* Need to check if something is pending to be written to socket */
	FD_SET(fd, write_fds);
 
	FD_ZERO(except_fds);
	FD_SET(fd, except_fds);

	return 0;
}

char *generic_receive_from_fd(int fd, int *ret)
{
	char lenbuf[5], *read_buf;
	int read_bytes = 0, total_read = 0, total_size = 4, received = 0, burst_len = 0;
	int rc = -1;
	ret = &rc;
	read_bytes = read(fd, lenbuf, total_size);
	if (read_bytes == 0) {
		rc = 0;
		return NULL;
	}
	read_buf = (char *)calloc(read_bytes, sizeof(char));
	burst_len = read(fd, read_buf, sizeof(readbuf));
	rc = 1;
	return read_buf;
}



char *receive_from_fd(int fd, int *ret)
{
	int i = 0, read_bytes = 0;
        char *read_buf=NULL, lenbuf[5];
        int total_read = 0, total_size = 4, received = 0, burst_len = 0;
	
	ret = &i;
	memset(lenbuf, 0, 5);
again:  read_bytes = recv(fd, lenbuf+total_read, total_size, MSG_WAITALL);
        if(read_bytes!= total_size)
        {
        	if (read_bytes == 0) {
                	printf("\nPIPE broken, attempting to create/join again.\n");
                        i = 0;
			return NULL;
                } else if (read_bytes < 0) {
                        if(errno == EWOULDBLOCK || errno == EAGAIN|| errno == EINTR) {
                        	goto again;
                        }
                } else {
                        printf("\nReceived bytes:%d, total bytes:%d\n", read_bytes, total_size);
                        total_read+=read_bytes;
                        total_size-=read_bytes;
                        printf("\nReceived bytes:%d, left bytes:%d\n", total_read, total_size);
                        if(errno == EWOULDBLOCK || errno == EAGAIN||errno == EINTR)
                                 goto again;
                }
         }
         total_read+=read_bytes;
         received = atoi(lenbuf);
         printf("\nReceived string: \"%s\" and length is %d\n", lenbuf, received);
         read_bytes = 0;
         read_buf = (char*)calloc(total_read+1, sizeof(char));
         while(read_bytes < received)
         {
         	burst_len = recv(fd, read_buf+read_bytes, received-read_bytes, MSG_WAITALL);
                if (burst_len > 0)
                	read_bytes+=burst_len;
                else if (burst_len == 0) {
                        free(read_buf);
			i = 0;
                        return NULL;
                }
                else {
                        if(errno == EWOULDBLOCK || errno == EAGAIN||errno == EINTR)
                        	continue;
                }
        }
        printf ("\nReceived msg: %s\n", read_buf);
	i = received;
	return read_buf;
}

void *qmanager_thread(void * arg)
{

	printf ("\n Inside qmanager thread\n");

	while(1)
	{
		printf ("\nInside qmanager_thread loop.");
		sleep(3);
	}
	return NULL;
}

static void sighandler(int signal) {
	fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal, strsignal(signal));
	exit (1);
}
