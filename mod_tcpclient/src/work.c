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
pthread_mutex_t conntex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condition_cond  = PTHREAD_COND_INITIALIZER;

/* Signal handler function (defined below). */
void sighandler(int signal);


void *monitor_thread(void *arg) {
        syslog(LOG_INFO,"RM: Inside monitor thread\n");
        while(1)
        {
		pthread_mutex_lock( &conntex );
		monitor_sock_conn(ref_gcfg);
		pthread_mutex_unlock( &conntex );
		sleep(MONITORING_PERIOD);
        }
        pthread_exit(NULL);
}


void *recv_worker_thread(void *arg) {
	con_t *c = (con_t*)arg;
	char* message = NULL, *out = NULL, *resp = NULL, *tid = NULL, *msg_typ = NULL, *typ = NULL;
	int rc = 0, activity = -1, max_fd = 0, out_len = 0, retcode = 0, lookup = 0;
        fd_set read_fds, write_fds, except_fds;
	struct timeval tv;
	long elapsed_msecs;
	request_t *req;
	response_t *res;
	tsidque_t n;
	char is_sid_present = 0;

	tv.tv_sec=READ_SEC_TO;
	tv.tv_usec=READ_USEC_TO;

        syslog(LOG_INFO,"RM: Inside worker thread [%d]", c->peer_id);
        while(1)
        {
		tid = NULL; is_sid_present = 0; out = NULL; out_len = 0; req = NULL; res = NULL;
reconn:
		while (c->state != 1)
		{
			pthread_mutex_lock( &conntex );
                	recreate_conn(c->peer_id, ref_gcfg);
                	pthread_mutex_unlock( &conntex );
			if (c->state != 1)
				sleep(1);
		}
		elapsed_msecs = 0;
                message = receive_from_fd(c->fd, &rc);
                switch(rc)
                {
                        case 0:
				close(c->fd);
				c->state = 0;
				if (message)
					free(message);
				message = NULL;
				/* It appears connection is lost, recreate the conection */
                        	goto reconn;
                        default:
				/* EVENT based message received */
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
                                                       	parse_xml_attribute(message, rc, "<TID>", "</TID>", out, &out_len);
                                                       	tid = (char*)calloc(out_len+1, sizeof(char));
                                                       	memcpy(tid, out, out_len);
                                                       	pthread_mutex_lock( &qtex );
                                                       	lookup = lookup_ID_idq(&gtsidq, tid, &elapsed_msecs, &n);
                                                       	pthread_mutex_unlock( &qtex );
							if (lookup == 1)
                                                               {
                                                                       req = (request_t*)calloc(1, sizeof(request_t));
                                                                       req->req_buf = message;
                                                                       req->msg_len = rc;
                                                                       req->no_transaction = 1;
                                                                       req->picked_up = 1;
                                                                       req_cb.reg_req_cb(1, req, elapsed_msecs);
                                                               }
                                                               else {
                                                                       req = (request_t*)calloc(1, sizeof(request_t));
                                                                       req->req_buf = message;
                                                                       req->msg_len = rc;
                                                                       req->no_transaction = 0;
                                                                       req->picked_up = 0;
                                                                       req_cb.reg_req_cb(0, req, elapsed_msecs);
                                                               }
							if(typ) free(typ); if(tid) free(tid);
							typ = NULL; tid = NULL; out = NULL; out_len = 0;
						}
						else 
						{
						/* EVENT RESPONSE */
							parse_xml_attribute(message, rc, "<REPLY_CODE>", "</REPLY_CODE>", out, &out_len);
				    			resp = (char*)calloc(out_len+1, sizeof(char));
				    			memcpy(resp, out, out_len);
				    			retcode = atoi(resp);
				    			free(resp); resp = NULL;
				    			out = NULL; out_len = 0;
				    			parse_xml_attribute(message, rc, "<TID>", "</TID>", out, &out_len);
				    			tid = (char*)calloc(out_len+1, sizeof(char));
                                           		memcpy(tid, out, out_len);
				    			pthread_mutex_lock( &qtex );
				   	 		lookup = lookup_ID_idq(&gtsidq, tid, &elapsed_msecs, &n);
				    			pthread_mutex_unlock( &qtex );
							res = (response_t*)calloc(1, sizeof(response_t));
                                                        strcpy(res->id, tid);
                                                        res->retcode = retcode;
                                                        res->query_res = message;
				    			if (lookup == 1)
								n.callback_f(0, n.callback_param, res, elapsed_msecs); 
							else
								n.callback_f(1, n.callback_param, res, elapsed_msecs);
				    			free(tid); 
				    			tid = NULL; 
				    			out = NULL; out_len = 0;
						}
					}
					else {
						free(message); message = NULL;
					}
				}
				else {
					/* SESSION based message received */
					parse_xml_attribute(message, rc, "<TYPE>", "</TYPE>", out, &out_len);
                                        if (out_len > 0)
                                        {
                                                typ = (char*)calloc(out_len+1, sizeof(char));
                                                memcpy(typ, out, out_len);
                                                retcode = atoi(typ);
                                                /* TYPE = 1; SESSION_REQUEST */
						out = NULL; out_len = 0;
                                                if (retcode == 1)
                                                {
							parse_xml_attribute(message, rc, "<SID>", "</SID>", out, &out_len);
							if (out_len > 0)
								is_sid_present = 1;
							out = NULL; out_len = 0;
							parse_xml_attribute(message, rc, "<TID>", "</TID>", out, &out_len);
                                                        tid = (char*)calloc(out_len+1, sizeof(char));
							memcpy(tid, out, out_len);
                                                        if (is_sid_present == 1)
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
						else
						{
							/* SESSION RESPONSE */
							parse_xml_attribute(message, rc, "<REPLY_CODE>", "</REPLY_CODE>", out, &out_len);
							resp = (char*)calloc(out_len+1, sizeof(char));
                                                	memcpy(resp, out, out_len);
                                                	retcode = atoi(resp);
                                                	free(resp); resp = NULL;
                                                	out = NULL; out_len = 0;
							parse_xml_attribute(message, rc, "<SID>", "</SID>", out, &out_len);
							if (out_len > 0)
								is_sid_present = 1;
                                                        out = NULL; out_len = 0;
                                                        parse_xml_attribute(message, rc, "<TID>", "</TID>", out, &out_len);
							tid = (char*)calloc(out_len+1, sizeof(char));
							memcpy(tid, out, out_len);
							if (is_sid_present == 1)
							{
								req = (request_t*)calloc(1, sizeof(request_t));
                                                                req->req_buf = message;
                                                                req->msg_len = rc;
                                                                req->no_transaction = 0;
                                                                req->picked_up = 1;
                                                                req_cb.reg_req_cb(0, req, elapsed_msecs);
							}
							else
							{
								pthread_mutex_lock( &qtex );
                                                		lookup = lookup_ID_idq(&gtsidq, tid, &elapsed_msecs, &n);
                                                		pthread_mutex_unlock( &qtex );
								res = (response_t*)calloc(1, sizeof(response_t));
                                                        	strcpy(res->id, tid);
                                                        	res->retcode = retcode;
                                                        	res->query_res = message;
                                                		if (lookup == 1)
                                                        		n.callback_f(0, n.callback_param, res, elapsed_msecs);
                                                		else
									n.callback_f(1, n.callback_param, res, elapsed_msecs);
                                                		free(tid); 
                                                		tid = NULL; 
                                                		out = NULL; out_len = 0;
							}
						}
					}
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
	char msglen[5];
        syslog(LOG_INFO,"RM: Inside send_worker_thread\n");
        while(1)
       {
		pthread_mutex_lock( &condition_mutex );
		while(gmsgq == NULL)
		{
			pthread_cond_wait( &condition_cond, &condition_mutex );
		}
		pthread_mutex_unlock( &condition_mutex );

		/* In hopes that receive_thread has reconnected */
		if (c->state != 1)
                        sleep(READ_SEC_TO);

                /* ToDo: Acquire lock on msgque and tsidque */
		pthread_mutex_lock( &qtex );
                node = pop_from_msgq(&gmsgq, c->peer_id);
		pthread_mutex_unlock( &qtex );
                if (node == NULL) {
			sleep(1);
                        continue;
		}
		memset(msglen, 0, 5);
		sprintf(msglen,"%04d", node->len);
		send_to_fd(c->fd, msglen, strlen(msglen));
		syslog(LOG_INFO,"RM: Sending [%s] to kamailio",node->data->req_buf);
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
	syslog(LOG_INFO,"\nRM: TCP connections up\n");
	
	/* Thread to manage queues */
	syslog (LOG_INFO,"\nRM: Creating qmanager thread\n");
	rc = pthread_create(&manager_t, NULL, qmanager_thread, NULL);
	if (rc != 0)
	{
		perror("pthread_create failed\n");
		exit (1);
	}
	pthread_detach(manager_t);
	sleep(1);
	/* Thread for far right peers communication */
	syslog(LOG_INFO,"\nRM: Creating worker threads.\n");
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
	syslog (LOG_INFO,"\nRM: Creating monitor thread\n");
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
	syslog(LOG_INFO,"\nRM: All threads are created to function separately\n");
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
		push_to_msgq(&gmsgq, &gtsidq, req->id, req->msg_len, req, NULL, NULL);
		pthread_mutex_unlock( &qtex );
	}
	pthread_exit(NULL);
}

int ready_to_send(request_t *req, TransactionCallback_Res_f *callback_f, void *callback_param)
{
	
	pthread_mutex_lock( &qtex );
	push_to_msgq(&gmsgq, &gtsidq, req->id, req->msg_len, req, callback_f, callback_param);
	pthread_mutex_unlock( &qtex );
	pthread_mutex_lock( &condition_mutex );
	if (gmsgq->next == NULL)
		pthread_cond_signal( &condition_cond );
	pthread_mutex_unlock( &condition_mutex );
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
			syslog(LOG_INFO,"RM: send() returned %d bytes. errno=%d\n", sent, errno);
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			else
				return -1;
		} else if (sent == 0)
		{
			syslog(LOG_INFO,"RM: send() returned 0 bytes. It seems that peer can't accept data right now. Try again later.\n");
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
                	syslog(LOG_INFO,"RM: Socket broken, attempting to create/join again.\n");
			return NULL;
                } else if (read_bytes < 0) {
			syslog(LOG_INFO,"RM: recv() returned %d bytes. errno=%d\n", read_bytes, errno);
                        if(errno == EWOULDBLOCK || errno == EAGAIN|| errno == EINTR) {
                        	goto again;
                        }
                } else {
                        syslog(LOG_INFO,"RM: Received bytes:%d, total bytes:%d\n", read_bytes, total_size);
                        total_read+=read_bytes;
                        total_size-=read_bytes;
                        syslog(LOG_INFO,"RM: Received bytes:%d, left bytes:%d\n", total_read, total_size);
                        if(errno == EWOULDBLOCK || errno == EAGAIN||errno == EINTR)
                                 goto again;
                }
         }
         total_read+=read_bytes;
         received = atoi(lenbuf);
         syslog(LOG_INFO,"\nRM: Received string: \"%s\" and length is %d\n", lenbuf, received);
         read_bytes = 0;
         read_buf = (char*)calloc(total_read+1, sizeof(char));
         while(read_bytes < received)
         {
         	burst_len = recv(fd, read_buf+read_bytes, received-read_bytes, MSG_WAITALL);
                if (burst_len > 0)
                	read_bytes+=burst_len;
                else if (burst_len == 0) {
 			syslog(LOG_INFO,"RM: Socket broken, attempting to create/join again.\n");
                        free(read_buf);
                        return NULL;
                }
                else {
			syslog(LOG_INFO,"RM: recv() returned %d bytes. errno=%d\n", read_bytes, errno);
                        if(errno == EWOULDBLOCK || errno == EAGAIN||errno == EINTR)
                        	continue;
                }
        }
        syslog (LOG_INFO,"\nRM: Received msg: %s\n", read_buf);
	return read_buf;
}

void *qmanager_thread(void * arg)
{

	syslog (LOG_INFO,"\nRM:  Inside qmanager thread\n");

	while(1)
	{
		syslog (LOG_INFO,"\nRM: Timer checking expired TSIDs.\n");
		pthread_mutex_lock( &qtex );
		rem_expired_idq(&gtsidq);
		pthread_mutex_unlock( &qtex );
		sleep(MONITORING_PERIOD*2);
	}
	return NULL;
}

void sighandler(int signal) {
	syslog(LOG_INFO, "RM: Received signal %d: %s.  Shutting down.\n", signal, strsignal(signal));
	exit (1);
}

int register_callback_responses(TransactionCallback_Res_f *callback_f,void *callback_param)
{
	res_cb.reg_res_cb = callback_f;
	res_cb.callback_param = callback_param;
	return 1;
}

int register_callback_request(TransactionCallback_Req_f *callback_f)
{
	req_cb.reg_req_cb = callback_f;
	return 1;
}
