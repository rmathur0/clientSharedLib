#include "../include/headers.h"
#include "../include/module.h"
#include "../include/task.h"
#include "../include/work.h"
#define FIFO "/tmp/myfifo"

#define BLOCK_SIZE 1024

configurator *ref_gcfg;
con_t *gcl;

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
		for (i = 0; i < ref_gcfg->num_peers; i++) {
			rc = getsockopt (gcl[i].fd, SOL_SOCKET, SO_ERROR, &err, &len);
			if ((rc != 0)||(err != 0)) {
				printf ("\nerror getting getsockopt return code: %s and socket error: %s\n", strerror(rc), strerror(err));
				close(gcl[i].fd);
				gcl[i].state=0;
				gcl[i].fd = socket(AF_INET, SOCK_STREAM, 0);
				bzero((char *) &server_addr, sizeof (server_addr));
				inet_pton(AF_INET, ref_gcfg->peers[i].ip, &(server_addr.sin_addr));
				connect(gcl[i].fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
				setnonblock(gcl[i].fd);
                		setsockopt(gcl[i].fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive , sizeof(keepalive));
                		setsockopt(gcl[i].fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
                		setsockopt(gcl[i].fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
                		setsockopt(gcl[i].fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
                		gcl[i].state = 1;
			}
		}
		sleep(3);
        }
        pthread_exit(NULL);
}


void *worker_thread(void *arg) {
	int client_fd, rc, i;
	char* message = (char*)calloc(BLOCK_SIZE, sizeof(char));

        puts("\nInside worker thread\n");
        while(1)
        {
                printf ("\nInside mworker_thread loop.\n");
                sleep(3);
		/* Check for reading
		 * If available, read
		 * Then check for writing
		 */
        }
        pthread_exit(NULL);
}

int manage(configurator *cfg)
{
	int i, rc, conn, keepalive = 1, keepcnt = 5, keepidle = 30, keepintvl = 120;
	pthread_t monit_t, worker_t;
	struct sockaddr_in server_addr;

	ref_gcfg = cfg;
	/* Number of Peers + 1 fd for PIPE */
	gcl = (con_t*)calloc(ref_gcfg->num_peers+1, sizeof(con_t));

	/* Create TCP connections and store them in a linked list */
	for (i = 0; i < ref_gcfg->num_peers; i++)
	{
		if ((conn = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			perror("\nSocket creation failed. Exiting.\n");
			exit(1);
		}
connect_now:
		bzero((char *) &server_addr, sizeof (server_addr));
		inet_pton(AF_INET, ref_gcfg->peers[i].ip, &(server_addr.sin_addr));
		server_addr.sin_port = htons(ref_gcfg->peers[i].port);
		rc = connect(conn, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if (rc < 0) {
			perror("\nConnect failed. Retrying in 5 seconds.\n");
			sleep(5);
			goto connect_now;
		}
		setnonblock(conn);
		setsockopt(conn, SOL_SOCKET, SO_KEEPALIVE, &keepalive , sizeof(keepalive));
		setsockopt(conn, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
		setsockopt(conn, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
		setsockopt(conn, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
		gcl[i].fd = conn;
		gcl[i].state = 1;
	}
	printf("\nTCP connections up\n");
	
	printf ("\nCreating manager thread\n");
	rc = pthread_create(&monit_t, NULL, manager_thread, NULL);
	if (rc != 0)
	{
		perror("pthread_create failed\n");
		exit (1);
	}
	pthread_detach(monit_t);
	for (i = 0; i < ref_gcfg->num_peers; i++)
	{
		rc = pthread_create(&worker_t, NULL, worker_thread, NULL);
		if (rc != 0)
		{
			perror("pthread_create failed\n");
			exit(1);
		}
		pthread_detach(worker_t);
	}
	printf ("\nCreating monitor thread\n");
        rc = pthread_create(&monit_t, NULL, monitor_thread, NULL);
        if (rc != 0)
        {
                perror("pthread_create failed\n");
                exit (1);
        }
        pthread_detach(monit_t);
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
	gcl[ref_gcfg->num_peers].fd = pip;
	gcl[ref_gcfg->num_peers].state = 1;
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

void *workers(void * arg)
{

	return NULL;
}

static void sighandler(int signal) {
	fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal, strsignal(signal));
	exit (1);
}
