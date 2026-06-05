#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

#define PORT "3490"
#define MAXDATASIZE 100
#define MAXIDLEN 10

struct userinfo
{
	char id[MAXIDLEN];
	char msg[MAXDATASIZE];
};


void * get_in_addr(struct sockaddr* sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc,char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr,"Usage:client hostname\n");
		exit(1);
	}
	int sockfd;
	int rv;
	struct addrinfo hints,*serv,*p;
	memset(&hints,0,sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	char s[INET6_ADDRSTRLEN];
	if((rv = getaddrinfo(argv[1],PORT,&hints,&serv))==-1)
	{
		fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(rv));
		return -1;
	}
	for(p = serv; p != NULL ; p = p->ai_next)
	{
		if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1)
		{	
			perror("socket");
			continue;
		}
		if(connect(sockfd,p->ai_addr,p->ai_addrlen)==-1)
		{	
			perror("connect");
			close(sockfd);
			continue;
		}	
		break;
	}	
	if(p == NULL)
	{
		fprintf(stderr,"fail to connect.\n");
		exit(1);
	}
	inet_ntop(p->ai_family,get_in_addr((struct sockaddr*)p->ai_addr),s,sizeof s);
	printf("successfully connect to %s\n",s);
	freeaddrinfo(serv);
	close(sockfd);
}

