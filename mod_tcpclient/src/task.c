#include "../include/headers.h"
#include "../include/module.h"
#include "../include/task.h"

static int glast_con_rr;

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
			return 1;
		}
		temp = temp->next;
	}
	return 0;
}

void add_entry_idq(tsidque_t **head, char *tid, char *sid)
{
	tsidque_t *temp = (tsidque_t*)calloc(1, sizeof(sidque_t));
	char *temp_id;
	tsidque_t *node = *head;
	struct timeval t;
	time_t curtime;
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
		temp->conn = con;
	else {
		num_peers = get_peers();
		temp->conn = glast_con_rr % num_peers;
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
