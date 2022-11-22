#ifndef __TASK_H_
#define __TASK_H_

#define MAX_BUF 2050
#define ID_SIZE 128
#define KEY_SIZE 256
#define EXPIRY 120

struct msgq;

typedef struct {
	int fd;
	int state;
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
typedef struct msgq {
	char *data;
	char tid[ID_SIZE];
	char sid[ID_SIZE];
	int len;
	char direction;
	struct msgq *prev;
	struct msgq *next;
} msgq_t;

/* Check if provided ID is present in the connection Q */
int is_ID_present_idq(tsidque_t **head, char *tid, char *sid, int *conn);

/* Add new element in this Q */
void add_entry_idq(tsidque_t **head, char *tid, char *sid);


#endif
