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
#include <fcntl.h>

#define MAXDATALEN 50
#define PORT "4440"
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

int recvall(int s,char* data,int len)
{
	int total = 0;
	int bytesleft = len;
	int n;
	while(total < len)
	{
		n = recv(s,data+total,bytesleft,0);
		if(n <= -1)
		{
			if(n==0)
				return 0;
			else
			{
				if(errno == EAGAIN || errno == EWOULDBLOCK)
				{
					usleep(1000);
					continue;
				}
				else
					n=-1;
			}
			break;
		}
		total += n;
		bytesleft -= n;
	}
	return n == -1 ? -1 : total;
}


void *get_in_addr(struct sockaddr* sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc,char **argv)
{
	char recvpacket[2+MAXIDLEN+2+MAXDATALEN+2];
	char s[INET6_ADDRSTRLEN];
	char packet[2+MAXIDLEN+2+MAXDATALEN+2];
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
	fcntl(sockfd,F_SETFL,O_NONBLOCK);
	inet_ntop(p->ai_family,get_in_addr((struct sockaddr*)p->ai_addr),s,sizeof s);
	freeaddrinfo(servinfo);
	printf("Sucessfully Connect to server: %s ,current ID: %s\n",s,argv[2]);
	int idlen = strlen(argv[2]);
	uint16_t netidlen = htons(idlen);
	while(1)
	{
		uint16_t net_total;
		int header_res = recvall(sockfd,(char*)&net_total,2);
		if(header_res == -1)
		{
			fprintf(stderr,"recv fail.\n");
			close(sockfd);
			exit(1);
		}
		else if(header_res == 0)
		{
			printf("Connection closed from server.\n");
			close(sockfd);
			exit(EXIT_SUCCESS);
		}
		else
		{
			int totalsize = ntohs(net_total);
			if(totalsize < 4)
			{
				fprintf(stderr,"Security alert: invaild packet size on server.\n");
				close(sockfd);
				exit(EXIT_FAILURE);
			}
			int offset = 2;
			int bodysize = totalsize - 2;
			if(totalsize > sizeof(recvpacket))
			{
				fprintf(stderr,"Security alert: invaild total packet size on server.\n");
				close(sockfd);
				exit(EXIT_FAILURE);
			}
			int body_res = recvall(sockfd,recvpacket+offset,bodysize);
			if(body_res <= -1)
			{
				fprintf(stderr,"Connection loss..\n");
				close(sockfd);
				exit(EXIT_FAILURE);
			}
			uint16_t netidlen;
			memcpy(&netidlen,recvpacket+offset,2);
			offset += 2;
			int idlen = ntohs(netidlen);
			if(idlen > MAXIDLEN)
			{
				fprintf(stderr,"Security alert: invaild id size on server.\n");
				close(sockfd);
				exit(EXIT_FAILURE);
			}
			char id[MAXIDLEN+1];
			memset(id,0,sizeof id);
			memcpy(id,recvpacket+offset,idlen);
			offset += idlen;
			id[idlen] = '\0';
			printf("%s:",id);

			uint16_t netdatalen;
			memcpy(&netdatalen,recvpacket+offset,2);
			offset += 2;
			int datalen = ntohs(netdatalen);
			if(datalen > MAXDATALEN)	
			{
				fprintf(stderr,"Security alert: invaild data size on server.\n");
				close(sockfd);
				exit(EXIT_FAILURE);
			}
			char data[MAXDATALEN+1];
			memset(data,0,sizeof data);
			memcpy(data,recvpacket+offset,datalen);
			data[datalen] = '\0';
			printf("%s\n",data);
			memset(recvpacket,0,sizeof recvpacket);
		}		
		char data[MAXDATALEN+1];
		if(fgets(data,MAXDATALEN,stdin)==NULL)
			break;
		data[strcspn(data,"\n")] = '\0';
		int datalen = strlen(data);
		printf("datasize: %d\n",datalen);
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
			printf("byteleft: %d\n",offset);
		}
		memset(&data,0,sizeof data);
		memset(&packet,0,sizeof packet);
		
	}
	close(sockfd);
	return 0;
	

}
