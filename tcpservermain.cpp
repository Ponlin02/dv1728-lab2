#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include "protocol.h"

// Included to get the support library
#include <calcLib.h>

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG


using namespace std;

bool serverSetup(int &sockfd, char *hoststring, char *portstring)
{
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

  int listen_status = listen(sockfd, SOMAXCONN);
  if(listen_status == -1)
  {
    printf("\nERROR: Listen Failed\n");
    printf("Returned: %d\n", listen_status);
    close(sockfd);
    return false;
  }

  #ifdef DEBUG
  printf("Listen Succeded!\n");
  #endif

  freeaddrinfo(res);
  return true;
}

int client_calc(const char* src)
{
  char operation[10];
  int n1;
  int n2;
  int result;

  sscanf(src, "%s %d %d", operation, &n1, &n2);

  if(strcmp(operation, "add") == 0)
  {
    result = n1 + n2;
  }
  else if(strcmp(operation, "sub") == 0)
  {
    result = n1 - n2;
  }
  else if(strcmp(operation, "mul") == 0)
  {
    result = n1 * n2;
  }
  else if(strcmp(operation, "div") == 0)
  {
    result = n1 / n2;
  }

  return result;
}

uint32_t client_calc(uint32_t operation, uint32_t n1, uint32_t n2)
{
  if(operation == 1)
  {
    return n1 + n2;
  }
  else if(operation == 2)
  {
    return n1 - n2;
  }
  else if(operation == 3)
  {
    return n1 * n2;
  }
  else if(operation == 4)
  {
    return n1 / n2;
  }
  else
  {
    printf("ERROR: Incorrect use of client_calc");
    return -1;
  }
}

ssize_t send_helper(int sockfd, const char* send_buffer)
{
  ssize_t bytes_sent = send(sockfd, send_buffer, strlen(send_buffer), 0);

  #ifdef DEBUG
  printf("\nBytes sent: %ld\n", bytes_sent);
  printf("SERVER SENT:\n%s\n", send_buffer);
  #endif

  return bytes_sent;
}

ssize_t recv_helper(int sockfd, char* recv_buffer, size_t bufsize)
{
  ssize_t bytes_recieved = recv(sockfd, recv_buffer, bufsize - 1, 0);
  if(bytes_recieved > 0)
  {
    recv_buffer[bytes_recieved] = '\0';
  }

  #ifdef DEBUG
  printf("\nBytes recieved: %ld\n", bytes_recieved);
  printf("CLIENT RESPONSE:\n%s", recv_buffer);
  #endif
  
  return bytes_recieved;
}

void case_text(int clientfd)
{
  char recv_buffer[1024];
  char send_buffer[10];
  initCalcLib();
  sprintf(send_buffer, "%s %d %d\n", randomType(), randomInt() + 1, randomInt());
  send_helper(clientfd, send_buffer);
  int bytes_recieved = recv_helper(clientfd, recv_buffer, sizeof(recv_buffer));
  if(bytes_recieved == -1)
  {
    printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
    return;
  }

  if(atoi(recv_buffer) == client_calc(send_buffer))
  {
    char send_buffer2[] = "OK\n";
    send_helper(clientfd, send_buffer2);
  }
  else
  {
    char send_buffer2[] = "NOT OK\n";
    send_helper(clientfd, send_buffer2);
  }
}

void case_binary(int clientfd)
{
  initCalcLib();
  uint32_t operation = (randomInt() % 4) + 1;
  uint32_t v1 = randomInt();
  uint32_t v2 = randomInt() + 1;
  int32_t result = client_calc(operation, v1, v2);

  calcMessage msg;
  msg.type = htons(2);
  msg.message = htonl(2);
  msg.protocol = htons(6);
  msg.major_version = htons(1);
  msg.minor_version = htons(1);

  calcProtocol pro;
  pro.type = htons(1);
  pro.major_version = htons(1);
  pro.minor_version = htons(1);
  pro.id = htonl(69);
  pro.arith = htonl(operation);
  pro.inValue1 = htonl(v1);
  pro.inValue2 = htonl(v2);
  pro.inResult = htonl(0);

  ssize_t bytes_sent = send(clientfd, &pro, sizeof(pro), 0);
  ssize_t bytes_recieved = recv(clientfd, &pro, sizeof(pro), 0);

  #ifdef DEBUG
  printf("\nBytes sent: %ld\n", bytes_sent);
  printf("bytes recieved %ld\n", bytes_recieved);
  #endif

  if(bytes_recieved == 26)
  {
    pro.type = ntohs(pro.type);
    pro.major_version = ntohs(pro.major_version);
    pro.minor_version = ntohs(pro.minor_version);
    //pro.id = ntohl(pro.id);
    pro.inResult = ntohl(pro.inResult);

    if(pro.type == 2 &&
      pro.major_version == 1 &&
      pro.minor_version == 1 &&
      pro.inResult == result)
    {
      msg.message = htonl(1);
    }

    #ifdef DEBUG
    printf("inResult: %d\n", pro.inResult);
    printf("real result: %d\n", result);
    #endif
  }
  else if(bytes_recieved == -1)
  {
    printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
    return;
  }
  else
  {
    printf("NOT OK\n");
    printf("ERROR: WRONG SIZE OR INCORRECT PROTOCOL\n");
    return;
  }

  ssize_t bytes_sent2 = send(clientfd, &msg, sizeof(msg), 0);

  #ifdef DEBUG
  printf("\nBytes sent: %ld\n", bytes_sent2);
  #endif
}

void handleClient(int clientfd)
{
  char recv_buffer[1024];
  char send_buffer[] = "TEXT TCP 1.1\nBINARY TCP 1.1\n\n";
  send_helper(clientfd, send_buffer);
  int bytes_recieved = recv_helper(clientfd, recv_buffer, sizeof(recv_buffer));
  if(bytes_recieved == -1)
  {
    printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
    return;
  }

  if(strstr(recv_buffer, "TEXT TCP 1.1") != NULL)
  {
    case_text(clientfd);
  }
  else if(strstr(recv_buffer, "BINARY TCP 1.1") != NULL)
  {
    case_binary(clientfd);
  }
  else
  {
    printf("ERROR: MISSMATCH PROTOCOL\n");
    return;
  }
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
  bool setup_status = serverSetup(sockfd, hoststring, portstring);
  if(!setup_status)
  {
    return EXIT_FAILURE;
  }

  //zombie child cleanup
  signal(SIGCHLD, SIG_IGN);

  while(1)
  {
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int pid;

    int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_size);
    if(clientfd == -1)
    {
      printf("\nERROR: clientfd Failed\n");
      printf("Returned: %d\n", clientfd);
      continue;
    }

    //Set options for the client socket
    timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    pid = fork();
    if(pid == 0)
    {
      //child process
      close(sockfd);
      handleClient(clientfd);
      close(clientfd);
      _exit(0);
    }
    else if(pid > 0)
    {
      //parent process
      close(clientfd);
    }
    else
    {
      close(sockfd);
      close(clientfd);
      printf("ERROR: fork failed\n");
    }
  }
  
  close(sockfd);
  return 0;
}
