#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define MAXBUFLEN 100
#define PORT "3490"

void *get_in_addr(struct sockaddr* sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc,char **argv)
{
	char server_IP[INET6_ADDRSTRLEN];
	char msg[MAXBUFLEN];
	int rv;
	struct addrinfo hints,*servinfo,*p;
	int sockfd;
	if(argc != 3)
	{
		fprintf(stderr,"Usage: client Hostname ID\n");
		exit(1);
	}
	memset(&hints,0,sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if((rv = getaddrinfo(argv[1],PORT,&hints,&servinfo))==-1)
	{
		fprintf(stderr,"client: getaddrinfo: %s\n",gai_strerror(rv));
		return 1;
	}
	for(p = servinfo; p != NULL ; p = p->ai_next)
	{
		if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1)
		{
			perror("client: socket");
			continue;
		}
		if(connect(sockfd,p->ai_addr,p->ai_addrlen)==-1)
		{
			perror("client: connect");
			continue;
		}
		break;
	}
	if(p==NULL)
	{
		fprintf(stderr,"client: fail to bind.\n");
		exit(1);
	}
	freeaddrinfo(servinfo);
	inet_ntop(p->ai_family,get_in_addr((struct sockaddr*)&p->ai_addr),server_IP,sizeof server_IP);
	printf("Sucessfully Connect to server: %s\n",server_IP);
	int recvnumbytes;
	int sendnumbytes;
	if((sendnumbytes = send(sockfd,argv[2],strlen(argv[2]),0))==-1)
	{
		perror("client: send");
		exit(1);
	}
	if((recvnumbytes = recv(sockfd,msg,sizeof msg,0))==-1)
	{
		perror("client: recv");
		exit(1);
	}
	msg[recvnumbytes] = '\0';
	printf("%s\n",msg);
	while(1)
	{
	}
	close(sockfd);
	return 0;
	

}
