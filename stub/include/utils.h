#ifndef __UTILS_H
#define __UTILS_H


// Check whether a substring is present in the base string
int chkSubStr(char *base, char *srch);

// Count the number of entries of a particular character in the given string
int chkCnt(char *base,char ch);

// Invalid configuration
void invalid();

// Check valid number
int validate_number(char *str);

// Check if the IP address is syntactically correct or not
int chkIP(char *ip);

// Check if the port is logical or not
int chkPort(int port);

// Check if the WT value make any sense
int chkWT(int val);


#endif
