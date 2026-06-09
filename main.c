#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>

#define PORT "3490"
#define BACKLOG 10
#define MAXDATALEN 50
#define MAXIDLEN 10


struct userinfo
{
	char id[MAXIDLEN+1];
	char buf[MAXDATALEN+1];
};

void* get_in_addr(struct sockaddr* sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendall(int s,char* buf,int *len)
{
	int total = 0;
	int bytesleft = *len;
	int n;
	while(total < *len)
	{
		n =send(s,buf + total ,bytesleft,0);
		if(n == -1)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			else
				break;
		}
		total += n;
		bytesleft -= n;
	}
	return n == -1 ? -1 : 0;
}

int recvall(int s,char* buf,int len)
{
	int total = 0;
	int bytesleft = len;
	int n;
	while(total < len)
	{
		n = recv(s,buf + total,bytesleft,0);
		if(n ==-1)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{	
				usleep(1000);
				continue;
			}
			else
				break;
		}
		if(n == 0)
		{
			return 0;
		}
		total += n;
		bytesleft -= n;
	}
	return n == -1 ? -1 : total;
}

void add_to_pfds(struct pollfd* pfds[],int newfd,int* fd_count,int* fd_size)
{
	if(*fd_size == *fd_count)
	{
		*fd_size *= 2;
		struct pollfd* temp = realloc(*pfds,sizeof(struct pollfd) * (*fd_size));
		if(temp == NULL)
		{
			fprintf(stderr,"realloc fail\n");
			exit(1);
		}
		*pfds = temp;
	}
	(*pfds)[*fd_count].fd = newfd;
	(*pfds)[*fd_count].events = POLLIN;
	(*fd_count)++;
}
void delete_from_pfds(struct pollfd* pfds[],int idx,int* fd_count)
{
	(*pfds)[idx] = (*pfds)[(*fd_count)-1];
	(*fd_count)--;
}

int get_listener_socket(void)
{
	int rv;
	int sockfd;
	int yes = 1;
	struct addrinfo hints,*servinfo,*p;
	memset(&hints,0,sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	if((rv = getaddrinfo(NULL,PORT,&hints,&servinfo))==-1)
	{
		fprintf(stderr,"pollserver: getaddrinfo %s\n",gai_strerror(rv));
		return -1;
	}
	for(p = servinfo; p != NULL ; p = p->ai_next)
	{
		if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1)
		{
			perror("server: socket");
			continue;
		}
		if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1)
		{
			perror("server: setsocktop");
			continue;
		}
		if(bind(sockfd,p->ai_addr,p->ai_addrlen)==-1)
		{
			perror("server: bind");
			continue;
		}
	break;
	}
	freeaddrinfo(servinfo);
	if(p == NULL)
	{
		fprintf(stderr,"server: fail to bind.\n");
		return -1;
	}
	if(listen(sockfd,BACKLOG)==-1)
	{
		perror("server: listen");
		return -1;
	}
 	fcntl(sockfd,F_SETFL,O_NONBLOCK);
	return sockfd;
}
int main(void)
{
        time_t endtime = time(NULL);
	int dot = 1;
	int listener_fd;
	int newfd;
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;
	char remoteIP[INET6_ADDRSTRLEN];
	char packet[2+2+MAXDATALEN+2+MAXIDLEN];
	int fd_count = 0;
	int fd_size = 5;
	int user_count = 0;
	int user_size = 0;
	struct pollfd* pfds = malloc(sizeof *pfds * fd_size);
	struct userinfo u;
	if((listener_fd = get_listener_socket())==-1)
	{
		fprintf(stderr,"fail to establish listener socket.\n");
		exit(1);
	}
	else
	{
		printf("Successfully establish listener socket.\n");
		printf("Welcome to beej's chatting server!.\n");
		printf("-------------------------------------\n");
	}
	pfds[0].fd = listener_fd;
	pfds[0].events = POLLIN;
	fd_count = 1;
	while(1)
	{
	       int poll_count = poll(pfds,fd_count,1000);
	       if(poll_count == -1)
	       {
			perror("poll");
			exit(1);
	       }
 	       time_t currenttime = time(NULL);
       	       if(currenttime - endtime >= 3)
       	       {
           	 if(fd_count == 1)
           	 {
               	 printf("\rwaiting for connection");
               	 for(int i = 0; i < dot ; i++)
               	 {
                	    printf(".");
               	 }
               	 printf("   ");
               	 fflush(stdout);
               	 dot++;
               	 if(dot == 4)
                   dot = 1;
           	 }
       		}
		for(int i = 0; i < fd_count ; i++)
		{
			if(pfds[i].revents & POLLIN)
			{
				if(pfds[i].fd == listener_fd)
				{
					addrlen = sizeof remoteaddr;
					newfd = accept(listener_fd,(struct sockaddr*)&remoteaddr,&addrlen);
					if(newfd == -1)
						perror("accept");
					else
					{
 			                        fcntl(newfd,F_SETFL,O_NONBLOCK);
						add_to_pfds(&pfds,newfd,&fd_count,&fd_size);
						inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*)&remoteaddr),remoteIP,addrlen);
						printf("new connection : %s, socket number: %d\n",remoteIP,newfd);
					}
				}
				
				else//broadcast msg from user.
				{
					uint16_t net_total = 0;
					int header_res = recvall(pfds[i].fd,(char*)&net_total,2);
					if(header_res == 0)
					{
						printf("close socket%d\n",pfds[i].fd);
						close(pfds[i].fd);
						delete_from_pfds(&pfds,i,&fd_count);
						i--;
						memset(packet,0,sizeof packet);
						continue;
					}
					if(header_res == -1)
					{
						perror("recv");
						close(pfds[i].fd);
						delete_from_pfds(&pfds,i,&fd_count);
						i--;
						continue;
					}
					int totalsize = ntohs(net_total);
					if(totalsize < 4)
					{
						fprintf(stderr,"Security alert: invaild packet size on socket %d.\n",i);
						close(pfds[i].fd);
						delete_from_pfds(&pfds,i,&fd_count);
						i--;
						continue;
					}
					int offset = 2;
					if(totalsize > sizeof(packet))
					{
						fprintf(stderr,"Security alert: invaild total size on socket %d.\n",i);
						close(pfds[i].fd);
						delete_from_pfds(&pfds,i,&fd_count);
						i--;
						continue;
					}
					int bodysize = totalsize - 2;
					int body_res = recvall(pfds[i].fd,packet + offset, bodysize);
					if(body_res <= 0)
					{
						fprintf(stderr,"Connection loss while recv body from socket %d\n",i);
						close(pfds[i].fd);
						delete_from_pfds(&pfds,i,&fd_count);
						i--;
						continue;
					}
					uint16_t netidlen;
					memcpy(&netidlen,packet+offset,2);
					int idlen = ntohs(netidlen);
					if(idlen > MAXIDLEN)
					{
						fprintf(stderr,"Security alert: invaild id size on socket %d.\n",i);
						close(pfds[i].fd);
						delete_from_pfds(&pfds,i,&fd_count);
						i--;
						continue;
					}
					printf("idlen: %d, ",idlen);
					offset += 2;

					char id[MAXIDLEN + 1];
					memset(id,0,sizeof id);
					memcpy(id,packet+offset,idlen);
					offset += idlen;
					id[idlen] = '\0';
					printf("%s : ",id);
					
					char data[MAXDATALEN + 1];
					uint16_t netdatalen;
					memset(data,0,sizeof data);
					memcpy(&netdatalen,packet+offset,2);
					offset+=2;
					int datalen = ntohs(netdatalen);
					if(datalen > MAXDATALEN )
					{
						fprintf(stderr,"Security alert: invaild id size on socket %d.\n",i);
						close(pfds[i].fd);
						delete_from_pfds(&pfds,i,&fd_count);
						i--;
						continue;
					}
					memcpy(data,packet+offset,datalen);
					id[datalen] = '\0';
					printf(" %s\n",data);
					offset += idlen;
					for(int j = 1; j < fd_count ; j++)
					{
						if(pfds[j].fd != listener_fd && pfds[j].fd != pfds[i].fd)
						{
							int numbytes = sendall(pfds[j].fd,packet,&offset);
							if(numbytes <= -1)
								perror("send");
						}
					}
					memset(packet,0,sizeof packet);
				}
			}
		}
	}

}

