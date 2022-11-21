#include <stdio.h>
#include <string.h>

void ipPort(char *str);

int main ()
{
  char str[] ="GW=127.0.0.1:7878,127.0.0.1:7676";
  char * pch, *qch, *rch;
  printf ("Splitting string \"%s\" into tokens:\n",str);
  pch = strtok_r (str,"=", &qch);
  pch = strtok_r (NULL, "=,",&qch);
  while (pch != NULL)
  {
    //pch = strtok (NULL, "=,:");
    printf ("\n%s\n",pch);
    ipPort(pch);
    pch = strtok_r (NULL, ",", &qch);
    printf("\nNext iteration\n");
  }
  return 0;
}

void ipPort(char *str)
{
  char * tok;
  printf ("\nipPort: str=%s\n", str);
  tok = strtok(str, ":");
  printf ("\nipPort: tok=%s\n", tok);
  tok = strtok(NULL, ":");
  printf ("\nipPort: tok=%d\n", atoi(tok));
}
