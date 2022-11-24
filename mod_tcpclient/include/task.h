/*
 * Contains information about the data containers and their associated operations
 */
#ifndef __TASK_H_
#define __TASK_H_

#define MAX_BUF 2050
#define ID_SIZE 128
#define KEY_SIZE 256
#define EXPIRY 120

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
	struct timeval ATime;
	struct timeval ETime;
	struct idque *prev;
	struct idque *next;
} tsidque_t;

/* DS to hold received unit on PIPE/Socket */
typedef struct msgque {
	char *data;
	char id[KEY_SIZE];
	int len;
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

/* Check if provided ID is present in the connection Q */
int is_ID_present_idq(tsidque_t **head, char *tid, char *sid, int *conn);

/* Add new element in this Q */
int add_entry_idq(tsidque_t **head, char *tid, char *sid);

/* Update the existing element in Q */
int update_entry_idq(tsidque_t **head, char *id);

/* Del expired element in this Q */
void rem_expired_idq(tsidque_t **head);

/* Add element in msgque_t */
void push_to_msgq(msgque_t **msghead, tsidque_t **idhead, char *tid, char *sid, int len, char *data);

/* Pop element from msgque_t for a connection*/
msgque_t *pop_from_msgq(msgque_t **head, int con); 

/* Add element in refque_t */
void push_to_refq(refque_t **refhead, long addr);

/* Pop element from refque_t for a connection*/
long pop_from_refq(refque_t **head);

#endif
