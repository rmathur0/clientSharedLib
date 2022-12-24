/*
 * Contains information about the data containers and their associated operations
 */
#ifndef __TASK_H_
#define __TASK_H_

#define MAX_BUF 2050

/* ID expiration time */
#define EXPIRY 30

/* Socket read timeout */
#define READ_SEC_TO 5
#define READ_USEC_TO 0

/* Structure to hold peer connection info */
typedef struct {
	int fd;
	int state;
	int peer_id;
} con_t;


/* Queue to determine Conn based on TID+SID */
typedef struct idque {
	char *id;
	int conn;
	char is_expired;
	struct timeval ATime;
	struct timeval ETime;
	struct idque *prev;
	struct idque *next;
	TransactionCallback_Res_f *callback_f;
	void *callback_param;
} tsidque_t;

/* DS to hold received unit on PIPE/Socket */
typedef struct msgque {
	request_t *data;
	char id[KEY_SIZE];
	int len; /* length of data->req_buf */
	int conn;
	struct msgque *prev;
	struct msgque *next;
} msgque_t;

/* List of heap addresses pointing to containing received messages from socket, to be send to PIPE */
typedef struct refque {
	long addr;
	struct refque *prev;
	struct refque *next;
} refque_t;

/* Create peers in con_t */
con_t *create_peers(configurator *cfg);

/* Monitor peers on con_t */
void monitor_sock_conn(configurator *cfg);

/* On demand recreate connection to a peer based on it's id*/
void recreate_conn(int pid, configurator *cfg);

/* Check if provided ID is present in the connection Q */
int is_ID_present_idq(tsidque_t **head, char *tid, int *conn);
int lookup_ID_idq(tsidque_t **head, char *tid, long *elapsed_msecs, tsidque_t *node);

/* Add new element in this Q */
int add_entry_idq(tsidque_t **head, char *tid, TransactionCallback_Res_f *callback_f, void *callback_param);

/* Update the existing element in Q */
int update_entry_idq(tsidque_t **head, char *id, TransactionCallback_Res_f *callback_f, void *callback_param);

/* Mark expired element in this Q */
void list_expired_idq(tsidque_t **head);

/* Del expired element in this Q */
void rem_id(tsidque_t **head, char *tid);

/* Add element in msgque_t */
void push_to_msgq(msgque_t **msghead, tsidque_t **idhead, char *tid, int len, request_t *data, TransactionCallback_Res_f *callback_f, void *callback_param);

/* Pop element from msgque_t for a connection*/
msgque_t *pop_from_msgq(msgque_t **head, int con); 

/* Add element in refque_t */
void push_to_refq(refque_t **refhead, long addr);

/* Pop element from refque_t for a connection*/
long pop_from_refq(refque_t **head);

int parse_xml_attribute(char *in,int in_len, char *startKey, char *endKey, xchar *out);
#endif
