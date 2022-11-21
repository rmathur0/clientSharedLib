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
	struct sockaddr_in sin;
	socklen_t len = sizeof (err);
        puts("\nInside monitor thread\n");
        while(1)
        {
		printf ("\nmonitoring TCP connections.\n");
		for (i = 0; i < ref_gcfg->num_peers; i++) {
			rc = getsockopt (gcl[i].fd, SOL_SOCKET, SO_ERROR, &err, &len);
			if ((rc != 0)||(err != 0)) {
				printf ("\nerror getting getsockopt return code: %s and socket error: %s\n", strerror(retval), strerror(err));
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
                		gcl[i].fd = conn;
                		gcl[i].state = 1;
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

void *manager_thread(void *arg)
{
	int i = 0,pip, read_bytes;
	char readbuf[BLOCK_SIZE];
	puts("\nInside manage_threads\n");
fifo:
	/* Create the FIFO if it does not exist */
	mknod(FIFO, S_IFIFO|0640, 0);
	pip = open(FIFO, O_RDONLY| O_NDELAY);
	/* Setting it to non-blocking so that broken PIPE can be checked and recreated */
	//setnonblock(pip);
	gcl[ref_gcfg->num_peers].fd = pip;
	gcl[ref_gcfg->num_peers].state = 1;
	while(1)
	{
		read_bytes = read(pip, readbuf, sizeof(readbuf));
      		if (read_bytes == 0) {
			printf("\nPIPE broken, attempting to create/join again.\n");
			goto fifo;
		}
		readbuf[read_bytes] = '\0';
      		printf("Received string: \"%s\" and length is %d\n", readbuf, (int)strlen(readbuf));
		printf ("\nInside manage_thread loop.\n");
		sleep(3);
	}
	pthread_exit(NULL);
}

void *workers(void * arg)
{

	return NULL;
}

static void sighandler(int signal) {
	fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal, strsignal(signal));
	exit (1);
}