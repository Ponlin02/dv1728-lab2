#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include "protocol.h"

// Included to get the support library
#include <calcLib.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG


using namespace std;

bool serverSetup(int &sockfd, char hoststring, char portstring)
{
  
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
  
  printf("TCP server on: %s:%s\n", hoststring,portstring);

  int sockfd;

  //variable that will be filled with data
  struct addrinfo *res, *pInfo;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int addrinfo_status = getaddrinfo(hoststring, portstring, &hints, &res);
  if(addrinfo_status != 0)
  {
    printf("\nERROR: getaddrinfo Failed\n");
    printf("Returned: %d\n", addrinfo_status);
    return EXIT_FAILURE;
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
    #ifdef DEBUG
    printf("Socket retry");
    #endif
  }

  if(sockfd == -1)
  {
    printf("\nERROR: Socket creation Failed\n");
    printf("Returned: %d\n", sockfd);
    return EXIT_FAILURE;
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
    return EXIT_FAILURE;
  }

  #ifdef DEBUG
  printf("Bind Succeded!\n");
  #endif

  int listen_status = listen(sockfd, SOMAXCONN);
  if(listen_status == -1)
  {
    printf("\nERROR: Listen Failed\n");
    printf("Returned: %d\n", listen_status);
    close(sockfd);
    return EXIT_FAILURE;
  }

  #ifdef DEBUG
  printf("Listen Succeded!\n");
  #endif

  freeaddrinfo(res);
  close(sockfd);
  return 0;
}
