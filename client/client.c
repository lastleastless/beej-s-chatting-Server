#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define MAXBUFLEN 100
#define PORT "3490"
#define MAXIDLEN 10

struct userinfo
{
	char id[MAXIDLEN+1];
	char buf[MAXBUFLEN+1];
};


void *get_in_addr(struct sockaddr* sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc,char **argv)
{
	char s[INET6_ADDRSTRLEN];
	struct userinfo r;
	struct userinfo u;
	int rv;
	struct addrinfo hints,*servinfo,*p;
	int sockfd;

	if(argc != 3)
	{
		fprintf(stderr,"Usage: client Hostname ID\n");
		exit(1);
	}
	if(strlen(argv[2]) >= MAXIDLEN)
	{
		fprintf(stderr,"ID length must be less than 10\n");
		exit(1);
	}
	printf("current ID: %s\n",argv[2]);
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
			close(sockfd);
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
	strncpy(u.id,argv[2],MAXIDLEN);
	freeaddrinfo(servinfo);
	fcntl(sockfd,F_SETFL,O_NONBLOCK);
	while(1)
	{
		int numbytes = recv(sockfd,&r,MAXBUFLEN + MAXIDLEN + 2,0);
		if(numbytes <= 0)
		{
			if(numbytes == 0)
			{
				printf("Connection closed from %s\n",s);
				break;
			}
			else
			{
				if(errno == EAGAIN || errno == EWOULDBLOCK)
					{
					}
				else
				{
					perror("recv");
					break;
				}
			}
		}
		char sendbuf[MAXBUFLEN];
	        fgets(sendbuf,MAXBUFLEN-MAXIDLEN-1,stdin);
		strncpy(u.buf,sendbuf,MAXBUFLEN);
		int sendbyte = 0;
		if((sendbyte = send(sockfd,(struct userinfo*)&u,sizeof u,0))==-1)
			perror("send");
	}
	close(sockfd);
	return 0;
	

}
