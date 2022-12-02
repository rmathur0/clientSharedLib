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
res_xml_cb res_cb;
req_xml_cb req_cb;

pthread_mutex_t qtex = PTHREAD_MUTEX_INITIALIZER;

/* Signal handler function (defined below). */
void sighandler(int signal);


void *monitor_thread(void *arg) {
        syslog(LOG_INFO,"\nInside monitor thread\n");
        while(1)
        {
		syslog (LOG_INFO,"\nmonitoring TCP connections.\n");
		monitor_sock_conn(ref_gcfg);
		sleep(MONITORING_PERIOD);
        }
        pthread_exit(NULL);
}


void *recv_worker_thread(void *arg) {
	con_t *c = (con_t*)arg;
	char* message = NULL, *out = NULL, *resp = NULL, *tid = NULL, *sid = NULL, *msg_typ = NULL, *typ = NULL;
	int rc = 0, activity = -1, max_fd = 0, out_len = 0, retcode = 0, lookup = 0;
        fd_set read_fds, write_fds, except_fds;
	struct timeval tv;
	long elapsed_msecs;
	request_t *req;
	response_t *res;
	tsidque_t n;

	tv.tv_sec=READ_SEC_TO;
	tv.tv_usec=READ_USEC_TO;

        syslog(LOG_INFO,"Inside worker thread [%d]", c->peer_id);
        while(1)
        {
		tid = NULL; sid = NULL; out = NULL; out_len = 0;

		if (c->state != 1)
			sleep(MONITORING_PERIOD);
		elapsed_msecs = 0;
		build_fd_sets(c->fd, &read_fds, &write_fds, &except_fds);
		if (max_fd < c->fd)
			max_fd = c->fd;
                activity = select(max_fd + 1, &read_fds, NULL, &except_fds, &tv);
		syslog(LOG_INFO,"\nCame out of select systemcall for peer [%d] with state [%d] & connection [%d].\n",c->peer_id, c->state, c->fd);
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
					if (message)
						free(message);
						message = NULL;
					/* It appears connection is lost, sleep for 10 sec and let monitor thread recreate the conection */
                                        /* sleep(MONITORING_PERIOD); */
					break;
                                default:
					parse_xml_attribute(message, rc, "<EVENT>", "</EVENT>", msg_typ, &out_len);
					if (out_len > 0)
					{
						parse_xml_attribute(message, rc, "<TYPE>", "</TYPE>", out, &out_len);
						if (out_len > 0)
						{
							typ = (char*)calloc(out_len+1, sizeof(char));
							memcpy(typ, out, out_len);
							retcode = atoi(typ);
							/* TYPE = 1; EVENT_REQUEST */
							if (retcode == 1)
							{
								out = NULL; out_len = 0;
								parse_xml_attribute(message, rc, "<SID>", "</SID>", out, &out_len);
								sid = (char*)calloc(out_len+1, sizeof(char));
                                                        	memcpy(sid, out, out_len);
                                                        	out = NULL; out_len = 0;
                                                        	parse_xml_attribute(message, rc, "<TID>", "</TID>", out, &out_len);
                                                        	tid = (char*)calloc(out_len+1, sizeof(char));
                                                        	memcpy(sid, out, out_len);
                                                        	pthread_mutex_lock( &qtex );
                                                        	lookup = lookup_ID_idq(&gtsidq, tid, sid, &elapsed_msecs, &n);
                                                        	pthread_mutex_unlock( &qtex );
								if (lookup == 1)
                                                                {
                                                                        req = (request_t*)calloc(1, sizeof(request_t));
                                                                        req->req_buf = message;
                                                                        req->msg_len = rc;
                                                                        req->no_transaction = 0;
                                                                        req->picked_up = 1;
                                                                        req_cb.reg_req_cb(0, req, elapsed_msecs);
                                                                }
                                                                else {
                                                                        /* When timeout, don't call any cb fn */
                                                                        free(message); message = NULL;
                                                                }
								free(typ); free(sid); free(tid);
								typ = NULL; sid = NULL; tid = NULL; out = NULL; out_len = 0;
								continue;
							}
							parse_xml_attribute(message, rc, "<REPLY_CODE>", "</REPLY_CODE>", out, &out_len);
							if (out_len > 0)
							{
					    			resp = (char*)calloc(out_len+1, sizeof(char));
					    			memcpy(resp, out, out_len);
					    			retcode = atoi(resp);
					    			free(resp); resp = NULL;
					    			out = NULL; out_len = 0;
					    			parse_xml_attribute(message, rc, "<SID>", "</SID>", out, &out_len);
					    			sid = (char*)calloc(out_len+1, sizeof(char));
					    			memcpy(sid, out, out_len);
					    			out = NULL; out_len = 0;
					    			parse_xml_attribute(message, rc, "<TID>", "</TID>", out, &out_len);
					    			tid = (char*)calloc(out_len+1, sizeof(char));
                                            			memcpy(sid, out, out_len);
					    			pthread_mutex_lock( &qtex );
					   	 		lookup = lookup_ID_idq(&gtsidq, tid, sid, &elapsed_msecs, &n);
					    			pthread_mutex_unlock( &qtex );
					    			if (lookup == 1)
					    			{
									res = (response_t*)calloc(1, sizeof(response_t));
									strcpy(res->id, tid);
									strcat(res->id, sid);
									res->retcode = retcode;
									res->query_res = message;
									n.callback_f(0, n.callback_param, res, elapsed_msecs); 
					    			}else{
									/* When timeout, don't call any cb fn */
									free(message); message = NULL;
								}
					    			free(tid); free(sid);
					    			tid = NULL; sid = NULL;
					    			out = NULL; out_len = 0;
							}
						}
					}
					else {
						parse_xml_attribute(message, rc, "<TYPE>", "</TYPE>", out, &out_len);
                                                if (out_len > 0)
                                                {
                                                        typ = (char*)calloc(out_len+1, sizeof(char));
                                                        memcpy(typ, out, out_len);
                                                        retcode = atoi(typ);
                                                        /* TYPE = 1; EVENT_REQUEST */
                                                        if (retcode == 1)
                                                        {
								parse_xml_attribute(message, rc, "<SIP_CALLID>", "</SIP_CALLID>", out, &out_len);
								if (out_len > 0)
								{
									resp = (char*)calloc(out_len+1, sizeof(char));
                                                                	memcpy(resp, out, out_len);
                                                                	pthread_mutex_lock( &qtex );
                                                                	lookup = lookup_ID_idq(&gtsidq, resp, NULL, &elapsed_msecs, &n);
                                                               		pthread_mutex_unlock( &qtex );
                                                                	if (lookup == 1)
                                                                	{
                                                                        	req = (request_t*)calloc(1, sizeof(request_t));
                                                                        	req->req_buf = message;
                                                                       		req->msg_len = rc;
                                                                        	req->no_transaction = 0;
                                                                        	req->picked_up = 1;
                                                                        	req_cb.reg_req_cb(0, req, elapsed_msecs);
                                                                	}
									else {
										/* When timeout, don't call any cb fn */
										free(message); message = NULL;
									}
								}
							}
							else
							{
								parse_xml_attribute(message, rc, "<REPLY_CODE>", "</REPLY_CODE>", out, &out_len);
								resp = (char*)calloc(out_len+1, sizeof(char));
                                                                memcpy(resp, out, out_len);
                                                                retcode = atoi(resp);
                                                                free(resp); resp = NULL;
                                                                out = NULL; out_len = 0;
								parse_xml_attribute(message, rc, "<SIP_CALLID>", "</SIP_CALLID>", out, &out_len);
								tid = (char*)calloc(out_len+1, sizeof(char));
								memcpy(tid, out, out_len);
								pthread_mutex_lock( &qtex );
                                                                lookup = lookup_ID_idq(&gtsidq, tid, sid, &elapsed_msecs, &n);
                                                                pthread_mutex_unlock( &qtex );
                                                                if (lookup == 1)
                                                                {
                                                                        res = (response_t*)calloc(1, sizeof(response_t));
                                                                        strcpy(res->id, tid);
                                                                        strcat(res->id, sid);
                                                                        res->retcode = retcode;
                                                                        res->query_res = message;
                                                                        n.callback_f(0, n.callback_param, res, elapsed_msecs);
                                                                }else{
                                                                        /* When timeout, don't call any cb fn */
                                                                        free(message); message = NULL;
                                                                }
                                                                free(tid); free(sid);
                                                                tid = NULL; sid = NULL;
                                                                out = NULL; out_len = 0;
							}
						}
					}
                                }
                        }
			else if (FD_ISSET(c->fd, &except_fds))
			{
				syslog (LOG_INFO,"\nException occurred on peer [%d] & connection [%d].\n",c->peer_id, c->fd);
				close(c->fd);
				c->state = 0;
			}

                }
        }
        pthread_exit(NULL);
}

void *send_worker_thread(void *arg)
{
        /* TODO change pbuf tyoe from char to xml structure */
        con_t *c = (con_t*)arg;
	msgque_t *node;

        syslog(LOG_INFO,"\nInside send_worker_thread\n");
        while(1)
       {
		if (c->state != 1)
                        sleep(MONITORING_PERIOD);

                /* ToDo: Acquire lock on msgque and tsidque */
		pthread_mutex_lock( &qtex );
                node = pop_from_msgq(&gmsgq, c->peer_id);
		pthread_mutex_unlock( &qtex );
                if (node == NULL) {
			sleep(1);
                        continue;
		}
		send_to_fd(c->fd,(char*)&node->len, sizeof(node->len));
                send_to_fd(c->fd, node->data->req_buf, node->len);
		free(node->data->req_buf);
		free(node->data);
		free(node);
        }
        pthread_exit(NULL);
}

int manage(configurator *cfg)
{
	int i, rc, j ;
	pthread_t manager_t, monit_t, *worker_t, pipe_t;

	ref_gcfg = cfg;
	gcl = create_peers(ref_gcfg);
	sleep(1);
	syslog(LOG_INFO,"\nTCP connections up\n");
	
	/* Thread to manage queues */
	syslog (LOG_INFO,"\nCreating qmanager thread\n");
	rc = pthread_create(&manager_t, NULL, qmanager_thread, NULL);
	if (rc != 0)
	{
		perror("pthread_create failed\n");
		exit (1);
	}
	pthread_detach(manager_t);
	sleep(1);
	/* Thread for far right peers communication */
	syslog(LOG_INFO,"\nCreating worker threads.\n");
	worker_t = (pthread_t*)calloc(ref_gcfg->num_peers*2, sizeof(pthread_t));
	for (i = 0, j = 0; i < ref_gcfg->num_peers; i++)
	{
		rc = pthread_create(&worker_t[j], NULL, recv_worker_thread, &gcl[i]);
		if (rc != 0)
		{
			perror("pthread_create failed\n");
			exit(1);
		}
		pthread_detach(worker_t[j]);
		rc = pthread_create(&worker_t[j+1], NULL, send_worker_thread, &gcl[i]);
                if (rc != 0)
                {
                        perror("pthread_create failed\n");
                        exit(1);
                }
                pthread_detach(worker_t[j+1]);
		j+= 2;
	}
	sleep(1);
	/* Thread for monitoring the connections to far right peers */
	syslog (LOG_INFO,"\nCreating monitor thread\n");
        rc = pthread_create(&monit_t, NULL, monitor_thread, NULL);
        if (rc != 0)
        {
                perror("pthread_create failed\n");
                exit (1);
        }
        pthread_detach(monit_t);
	sleep(1);
	/* Thread for PIPE communication *
	syslog (LOG_INFO,"\nCreating pipe thread\n");
        rc = pthread_create(&pipe_t, NULL, rcv_pipe_thread, NULL);
        if (rc != 0)
        {
                perror("pthread_create failed\n");
                exit (1);
        }
        pthread_detach(pipe_t);
	sleep(1);	
	*/
	syslog(LOG_INFO,"\nAll threads are created to function separately\n");
	return 0;
}

void *rcv_pipe_thread(void *arg)
{
	int pip = 0, len = 0 ;
        int read_bytes = 0, total_size = 8;
	long val;
	/* TODO change pbuf tyoe from char to xml structure */
	char pbuf[9];
	char  *tid, *sid;
	request_t *req;
	syslog(LOG_INFO,"\nInside pipe_threads\n");
fifo:	
	pip = open(SL_RCVFIFO, O_RDONLY);
	while(1)
	{
        	memset(pbuf, 0, 9);
        	read_bytes = read(pip, pbuf, total_size);
        	if (read_bytes == 0) {
                	syslog (LOG_INFO,"\nRead failed for Fifo_ingress. Trying to re-open.\n");
			goto fifo;
        	}
        	val = atol(pbuf);
        	req = (request_t *)val;
		/* ToDo: Acquire lock to add element in the msgque_t and tsidque_t 
 		 * ToDo: tid and sid needs to be set inside xml structure
 		 * */
		pthread_mutex_lock( &qtex );
		push_to_msgq(&gmsgq, &gtsidq, req->id, NULL, req->msg_len, req, NULL, NULL);
		pthread_mutex_unlock( &qtex );
	}
	pthread_exit(NULL);
}

int ready_to_send(request_t *req, TransactionCallback_Res_f *callback_f, void *callback_param)
{
	pthread_mutex_lock( &qtex );
	push_to_msgq(&gmsgq, &gtsidq, req->id, NULL, req->msg_len, req, callback_f, callback_param);
	pthread_mutex_unlock( &qtex );
	return 1;
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
			syslog(LOG_INFO,"\nsend() returned 0 bytes. It seems that peer can't accept data right now. Try again later.\n");
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
                	syslog(LOG_INFO,"\nPIPE broken, attempting to create/join again.\n");
			return NULL;
                } else if (read_bytes < 0) {
                        if(errno == EWOULDBLOCK || errno == EAGAIN|| errno == EINTR) {
                        	goto again;
                        }
                } else {
                        syslog(LOG_INFO,"\nReceived bytes:%d, total bytes:%d\n", read_bytes, total_size);
                        total_read+=read_bytes;
                        total_size-=read_bytes;
                        syslog(LOG_INFO,"\nReceived bytes:%d, left bytes:%d\n", total_read, total_size);
                        if(errno == EWOULDBLOCK || errno == EAGAIN||errno == EINTR)
                                 goto again;
                }
         }
         total_read+=read_bytes;
         received = atoi(lenbuf);
         syslog(LOG_INFO,"\nReceived string: \"%s\" and length is %d\n", lenbuf, received);
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
        syslog (LOG_INFO,"\nReceived msg: %s\n", read_buf);
	return read_buf;
}

void *qmanager_thread(void * arg)
{

	syslog (LOG_INFO,"\n Inside qmanager thread\n");

	while(1)
	{
		syslog (LOG_INFO,"\nTimer checking expired TSIDs.\n");
		pthread_mutex_lock( &qtex );
		rem_expired_idq(&gtsidq);
		pthread_mutex_unlock( &qtex );
		sleep(MONITORING_PERIOD*2);
	}
	return NULL;
}

void sighandler(int signal) {
	syslog(LOG_INFO, "Received signal %d: %s.  Shutting down.\n", signal, strsignal(signal));
	exit (1);
}

int register_callback_responses(TransactionCallback_Res_f *callback_f,void *callback_param)
{
	res_cb.reg_res_cb = callback_f;
	res_cb.callback_param = callback_param;
	return 1;
}

int register_callback_requests(TransactionCallback_Req_f *callback_f)
{
	req_cb.reg_req_cb = callback_f;
	return 1;
}
