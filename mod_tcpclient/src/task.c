#include "../include/headers.h"
#include "../include/module.h"
#include "../include/task.h"

con_t *gconn_list;
static int glast_con_rr;

/**
 *  *  * Set a socket to non-blocking mode.
 *   *   */
static int setnonblock(int fd) {
        int flags;

        flags = fcntl(fd, F_GETFL);
        if (flags < 0) return flags;
        flags |= O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) < 0) return -1;
        return 0;
}

/* Create peers in con_t */
con_t *create_peers(configurator *cfg)
{
	int i = 0, rc = -1, retry = 0, conn;
	int keepalive = 1, keepcnt = 5, keepidle = 30, keepintvl = 120;
	struct addrinfo hints, *res;
	char sndbuf[5];

	syslog(LOG_INFO, "RM: Creating peer connections");
	gconn_list = (con_t*)calloc(cfg->num_peers, sizeof(con_t));
	memset(sndbuf, 0, 5);
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        /* Create TCP connections and store them */
        for (i = 0; i < cfg->num_peers; i++)
        {
		retry = 1;
		sprintf(sndbuf, "%d", cfg->peers[i].port);
		syslog(LOG_INFO,"RM: Creating socket connection to %s:%s",cfg->peers[i].ip, sndbuf);
		getaddrinfo(cfg->peers[i].ip, sndbuf, &hints, &res);
		conn = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
connect_now:            if (conn < 0) {
                        perror("\nSocket creation failed. Exiting.\n");
                        exit(1);
                }
                rc = connect(conn, res->ai_addr, res->ai_addrlen);
		syslog(LOG_INFO,"RM: connect returned [%d] \n",rc);
                if (rc < 0) {
                        syslog (LOG_INFO,"RM: Connect failed for [%s:%d] \n", cfg->peers[i].ip, cfg->peers[i].port);
                        if (retry == 1) {
				perror("RM: Retrying in 5 seconds.\n");
				close(conn);
				retry--;
				sleep(5);
                        	goto connect_now;
			}
			continue;
                }
                //setnonblock(conn);
                setsockopt(conn, SOL_SOCKET, SO_KEEPALIVE, &keepalive , sizeof(keepalive));
                setsockopt(conn, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
                setsockopt(conn, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
                setsockopt(conn, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
                gconn_list[i].fd = conn;
                gconn_list[i].state = 1;
                gconn_list[i].peer_id = i;
        }
	syslog(LOG_INFO,"RM: Leaving create_peers");
	return gconn_list;
}

void monitor_sock_conn(configurator *cfg)
{
	int i = 0, rc = -1, err = 0;
        int keepalive = 1, keepcnt = 5, keepidle = 30, keepintvl = 120;
        struct addrinfo hints, *res;
        char sndbuf[5];
	socklen_t len = sizeof (err);

	syslog (LOG_INFO,"RM: monitoring TCP connections.\n");
	memset(sndbuf, 0, 5);
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        for (i = 0; i < cfg->num_peers; i++) {
	       	rc = getsockopt (gconn_list[i].fd, SOL_SOCKET, SO_ERROR, &err, &len);
        	if ((rc != 0)||(err != 0)) {
        		//syslog (LOG_INFO,"RM: error getting getsockopt return code: %s and socket error: %s\n", strerror(rc), strerror(err));
        		close(gconn_list[i].fd);
        		gconn_list[i].state=0;
			sprintf(sndbuf, "%d", cfg->peers[i].port);
			getaddrinfo(cfg->peers[i].ip, sndbuf, &hints, &res);
 			gconn_list[i].fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
			rc = connect(gconn_list[i].fd, res->ai_addr, res->ai_addrlen);
        		//setnonblock(gconn_list[i].fd);
        		if (rc < 0)
				continue;
        		setsockopt(gconn_list[i].fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive , sizeof(keepalive));
        		setsockopt(gconn_list[i].fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
        		setsockopt(gconn_list[i].fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
        		setsockopt(gconn_list[i].fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
        		gconn_list[i].state = 1;
        	}
        }
}

void recreate_conn(int pid, configurator *cfg)
{
        int rc = -1, err = 0; 
        int keepalive = 1, keepcnt = 5, keepidle = 30, keepintvl = 120;
        struct addrinfo hints, *res;
        char sndbuf[5]; 
        
        syslog (LOG_INFO,"RM: Trying to recreate TCP connection..\n");
        memset(sndbuf, 0, 5);
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
	sprintf(sndbuf, "%d", cfg->peers[pid].port);
	getaddrinfo(cfg->peers[pid].ip, sndbuf, &hints, &res);
	gconn_list[pid].state = 0;
	gconn_list[pid].fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	rc = connect(gconn_list[pid].fd, res->ai_addr, res->ai_addrlen);
        syslog(LOG_INFO,"RM: Upon recreation, connect returned [%d] \n",rc);
        if (rc < 0) {
        	syslog (LOG_INFO,"RM: Connect failed again for [%s:%d] return code:[%d] & socket error:[%s]\n", cfg->peers[pid].ip, cfg->peers[pid].port, rc, strerror(errno));
		return;
        }
	setsockopt(gconn_list[pid].fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive , sizeof(keepalive));
        setsockopt(gconn_list[pid].fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
        setsockopt(gconn_list[pid].fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
        setsockopt(gconn_list[pid].fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
        gconn_list[pid].state = 1;
}

/* Check if provided ID (TID+SID) is present in the connection Q */
int is_ID_present_idq(tsidque_t **head, char *tid, int *conn)
{
	tsidque_t *temp = *head;
	char *temp_id;

	if (*head == NULL)
		return 0;
	if (tid && *tid) 
	{
		temp_id = (char*)calloc((strlen(tid) + 1), sizeof(char));
                strcpy(temp_id, tid);
		while(temp != NULL)
		{	
			if ( strcmp(temp_id, temp->id) == 0)
			{
				*conn = temp->conn;
				free(temp_id);
				return 1;
			}
			temp = temp->next;
		}
	}
	return 0;
}

int lookup_ID_idq(tsidque_t **head, char *tid, long *elapsed_msecs, tsidque_t *node)
{
        tsidque_t *temp = *head;
        char *temp_id;
	long time_elapsed = 0;
	struct timeval curr;
        *elapsed_msecs = time_elapsed;

        if (*head == NULL)
                return 0;
        if (tid && *tid)
        {
                temp_id = (char*)calloc((strlen(tid) + 1), sizeof(char));
                strcpy(temp_id, tid);
        	while(temp != NULL)
        	{
			syslog(LOG_INFO,"RM: Checking the element, %s:%s",temp_id, temp->id);
                	if ( strcmp(temp_id, temp->id) == 0)
                	{
                	        gettimeofday(&curr, NULL);
				time_elapsed = (curr.tv_sec - temp->ATime.tv_sec)*1000 + (curr.tv_usec - temp->ATime.tv_usec)/1000;
				*elapsed_msecs = time_elapsed;
				node->id = temp->id;
				node->is_expired = temp->is_expired;
				node->callback_f = temp->callback_f;
				node->callback_param = temp->callback_param;
                        	free(temp_id);
                        	return 1;
                	}
                	temp = temp->next;
        	}
	}
        return 0;

}


int update_entry_idq(tsidque_t **head, char *id, TransactionCallback_Res_f *callback_f, void *callback_param)
{
	tsidque_t *temp = *head;

	if (*head == NULL)
		return 0;
	while(temp != NULL)
	{
		if ( strcmp(id, temp->id) == 0)
		{
			gettimeofday(&temp->ATime, NULL);
			temp->ETime = temp->ATime;
			temp->ETime.tv_sec+= EXPIRY;
			temp->callback_f = callback_f;
			temp->callback_param = callback_param;
			return temp->conn;
		}
		temp = temp->next;
	}
	return -1;
}

int add_entry_idq(tsidque_t **head, char *tid, TransactionCallback_Res_f *callback_f, void *callback_param)
{
	tsidque_t *temp = (tsidque_t*)calloc(1, sizeof(tsidque_t));
	char *temp_id;
	tsidque_t *trav = *head;
	int num_peers = -1, con = -1;
	temp->prev = temp->next = NULL;

	if (tid && *tid)
        {
                temp_id = (char*)calloc(strlen(tid)+1, sizeof(char));
		strcpy(temp_id, tid);
        }
	else
	{
		return -1;
	}
	temp->id = temp_id;
	gettimeofday(&temp->ATime, NULL);
	temp->ETime = temp->ATime;
	temp->ETime.tv_sec+= EXPIRY;
	temp->is_expired = 0;
	temp->next = NULL;
	temp->callback_f = callback_f;
        temp->callback_param = callback_param;	
	num_peers = get_peers();
assign_conn:	num_peers = get_peers();
reassign_conn:	con = glast_con_rr % num_peers;
		if (gconn_list[con].state == 1)
			temp->conn = con;
		else {
			glast_con_rr++;
			if (glast_con_rr == num_peers)
				glast_con_rr = 0;
			goto reassign_conn;
		}
		glast_con_rr++;
		if (glast_con_rr == num_peers)
			glast_con_rr = 0;
	if (trav == NULL)
	{
		*head = temp;
		temp->prev = NULL;
		return con;
	}
	while(trav->next != NULL)
		trav = trav->next;
	trav->next = temp;
	temp->prev = trav;
	return con;
}

void list_expired_idq(tsidque_t **head)
{
	tsidque_t *temp = NULL;
	struct timeval now;
	tsidque_t *current = *head;

	if (current == NULL)
		return;

        while(current != NULL)
        {       
		gettimeofday(&now, NULL);
        	if ( (current->ETime.tv_sec < now.tv_sec) || ((current->ETime.tv_sec == now.tv_sec) && (current->ETime.tv_usec < now.tv_usec)) )
        	{       
			syslog(LOG_INFO, "RM: Elem expired.. ID:%s, Callback_param:%s", current->id, current->callback_param);
			current->is_expired = 1;
        	}
        	current = current->next;
        }
}


void rem_id(tsidque_t **head, char *tid)
{
        tsidque_t *temp = NULL;
        struct timeval now;
        tsidque_t *current = *head;

        if (current == NULL)
                return;
	if (strcmp(current->id, tid) == 0)
	{
		*head = (*head)->next;
		free(current->id);
		free(current);
	}
	else
	{
        	while(current->next != NULL)
        	{
			syslog(LOG_INFO,"RM: Checking %s with %s", tid, current->next->id);
                	if (strcmp(current->next->id, tid) == 0)
			{
				syslog(LOG_INFO,"RM: Removing %s", tid);
				temp = current->next;
				current->next = current->next->next;
				free(temp->id);
				free(temp);
				break;
			}
			else
                		current = current->next;
        	}
	}
}
/* Add element in msgque_t */
void push_to_msgq(msgque_t **msghead, tsidque_t **idhead, char *tid, int len, request_t *data, TransactionCallback_Res_f *callback_f, void *callback_param)
{
	msgque_t *trav = *msghead;
	msgque_t *temp = (msgque_t*)calloc(1, sizeof(msgque_t));
	char *temp_id;
        int rc = -1, ret = -1, con = -1;
	temp->prev = temp->next = NULL;

        if (tid && *tid)
        {
                temp_id = (char*)calloc(strlen(tid)+1, sizeof(char));
                strcpy(temp_id, tid);
		syslog(LOG_INFO,"RM: push_to_msgq id:%s", temp_id);
        }
	else
		return;
	temp->data = data;
	temp->len = len;
	strcpy(temp->id, temp_id);
	temp->conn = con;
	rc = is_ID_present_idq(idhead, temp_id, &temp->conn);
	if (rc == 1)
	{
		ret = update_entry_idq(idhead, temp_id, callback_f, callback_param);
		if (ret == -1)
			goto add;
	}
	else
add:		temp->conn = add_entry_idq(idhead, temp_id, callback_f, callback_param);
	
	if (trav == NULL)
        {
                *msghead = temp;
                temp->prev = NULL;
                return;
        }
        while(trav->next != NULL)
                trav = trav->next;
        trav->next = temp;
        temp->prev = trav;
}

/* Pop element from msgque_t for a connection*/
msgque_t *pop_from_msgq(msgque_t **head, int con)
{
	msgque_t *temp = *head;
	msgque_t *node = NULL;

	if (temp == NULL)
		return NULL;
	if (temp->conn == con) {
		*head = (*head)->next;
		if (*head)
			(*head)->prev = NULL;
		temp->next = temp->prev = NULL;
		return temp;
	}
	else
	{
		while(temp != NULL)
		{
			if (temp->conn == con)
			{
				node = temp;
				if (temp->next){
					temp->next->prev = temp->prev;
					temp->prev->next = temp->next;
				}
				node->next = node->prev = NULL;
				break;
			}
			else
			{
				temp = temp->next;
			}
		}
	}

	return node;
}



int parse_xml_attribute(char *in,int in_len, char *startKey, char *endKey, xchar *out)
{
        int i=0, j=0;
	if (in && *in)
	{
        	for(i=0;i<in_len; ++i)
        	{
                	if(strncmp(&in[i], startKey, strlen(startKey)) == 0)
                	{
                	        i+=strlen(startKey);
                       		j=i;
                	}
                	else if(strncmp(&in[i], endKey,strlen(endKey)) == 0)
                	{
                        	printf("\nRM: KeyName:%s, KeyValue: %.*s\n", startKey,i-j,in+j);
                        	out->len=i-j;
                        	out->s=in+j;
                        	return 1;
                	}

        	}
	}
        out->len=0;
        out->s = NULL;
        return 0;
}

