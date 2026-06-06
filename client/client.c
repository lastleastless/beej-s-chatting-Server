#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

#define MAXDATALEN 100
#define PORT "3490"
#define MAXIDLEN 10

int sendall(int s,char* data,int* len)
{
	int total = 0;
	int bytesleft = *len;
	int n;
	while(total < *len)
	{
		n = send(s,data+total,bytesleft,0);
		if(n==-1)
			{
				if(errno == EAGAIN || errno == EWOULDBLOCK)
					continue;
				else
					break;
			}
		total += n;
		bytesleft -= n;
	}
	return n==-1 ? -1 : 0;
}


void *get_in_addr(struct sockaddr* sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc,char **argv)
{
	char s[INET6_ADDRSTRLEN];
	char packet[2+MAXIDLEN+3+MAXDATALEN+3];
	//packet -> totalsize , IDsize , ID , BUFsize , BUF
	int rv;
	struct addrinfo hints,*servinfo,*p;
	int sockfd;
	if(argc != 3)
	{
		fprintf(stderr,"Usage: client Hostname ID\n");
		exit(1);
	}
	if(strlen(argv[2]) > MAXIDLEN)
	{
		fprintf(stderr,"ID length must be less than 10\n");
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
	inet_ntop(p->ai_family,get_in_addr((struct sockaddr*)p->ai_addr),s,sizeof s);
	freeaddrinfo(servinfo);
	printf("Sucessfully Connect to server: %s ,current ID: %s\n",s,argv[2]);
	int idlen = strlen(argv[2]);
	uint16_t netidlen = htons(idlen);
	while(1)
	{
		char data[MAXDATALEN+1];
		if(fgets(data,MAXDATALEN,stdin)==NULL)
			break;
		data[strcspn(data,"\n")] = '\0';
		int datalen = strlen(data);
		uint16_t netdatalen = htons(datalen);
		int offset = 2;
		memcpy(packet+offset,&netidlen,2);
		offset += 2;
		memcpy(packet+offset,argv[2],idlen);
		offset += idlen;
		memcpy(packet+offset,&netdatalen,2);
		offset += 2;
		memcpy(packet+offset,data,datalen);
		offset += datalen;
		uint16_t totalsize = htons(offset);
		memcpy(packet,&totalsize,2);
		if(sendall(sockfd,packet,&offset)==-1)
		{
			fprintf(stderr,"client : send fail..\n");
		}
		else
		{
			printf("Successfully send packet to %s\n",s);
		}
		memset(&data,0,sizeof data);
		memset(&packet,0,sizeof packet);
		
	}
	close(sockfd);
	return 0;
	

}
