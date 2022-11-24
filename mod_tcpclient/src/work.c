#include "../include/headers.h"
#include "../include/module.h"
#include "../include/task.h"
#include "../include/work.h"

#define BLOCK_SIZE 1024

/* Common data */
configurator *ref_gcfg;
con_t *gcl;
msgque_t *gmsgq;
tsidque_t *gtsidq;
refque_t *grefq;

/* Signal handler function (defined below). */
void sighandler(int signal);

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
	int rc = 0, activity = -1, max_fd = 0;
	long addr;
        fd_set read_fds, write_fds, except_fds;
	struct timeval tv;
	msgque_t *node;

	tv.tv_sec=READ_SEC_TO;
	tv.tv_usec=READ_USEC_TO;

        puts("\nInside worker thread\n");
        while(1)
        {
		build_fd_sets(c->fd, &read_fds, &write_fds, &except_fds);
		if (max_fd < c->fd)
			max_fd = c->fd;
                activity = select(max_fd + 1, &read_fds, NULL, &except_fds, &tv);
		printf("\nCame out of select systemcall for peer [%d] & connection [%d].\n",c->peer_id, c->fd);
                switch (activity)
                {
                case -1:
                case 0:
                        perror("\nSelect failed.Exiting.\n");
                        exit(1);
                default:
                        if (FD_ISSET(c->fd, &read_fds))
                        {
                                message = receive_from_fd(c->fd, &rc);
                                switch(rc)
                                {
                                case 0:
					/* It appears connection is lost, sleep for 10 sec and let monitor thread recreate the conection */
                                        sleep(10);
                                default:
                                        printf("\n Received string: [%s]\n",message);
					addr = message;
					/* ToDo: Acquire lock on refque */
					push_to_refq(&grefq, addr);
                                }
                        }
			else if (FD_ISSET(c->fd, &except_fds))
			{
				printf ("\nException occurred on peer [%d] & connection [%d].\n",c->peer_id, c->fd);
				close(c->fd);
				c->state = 0;
			}

                }
		/* ToDo: Acquire lock on msgque and tsidque */
		node = pop_from_msgq(&gmsgq, c->peer_id);
		rc = send_to_fd(c->fd, node->data, node->len);
		/* rc contains total bytes written on wire */

                printf ("\nInside manage_thread loop.\n");
        }
        pthread_exit(NULL);
}

int manage(configurator *cfg)
{
	int i, rc ;
	pthread_t manager_t, monit_t, *worker_t, pipe_t[2];

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
	worker_t = (pthread_t*)calloc(ref_gcfg->num_peers, sizeof(pthread_t));
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
        rc = pthread_create(&pipe_t[0], NULL, rcv_pipe_thread, NULL);
        if (rc != 0)
        {
                perror("pthread_create failed\n");
                exit (1);
        }
        pthread_detach(pipe_t[0]);
        rc = pthread_create(&pipe_t[1], NULL, snd_pipe_thread, NULL);
        if (rc != 0)
        {       
                perror("pthread_create failed\n");
                exit (1);
        }
        pthread_detach(pipe_t[1]);
	
	printf("\nAll threads are created to function separately\n");
	return 0;
}

void *rcv_pipe_thread(void *arg)
{
	int rc = 0, pip = 0, len = 0 ;
        int read_bytes = 0, total_size = 8;
	long val;
	/* TODO change pbuf tyoe from char to xml structure */
	char pbuf[9];
	char *databuf, *tid, *sid;
	puts("\nInside pipe_threads\n");
fifo:	
	pip = open(SL_RCVFIFO, O_RDONLY);
	while(1)
	{
        	memset(pbuf, 0, 9);
        	read_bytes = read(pip, pbuf, total_size);
        	if (read_bytes == 0) {
                	rc = 0;
                	printf ("\nRead failed for Fifo_ingress. Trying to re-open.\n");
			goto fifo;
        	}
        	val = atol(pbuf);
        	databuf = (char *)val;
		/* ToDo: Acquire lock to add element in the msgque_t and tsidque_t 
 		 * ToDo: tid and sid needs to be set inside xml structure
 		 * */
		len = strlen(databuf);
		push_to_msgq(&gmsgq, &gtsidq, tid, sid, len, databuf);
	}
	pthread_exit(NULL);
}

void *snd_pipe_thread(void *arg)
{
        int rc = 0,pip = 0, activity = -1 ;
        /* TODO change pbuf tyoe from char to xml structure */
        long val;

        puts("\nInside pipe_threads\n");
fifo:
        pip = open(SL_SNDFIFO, O_WRONLY);
        while(1)
        {
		/* ToDo: Acquire lock with condition that there is atleast one element in refque */
		val = pop_from_refq(&grefq);
		write(pip, &val, sizeof(val));
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

void *generic_receive_from_fd(int fd, int *ret)
{
	char pbuf[9];
	void *read_buf;
	int read_bytes = 0, total_size = 8;
	long val;
	int rc = -1;
	ret = &rc;
	memset(pbuf, 0, 9);
	read_bytes = read(fd, pbuf, total_size);
	if (read_bytes == 0) {
		rc = 0;
		return NULL;
	}
	rc = 1;
	val = atol(pbuf);
	read_buf = val;
	return read_buf;
}

int send_to_fd(int fd, char *buf, int len)
{
	int sent = 0, total_sent = 0;

	while(total_sent < len)
	{
		sent = send(fd, buf+total_sent, len-total_sent, MSG_DONTWAIT|MSG_NOSIGNAL);
		if (sent < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			else
				return -1;
		} else if (sent == 0)
		{
			printf("\nsend() returned 0 bytes. It seems that peer can't accept data right now. Try again later.\n");
			return -1;
		}
		total_sent += sent;
	}
	return total_sent;
}

char *receive_from_fd(int fd, int *ret)
{
	int read_bytes = 0;
        char *read_buf=NULL, lenbuf[5];
        int total_read = 0, total_size = 4, received = 0, burst_len = 0;
	
	ret = &read_bytes;
	memset(lenbuf, 0, 5);
again:  read_bytes = recv(fd, lenbuf+total_read, total_size, MSG_WAITALL);
        if(read_bytes!= total_size)
        {
        	if (read_bytes == 0) {
                	printf("\nPIPE broken, attempting to create/join again.\n");
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
                        return NULL;
                }
                else {
                        if(errno == EWOULDBLOCK || errno == EAGAIN||errno == EINTR)
                        	continue;
                }
        }
        printf ("\nReceived msg: %s\n", read_buf);
	return read_buf;
}

void *qmanager_thread(void * arg)
{

	printf ("\n Inside qmanager thread\n");

	while(1)
	{
		printf ("\nTimer checking expired TSIDs.\n");
		/* ToDo: Acquire lock on refque */
		rem_expired_idq(&gtsidq);
		sleep(3);
	}
	return NULL;
}

void sighandler(int signal) {
	fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal, strsignal(signal));
	exit (1);
}
