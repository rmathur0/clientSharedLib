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
	struct sockaddr_in server_addr;

	gconn_list = (con_t*)calloc(cfg->num_peers, sizeof(con_t));

        /* Create TCP connections and store them */
        for (i = 0; i < cfg->num_peers; i++)
        {
		retry = 1;
connect_now:            if ((conn = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                        perror("\nSocket creation failed. Exiting.\n");
                        exit(1);
                }
                bzero((char *) &server_addr, sizeof (server_addr));
                inet_pton(AF_INET, cfg->peers[i].ip, &(server_addr.sin_addr));
                server_addr.sin_port = htons(cfg->peers[i].port);
                rc = connect(conn, (struct sockaddr *)&server_addr, sizeof(server_addr));
                if (rc < 0) {
                        printf ("\nConnect failed for [%s:%d] \n", cfg->peers[i].ip, cfg->peers[i].port);
                        if (retry == 1) {
				perror("\nRetrying in 5 seconds.\n");
				close(conn);
				retry--;
				sleep(5);
                        	goto connect_now;
			}
                }
                setnonblock(conn);
                setsockopt(conn, SOL_SOCKET, SO_KEEPALIVE, &keepalive , sizeof(keepalive));
                setsockopt(conn, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
                setsockopt(conn, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
                setsockopt(conn, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
                gconn_list[i].fd = conn;
                gconn_list[i].state = 1;
                gconn_list[i].peer_id = i;
        }
	return gconn_list;
}

void monitor_conn(configurator *cfg)
{
	int i = 0, rc = -1, err = 0;
        int keepalive = 1, keepcnt = 5, keepidle = 30, keepintvl = 120;
        struct sockaddr_in server_addr;
	socklen_t len = sizeof (err);

	printf ("\nmonitoring TCP connections.\n");
        for (i = 0; i < cfg->num_peers; i++) {
        	rc = getsockopt (gconn_list[i].fd, SOL_SOCKET, SO_ERROR, &err, &len);
        	if ((rc != 0)||(err != 0)) {
        		printf ("\nerror getting getsockopt return code: %s and socket error: %s\n", strerror(rc), strerror(err));
        		close(gconn_list[i].fd);
        		gconn_list[i].state=0;
        		gconn_list[i].fd = socket(AF_INET, SOCK_STREAM, 0);
        		bzero((char *) &server_addr, sizeof (server_addr));
        		inet_pton(AF_INET, cfg->peers[i].ip, &(server_addr.sin_addr));
        		server_addr.sin_port = htons(cfg->peers[i].port);
        		connect(gconn_list[i].fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        		setnonblock(gconn_list[i].fd);
        		setsockopt(gconn_list[i].fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive , sizeof(keepalive));
        		setsockopt(gconn_list[i].fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
        		setsockopt(gconn_list[i].fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
        		setsockopt(gconn_list[i].fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
        		gconn_list[i].state = 1;
        	}
        }
}


/* Check if provided ID (TID+SID) is present in the connection Q */
int is_ID_present_idq(tsidque_t **head, char *tid, char *sid, int *conn)
{
	tsidque_t *temp = *head;
	char *temp_id;

	if (*head == NULL)
		return 0;
	if (sid && *sid)
	{
		temp_id = (char*)calloc((strlen(tid) + strlen(sid) + 1), sizeof(char));
		strcpy(temp_id, tid);
		strcat(temp_id, sid);
	}
	else 
	{
		temp_id = tid;
	}
	while(temp->next != NULL)
	{
		
		if ( strcmp(temp_id, temp->id) == 0)
		{
			*conn = temp->conn;
			free(temp_id);
			return 1;
		}
		temp = temp->next;
	}
	return 0;
}

void update_entry_idq(tsidque_t **head, char *id)
{
	tsidque_t *temp = *head;
	while(temp->next != NULL)
	{
		if ( strcmp(id, temp->id) == 0)
		{
			gettimeofday(&temp->ATime, NULL);
			temp->ETime = temp->ATime;
			temp->ETime.tv_sec+= EXPIRY;
			return;
		}
	}
}

void add_entry_idq(tsidque_t **head, char *tid, char *sid)
{
	tsidque_t *temp = (tsidque_t*)calloc(1, sizeof(tsidque_t));
	char *temp_id;
	tsidque_t *node = *head;
	int rc = -1, num_peers = -1, con = -1;

	if (sid && *sid)
        {
                temp_id = (char*)calloc((strlen(tid) + strlen(sid) + 1), sizeof(char));
                strcpy(temp_id, tid);
                strcat(temp_id, sid);
        }
        else 
        {
                temp_id = (char*)calloc(strlen(tid)+1, sizeof(char));
		strcpy(temp_id, tid);
        }
	temp->id = temp_id;
	gettimeofday(&temp->ATime, NULL);
	temp->ETime = temp->ATime;
	temp->ETime.tv_sec+= EXPIRY;
	temp->next = NULL;
	
	num_peers = get_peers();
	rc = is_ID_present_idq(head, tid, sid, &con);
	if (rc == 1)
		update_entry_idq(head, temp_id);
	else {
		num_peers = get_peers();
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
	}
	if (node == NULL)
	{
		*head = temp;
		temp->prev = NULL;
		return;
	}
	while(node->next != NULL)
		node = node->next;
	node->next = temp;
	temp->prev = node;
}

void rem_expired_idq(tsidque_t **head)
{
	tsidque_t *node = *head, *temp;
	struct timeval now;

	if (node == NULL)
		return;
	do
	{
		temp = node->next;
		gettimeofday(&now, NULL);
		if ( (node->ETime.tv_sec < now.tv_sec) || ((node->ETime.tv_sec == now.tv_sec) && (node->ETime.tv_usec < now.tv_usec)) )
		{
			/* Single entry */
			if (node->prev == NULL) {
				free(node->id);
				free(node);
				*head = NULL;
				break;
			}
			else if (node->next == NULL) { /* Last element */
				node->prev->next = NULL;
				free(node->id);
				free(node);
				break;
			} else {
				node->prev->next = node->next;
				node->next->prev = node->prev;
				free(node->id);
				free(node);
			}
		}
			node = temp;
	}while(node != NULL);
}


/* Add element in msgque_t */
void push_to_msgq(msgque_t **msghead, tsidque_t **idhead, char *tid, char *sid, int len, int conn, char *data)
{
	msgque_t *trav = *msghead;
	msgque_t *temp = (msgque_t*)calloc(1, sizeof(msgque_t));
	char *temp_id;
        int rc = -1, num_peers = -1, con = -1;

        if (sid && *sid)
        {
                temp_id = (char*)calloc((strlen(tid) + strlen(sid) + 1), sizeof(char));
                strcpy(temp_id, tid);
                strcat(temp_id, sid);
        }
        else
        {
                temp_id = (char*)calloc(strlen(tid)+1, sizeof(char));
                strcpy(temp_id, tid);
        }
	temp->data = data;
	temp->len = len;
	strcpy(temp->id, temp_id);
	temp->conn = conn;
	rc = is_ID_present_idq(idhead, tid, sid, &temp->conn);
	if (rc == 1)
		update_entry_idq(idhead, temp_id);
	else
		add_entry_idq(idhead, tid, sid);
	
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
	
	if (temp == NULL)
		return NULL;
	if ((temp->prev == NULL) && (temp->conn == con)) {
		*head = NULL;
		return temp;
	}
	while(temp->next != NULL)
	{
		if (temp->conn == con)
		{
			temp->prev->next = temp->next;
			temp->next->prev = temp->prev;
			break;
		}
		temp = temp->next;
	}
	return temp;
}
