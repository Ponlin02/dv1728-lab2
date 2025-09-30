#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
//#include <signal.h>
#include <time.h>
#include "protocol.h"


// Included to get the support library
#include <calcLib.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG
#define MAXCLIENTS 10

using namespace std;

struct clientInfo
{
  struct sockaddr_storage addr;
  socklen_t addr_len;
  time_t last_seen;

  bool active;
};

//Function that tries to find a client if it exists in the table, 
//if not try to put it in the table. Returns index or "-1" for ERROR
int client_lockup(struct clientInfo *table, struct sockaddr_storage *addr, socklen_t addr_len)
{
  //Try to find an existing client
  for(int i = 0; i < MAXCLIENTS; i++)
  {
    if(table[i].active &&
      table[i].addr_len == addr_len &&
      memcmp(&table[i].addr, addr, addr_len) == 0)
      {
        return i; //client index
      }
  }

  //New client, put the client in table
  for(int i = 0; i < MAXCLIENTS; i++)
  {
    if(!table[i].active)
    {
      table[i].addr = *addr;
      table[i].addr_len = addr_len;
      table[i].last_seen = time(NULL);

      table[i].active = true;
      return i; //client index
    }
  }

  //Client didnt exist, and table was full, error = -1
  return -1;
}

bool serverSetup(int &sockfd, char *hoststring, char *portstring)
{
  //variable that will be filled with data
  struct addrinfo *res, *pInfo;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  int addrinfo_status = getaddrinfo(hoststring, portstring, &hints, &res);
  if(addrinfo_status != 0)
  {
    printf("\nERROR: getaddrinfo Failed\n");
    printf("Returned: %d\n", addrinfo_status);
    return false;
  }

  #ifdef DEBUG
  printf("getaddrinfo Succeded!\n");
  #endif

  for(pInfo = res; pInfo != NULL; pInfo = pInfo->ai_next)
  {
    sockfd = socket(pInfo->ai_family, pInfo->ai_socktype, pInfo->ai_protocol);
    if(sockfd != -1)
    {
      break;
    }
  }

  if(sockfd == -1)
  {
    printf("\nERROR: Socket creation Failed\n");
    printf("Returned: %d\n", sockfd);
    return false;
  }

  #ifdef DEBUG
  printf("Socket creation Succeded!\n");
  #endif

  int opt = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  int bind_status = bind(sockfd, pInfo->ai_addr, pInfo->ai_addrlen);
  if(bind_status == -1)
  {
    printf("\nERROR: Bind Failed\n");
    printf("Returned: %d\n", bind_status);
    close(sockfd);
    return false;
  }

  #ifdef DEBUG
  printf("Bind Succeded!\n");
  #endif

  freeaddrinfo(res);
  return true;
}

int main(int argc, char *argv[]){
  if (argc < 2) {
    fprintf(stderr, "Usage: %s protocol://server:port/path.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  
  char *input = argv[1];
  char *sep = strchr(input, ':');
  
  if (!sep) {
    fprintf(stderr, "Error: input must be in host:port format\n");
    return 1;
  }
  
  // Allocate buffers big enough
  char hoststring[256];
  char portstring[64];
  
  // Copy host part
  size_t hostlen = sep - input;
  if (hostlen >= sizeof(hoststring)) {
    fprintf(stderr, "Error: hostname too long\n");
    return 1;
  }
  strncpy(hoststring, input, hostlen);
  hoststring[hostlen] = '\0';
  
  // Copy port part
  strncpy(portstring, sep + 1, sizeof(portstring) - 1);
  portstring[sizeof(portstring) - 1] = '\0';
  
  printf("UDP Server on: %s:%s\n", hoststring,portstring);

  int sockfd;
  fd_set readfds;
  bool setup_status = serverSetup(sockfd, hoststring, portstring);
  if(!setup_status)
  {
    return EXIT_FAILURE;
  }

  struct clientInfo client_table[MAXCLIENTS];

  while(1)
  {
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    int select_status = select(sockfd + 1, &readfds, NULL, NULL, NULL);
    if(select_status == -1)
    {
      printf("ERROR: Select error\n");
      continue;
    }

    if(FD_ISSET(sockfd, &readfds))
    {
      char recv_buffer[1024];
      struct sockaddr_storage client_addr;
      socklen_t addr_len = sizeof(client_addr);

      int bytes_recieved = recvfrom(sockfd, recv_buffer, sizeof(recv_buffer) - 1, 0, (struct sockaddr*)&client_addr, &addr_len);
      recv_buffer[bytes_recieved] = '\0';
      if(select_status == -1)
      {
        printf("ERROR: Recieve from error\n");
        continue;
      }
      
      int index = client_lockup(client_table, &client_addr, addr_len);
      if(index == -1)
      {
        printf("ERROR: Too many clients\n");
        continue;
      }

      #ifdef DEBUG
      printf("Client Index = %d\n", index);
      //printf("Step: %d\n", );
      #endif
    }
  }

  close(sockfd);
  return 0;
}
