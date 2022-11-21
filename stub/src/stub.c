#include "../include/headers.h"
#include "../include/module.h"
#include "../include/utils.h"
#define FIFO "/tmp/myfifo"

int validateIpPort(char *endpt, end_peers *p);

int main()
{

  FILE * fptr;
  char * token1;
  char *end_str;
  char *myfile = "./tcp_clb.cfg";
  char buf[1024];
  int cnt = 0, i = 0, pip;
  configurator *config;

  fptr = fopen(myfile,"r");
  if (!fptr) {
    printf("\nUnable to open file:%s\n",myfile);
    exit(1);
  }
  config = (configurator*)malloc(1*sizeof(configurator));

  memset(buf, 0, 1024);
  while(fgets(buf, 1024, fptr) != NULL) 
  {
    printf("\nRead line: [%s]\n", buf);
    //GW=127.0.0.1:7878,127.0.0.1:7676
    if (chkSubStr(buf, "GW"))
    {
      printf("\nGWs read: [%s]\n", buf);
      //How many GWs ?
      cnt = chkCnt(buf, ',');
      config->num_peers = cnt+1;
      config->peers = (end_peers*)malloc(config->num_peers*sizeof(end_peers));
      //How many '=' ?
      cnt = chkCnt(buf, '=');
      if (cnt != 1) invalid();
      //Split on '='
      token1 = strtok_r(buf, "=", &end_str);
      //Split on '=' and ','
      token1 = strtok_r(NULL, "=,", &end_str);
      while (token1 != NULL)
      {
        //Check IP:port format
        if (chkCnt(token1,':') != 1) invalid();
        if (!validateIpPort(token1,&config->peers[i])) invalid();

        token1 = strtok_r (NULL, ",", &end_str);
        printf ("\nCheck for next GW\n");
        i++;
      }
    } else if (chkSubStr(buf, "WT"))
    {
      printf("\nWTs read: [%s]\n", buf);
      //WT=4
      //How many '=' ?
      cnt = chkCnt(buf, '=');
      if (cnt != 1) invalid();
      //Split on '='
      token1 = strtok(buf, "=");
      token1 = strtok(NULL, "=");
      cnt = atoi(token1);
      if (!chkWT(cnt)) invalid();
      config->num_worker_threads = cnt;
    }
    memset(buf, 0, 1024);
  }
  fclose(fptr);
  mknod(FIFO, S_IFIFO|0640, 0);
  pip = open(FIFO, O_WRONLY);  
  mod_init(*config);
  
  sleep(10);
  return 0;
}

int validateIpPort(char *endpt, end_peers *p)
{
  char * tok, *endstr;
  printf ("\nipPort: str=%s\n", endpt);
  tok = strtok_r(endpt, ":", &endstr);
  printf ("\nIP: tok=%s\n", tok);
  strcpy(p->ip, tok);
  if (!chkIP(tok)) return 0;
  tok = strtok_r(NULL, ":", &endstr);
  printf ("\nipPort: tok=%s\n", tok);
  p->port = atoi(tok);
  if (!chkPort(atoi(tok))) return 0;

  return 1;
}
