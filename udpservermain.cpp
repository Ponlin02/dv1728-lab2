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
//#define DEBUG
#define MAXCLIENTS 100

using namespace std;

struct clientInfo
{
  struct sockaddr_storage addr;
  socklen_t addr_len;
  time_t last_seen;
  int step;
  int result;
  bool use_text;
  bool use_binary;
  bool active;
};

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
      memset(&table[i], 0, sizeof(clientInfo));
      //table[i].addr = *addr;
      memcpy(&table[i].addr, addr, addr_len);
      table[i].addr_len = addr_len;
      table[i].last_seen = time(NULL);
      table[i].step = 0;
      table[i].result = 0;
      table[i].use_binary = false;
      table[i].use_text = false;
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

void processInitialData(int sockfd, struct clientInfo *table, int index, char* buf, int bytes_recieved)
{
  if(strstr(buf, "TEXT UDP 1.1") != NULL) //case text
  {
    buf[bytes_recieved] = '\0';
    char send_buffer[10];
    sprintf(send_buffer, "%s %d %d\n", randomType(), randomInt() + 1, randomInt());
    ssize_t bytes_sent = sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&table[index].addr, table[index].addr_len);
    if(bytes_sent == -1)
    {
      printf("ERROR: Send to error\n");
      table[index].active = false;
      return;
    }

    /*#ifdef DEBUG
    printf("\nBytes recieved: %d\n", bytes_recieved);
    printf("CLIENT RESPONSE:\n%s", buf);
    #endif

    #ifdef DEBUG
    printf("\nBytes sent: %ld\n", bytes_sent);
    printf("SERVER SENT:\n%s\n", send_buffer);
    #endif*/

    table[index].step = 1;
    table[index].result = client_calc(send_buffer);
    table[index].use_text = true;
    return;
  }
  else if(bytes_recieved == sizeof(calcMessage)) //case binary
  {
    calcMessage msg;
    memcpy(&msg, buf, sizeof(calcMessage));
    msg.type = ntohs(msg.type);
    msg.protocol = ntohs(msg.protocol);
    msg.major_version = ntohs(msg.major_version);
    msg.minor_version = ntohs(msg.minor_version);
    
    if(!(msg.type == 22 && 
      msg.protocol == 17 && 
      msg.major_version == 1 && 
      msg.minor_version == 1))
    {
      printf("NOT OK\n");
      printf("ERROR: INVALID MESSAGE\n");
      table[index].active = false;

      #ifdef DEBUG
      printf("type: %d\n", msg.type);
      printf("protocol: %d\n", msg.protocol);
      printf("major: %d\n", msg.major_version);
      printf("minor: %d\n", msg.minor_version);
      #endif
      return;
    }

    uint32_t operation = (randomInt() % 4) + 1;
    uint32_t v1 = randomInt();
    uint32_t v2 = randomInt() + 1;

    calcProtocol pro;
    memset(&pro, 0, sizeof(calcProtocol));
    pro.type = htons(1);
    pro.major_version = htons(1);
    pro.minor_version = htons(1);
    pro.id = htonl(69);
    pro.arith = htonl(operation);
    pro.inValue1 = htonl(v1);
    pro.inValue2 = htonl(v2);
    pro.inResult = htonl(0);

    ssize_t bytes_sent = sendto(sockfd, &pro, sizeof(pro), 0, (struct sockaddr*)&table[index].addr, table[index].addr_len);
    if(bytes_sent == -1)
    {
      printf("ERROR: Send to error\n");
      table[index].active = false;
      return;
    }

    /*#ifdef DEBUG
    printf("\nBytes recieved: %d\n", bytes_recieved);
    printf("CLIENT RESPONSE:\n%s", buf);
    #endif

    #ifdef DEBUG
    printf("\nBytes sent: %ld\n", bytes_sent);
    #endif*/

    table[index].step = 1;
    table[index].result = client_calc(operation, v1, v2);
    table[index].use_binary = true;
    return;
  }
  else //Error
  {
    printf("NOT OK\n");
    printf("ERROR: WRONG SIZE OR INCORRECT PROTOCOL\n");
    table[index].active = false;
    return;
  }
}

void text_response(int sockfd, struct clientInfo *table, int index, char* buf, int bytes_recieved)
{
  //buf[bytes_recieved] = '\0';
  char send_buffer[10];
  if(table[index].result == atoi(buf))
  {
    strcpy(send_buffer, "OK\n");
  }
  else
  {
    strcpy(send_buffer, "NOT OK\n");
  }

  #ifdef DEBUG
  //printf("Table result: %d\n", table[index].result);
  printf("Client result: %d\n", atoi(buf));
  #endif

  sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&table[index].addr, table[index].addr_len);
  table[index].active = false;
  return;
}

void binary_response(int sockfd, struct clientInfo *table, int index, char* buf, int bytes_recieved)
{
  if(bytes_recieved != sizeof(calcProtocol))
  {
    printf("NOT OK\n");
    printf("ERROR: WRONG SIZE OR INCORRECT PROTOCOL\n");
    table[index].active = false;
    return;
  }

  calcMessage msg;
  memset(&msg, 0, sizeof(calcMessage));
  msg.type = htons(2);
  msg.message = htonl(0);
  msg.protocol = htons(17);
  msg.major_version = htons(1);
  msg.minor_version = htons(1);

  calcProtocol pro;
  memcpy(&pro, buf, sizeof(calcProtocol));
  int clientResult = ntohl(pro.inResult);
  
  if(table[index].result == clientResult)
  {
    msg.message = htonl(1);
  }
  else
  {
    msg.message = htonl(2);
  }

  ssize_t bytes_sent = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&table[index].addr, table[index].addr_len);
  if(bytes_sent == -1)
  {
    printf("ERROR: Send to error\n");
  }

  table[index].active = false;
  return;
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
  memset(client_table, 0, sizeof(client_table));
  struct timeval tv;
  initCalcLib();

  while(1)
  {
    time_t now = time(NULL);
    for(int i = 0; i < MAXCLIENTS; i++)
    {
      if(client_table[i].active && (now - client_table[i].last_seen > 10))
      {
        #ifdef DEBUG
        printf("Client timeout (slot: %d)\n", i);
        #endif
        client_table[i].active = false;
      }
    }

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int select_status = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    if(select_status == -1)
    {
      printf("ERROR: Select error\n");
      continue;
    }
    else if(select_status == 0)
    {
      //rerun loop to check for timeouts
      continue;
    }

    if(FD_ISSET(sockfd, &readfds))
    {
      char recv_buffer[1024];
      struct sockaddr_storage client_addr;
      socklen_t addr_len = sizeof(client_addr);

      int bytes_recieved = recvfrom(sockfd, recv_buffer, sizeof(recv_buffer) - 1, 0, (struct sockaddr*)&client_addr, &addr_len);
      if(bytes_recieved == -1)
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
      printf("Step: %d\n", client_table[index].step);
      #endif

      client_table[index].last_seen = time(NULL);

      if(client_table[index].step == 0)
      {
        processInitialData(sockfd, client_table, index, recv_buffer, bytes_recieved);
      }
      else if(client_table[index].step == 1)
      {
        if(client_table[index].use_binary && client_table[index].use_text)
        {
          printf("ERROR: Cant use text and binary\n");
          client_table[index].active = false;
          continue;
        }
        else if(client_table[index].use_text)
        {
          text_response(sockfd, client_table, index, recv_buffer, bytes_recieved);
        }
        else if(client_table[index].use_binary)
        {
          binary_response(sockfd, client_table, index, recv_buffer, bytes_recieved);
        }
        else
        {
          printf("ERROR: Client should use text or binary\n");
          client_table[index].active = false;
          continue;
        }
      }
    }
  }

  close(sockfd);
  return 0;
}
