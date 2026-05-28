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

#define PORT "3490"
#define BACKLOG 10
#define MAXBUFLEN 100

void* get_in_addr(struct sockaddr* sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
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
	}
	freeaddrinfo(servinfo);
	if(p = NULL)
	{
		fprintf(stderr,"server: fail to bind.\n");
		return -1;
	}
	if(listen(sockfd,BACKLOG)==-1)
	{
		perror("server: listen");
		return -1;
	}
	return sockfd;
}
int main(void)
{
	int listener_fd;
	int newfd;
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;
	char remoteIP[INET6_ADDRSTRLEN];
	char msg[MAXBUFLEN];
	int fd_count = 0;
	int fd_size = 5;
	struct pollfd* pfds = malloc(sizeof *pfds * fd_size);
	if((listener_fd = get_listener_socket())==-1)
	{
		fprintf(stderr,"fail to establish listener socket.\n");
		exit(1);
	}
	else
	{
		printf("Successfully establish listener socket.\n");
		printf("waiting for connection..\n");
	}
	pfds[0].fd = listener_fd;
	pfds[0].events = POLLIN;
	fd_count = 1;
	while(1)
	{
		int poll_count = poll(pfds,fd_count,-1);
		if(poll_count == -1)
		{
			perror("poll");
			exit(1);
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
						add_to_pfds(&pfds,newfd,&fd_count,&fd_size);
						inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*)&remoteaddr),remoteIP,sizeof remoteIP);
						printf("get connection from %s , socket number: %d\n",remoteIP,newfd);
						send(newfd, "Welcome To beej's chat server!\n",40,0);
					}
				}
				else
				{
					int numbytes = recv(pfds[i].fd,msg,MAXBUFLEN,0);
					if(numbytes <= 0)
					{
						if(numbytes == 0)
						{
							printf("connection closed from %d\n",pfds[i].fd);
						}
						else
						{
							perror("recv");
						}

							close(pfds[i].fd);
							delete_from_pfds(&pfds,i,&fd_count);
					}
					else
					{
						for(int j = 0; j < fd_count ; j++)
						{
							if((pfds[j].fd != listener_fd) &&(pfds[j].fd != pfds[i].fd))
							{
								if(send(pfds[j].fd,msg,numbytes,0)==-1)
									perror("send");
							}
						}
					}

				}
			}
		}
	}

}

