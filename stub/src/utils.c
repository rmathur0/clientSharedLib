#include "../include/headers.h"
#include "../include/utils.h"

// Invalid configuration file
void invalid()
{
        printf("\nContents of the config file are not correct. Please use a valid config file.\n");
        exit(1);
}

// Check whether a substring is present in the base string
int chkSubStr(char *base, char *srch)
{
    char *ret = strstr(base, srch);
    if (ret)
        return 1;
    else 
        return 0;
}

// Count the number of entries of a particular character in the given string
int chkCnt(char *base,char ch)
{
    int i,count=0;
     for(i=0;base[i];i++)
    {
        if(base[i]==ch)
        {
          count++;
        }
    }
        return count;
}

// Check valid number
int validate_number(char *str)
{
    while (*str) {
        if(!isdigit(*str)){
            return 0;
        }
        str++;
    }
    return 1;
}

// Check if the IP address is syntactically correct or not
int chkIP(char *ip)
{
    int num, dots = 0;
    char *ptr, *endstr;
    if (ip == NULL)
       return 0;
       ptr = strtok_r(ip, ".", &endstr); //Tokenise the string using '.'
       if (ptr == NULL)
          return 0;
    while (ptr) {
       if (!validate_number(ptr)) //check whether the sub string is holding only number or not
          return 0;
          num = atoi(ptr); //convert substring to number
          if (num >= 0 && num <= 255) {
             ptr = strtok_r(NULL, ".",&endstr); //cut the next part of the string
             if (ptr != NULL)
                dots++; //increase the dot count
          } else
                return 0;
     }
     if (dots != 3) //if the number of dots are not 3, return false
        return 0;
      
    return 1;
}

// Check if the port is logical or not
int chkPort(int port)
{
    if( (port >=0) && (port <= 65535) )
        return 1;
    return 0;
}

// Check if the WT value make any sense
int chkWT(int val)
{
    if( (val >= 0) && (val <=32) )
        return 1;
    return 0;
}
