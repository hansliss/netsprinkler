#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFSIZE 8192

void usage(char *p) {
  fprintf(stderr, "Usage: %s -l <listen addr> -s <send addr> [-s <send addr> ...]\n", p);
  fprintf(stderr, "<send addr> = {address|host}:{port|service}\n");
}

/*
 * Notice: Destroys its first argument, and terminates on error.
 */
void parseaddr(char *addrdesc, struct addrinfo **res) {
  char *p=addrdesc;
  char *addr = NULL;
  char *service = NULL;
  char *e=addrdesc + strlen(addrdesc);
  struct addrinfo hints = {
    0,
    AF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    0,
    NULL,
    NULL,
    NULL
  };
  int r;
  while(p < e && *p != ':') {
    p++;
  }
  if (p == e) {
    service = addrdesc;
  } else {
    addr = addrdesc;
    *(p++) = '\0';
    service = p;
  }
  r = getaddrinfo(addr, service, &hints, res);
  if (r) {
    if (addr) {
      fprintf(stderr, "Fatal: %s:%s : %s\n", addr, service, gai_strerror(r));
    } else {
      fprintf(stderr, "Fatal: %s : %s\n", service, gai_strerror(r));
    }
    exit(-99);
  }
  if ((*res)->ai_family != AF_INET) {
    fprintf(stderr, "I only handle IPv4 at the moment.\n");
    exit(-98);
  }
}

int main(int argc, char *argv[]) {
  struct addrinfo *listen_ai=NULL;
  struct addrinfo **send_ai;
  struct sockaddr_in res;
  unsigned int reslen, datalen;
  int ndest=0, ssize=2, o;
  int sockfd;
  unsigned char buf[BUFSIZE];
  
  send_ai=(struct addrinfo **)malloc(ssize * sizeof(struct addrinfo *));
  while ((o = getopt(argc, argv, "l:s:")) != -1) {
    switch (o) {
    case 'l':
      parseaddr(optarg, &listen_ai);
      break;
    case 's':
      if ((ndest + 1) > ssize) {
	ssize *= 1.5;
	send_ai=(struct addrinfo **)realloc(send_ai, ssize * sizeof(struct addrinfo *));
      }
      parseaddr(optarg, &(send_ai[ndest++]));
      break;
    default:
      usage(argv[0]);
      return -1;
    }
  }
  if (!listen_ai || ndest == 0) {
    usage(argv[0]);
    return -1;
  }

  /***** loop *****/
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    return -10;
  }
  if (bind(sockfd, listen_ai->ai_addr, sizeof(*listen_ai)) < 0) {
    perror("bind()");
    return -11;
  }
  while (1) {
    reslen = sizeof(res);
    datalen = recvfrom(sockfd, buf, sizeof(buf), MSG_WAITALL, (struct sockaddr *)&res, &reslen);
    if (datalen >= 0) {
      for (int i=0; i<ndest; i++) {
	sendto(sockfd, buf, datalen, MSG_CONFIRM, send_ai[i]->ai_addr, send_ai[i]->ai_addrlen);
      }
    }
  }
  return 0;
}
