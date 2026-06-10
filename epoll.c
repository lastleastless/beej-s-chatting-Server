#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define MAXEVENTS 20
#define MAXIDLEN 10
#define MAXDATALEN 10

#define PORT "4440"
#define BACKLOG 10

int sendall(int s,char* packet,int len)
{
	int total = 0;
	int bytesleft = len;
	int n;
	while(total < len)
	{
		n = send(s,packet+total,bytesleft,0);
		if(n <= 0)
		{
			if(n == 0)
				return 0;
			else
			{
				if(errno == EAGAIN || errno == EWOULDBLOCK)
					continue;
				else
				{
					n = -1;
					break;
				}
			}
		}
		total += n;
		bytesleft -= n;
	}
	return n == -1 ? -1 : total;
}
int recvall(int s,char* buffer,int len)
{
	int total = 0;
	int bytesleft = len;
	int n;
	while(total < len)
	{
		n = recv(s,buffer+total,bytesleft,0);
		if(n <= 0)
		{
			if(n == 0)
				return 0;
			else
			{
				if(errno == EAGAIN || errno == EWOULDBLOCK)
					continue;
				else
				{
					n = -1;
					break;
				}
			}
		}
		total += n;
		bytesleft -= n;
	}
	return n==-1 ? -1 : total;
}


void *get_in_addr(struct sockaddr* sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main()
{
	int listener_fd;
	int rv;
	struct addrinfo hints,*servinfo,*p;
	struct epoll_event ev;
	struct epoll_event events[MAXEVENTS];
	socklen_t clientaddrlen;
	int yes = 1;
	memset(&hints,0,sizeof hints);
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	if((rv = getaddrinfo(NULL,PORT,&hints,&servinfo))==-1)
	{
		fprintf(stderr,"getaddrinfo: %s\n",gai_sterror(rv));
		exit(1);
	}
	for(p =servinfo; p != NULL ;p = p->ai_next)
	{
		if((listener_fd = socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1)
		{	
			perror("recv");
			continue;
		}
		if(setsockopt(listener_fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof (int))==-1)
		{	
			perror("setsocktop");
			continue;
		}
		if(bind(listener_fd,p->ai_addr,p->ai_addrlen)==-1)
		{
			perror("bind");
			continue;
		}
		break;
	}
	if(p==NULL)
	{
		fprintf(stderr,"server: fail to bind..\n");
		exit(1);
	}
	if(listen(listener_fd,BACKLOG)==-1)
	{
		perror("listen");
		exit(1);
	}
	fcntl(listener_fd,F_SETFL,O_NONBLOCK);
	printf("Successfully establish listener socket.\n");
	int epfd = epoll_create1(0);
	if(epfd == -1)
	{
		perror("epoll create1:");
		exit(1);
	}
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = listener_fd;
	if(epoll_ctl(epfd,EPOLL_CTL_ADD,listener_fd,&ev)==-1)
	{
		perror("epoll_ctl:");
		exit(1);
	}
	while(1)
	{
		int nfds = epoll_wait(epfd,events,MAXEVENTS,-1);
		if(nfds == -1)
		{
			if(errno == EINTR)
				continue;
			perror("epoll_wait");
			break;
		}
		for(int i = 0 ; i < nfds; i++)
		{
			if(events[i].fd == listener_fd)
			{
				char clientip[INET6_ADDRSTRLEN];
				struct sockaddr_storage clientaddr;
				socklen_t clientaddrlen = sizeof clientaddr;
				int newfd = accept(listener_fd,(struct sockaddr*)&clientaddr,&clientaddrlen);
				if(newfd == -1)
				{
					if(errno == EAGAIN || EWOULDBLOCK)
					{}
					else
					{
						perror("accept");
						break;
					}
				}
				fcntl(newfd,F_SETFL,O_NONBLOCK);
				struct poll_event client_ev;
				client_ev.events = EPOLLIN | EPOLLET;
				client_ev.data.fd = newfd;
				inet_ntop(clientaddr.ss_family,get_in_addr((struct sockaddr*)&clientaddr),&clientip,sizeof clientip);
				printf("Successfully connect to %s, socket number: %d\n",clientip,newfd);
				if(epoll_ctl(epfd,EPOLL_CTL_ADD,newfd,&ev)==-1)
				{
					perror("client epoll_clt");
					close(newfd);
				}
			}
			else
			{
				int sender_fd = events[i].data.fd;
				char packet[2+2+MAXIDLEN+2+MAXDATALEN];
				uint16_t nettotal;
				int header_res = recvall(sender_fd,(char*)&nettotal,2);
				if(header_res == -1)
				{
					perror("recv");
					close(sender_fd);
				}
				else if(header_res == 0)
				{
					printf("Connection closed from socket %d\n",sender_fd);
					close(sender_fd);
				}
				else
				{
					int totalsize = ntohs(nettotal);
					int bodysize = totalsize - 2;
					int offset = 2;
					if(totalsize < 4)
					{
						fprintf(stderr,"Security alert: invaild packet size on %d\n",sender_fd);
						close(sender_fd);
					}
					int body_res = recvall(sender_fd,packet+offset,bodysize);
					if(body_res <= 0)
					{
						fprintf(stderr,'Connection loss from %d\n",sender_fd);
						close(sender_fd);
					}
					uint16_t netidlen;
					memcpy(&netidlen,packet+offset,2);
					int idlen = ntohs(netidlen);
					if(idlen > MAXIDLEN || offset + idlen > totalsize)
					{
						fprintf(stderr,"Security alert: invaild id length: %d on fd: %d\n",idlen,sender_fd);
						close(sender_fd);
					}
					offset += 2;
					char id[MAXIDLEN+1];
					memcpy(id,packet+offset,idlen);
					id[idlen]='\0';
					offset += idlen;
					
					uint16_t netdatalen;
					memcpy(&netdatalen,packet+offset,2);
					int datalen = ntohs(netdatalen);
					if(datalen > MAXDATALEN || datalen + offset > totalsize)
					{
						fprintf(stderr,'Security alert: invaild data size: %d on fd %d\n:,datalen,sender_fd);
						close(sender_fd);
					}
					char data[MAXDATALEN+1];
					memcpy(data,packet+offset,datalen);
					offset += datalen;
					printf("%s : %s\n",id,data);
					memcpy(packet,&totalsize,2);
					for(int j = 1; j < nfds ; j++)
					{
						if(events[j].fd != sender_fd)
						{
							int sendbytes = sendall(events[j].fd,packet,totalsize);
							if(sendbytes == 0)
							{
								printf("Connection closed from %d while broadcasting.\n",events[j].fd);
								close(events[j].fd);
							}
							else if(sendbytes == -1)
							{
								perror("send:broadcasting:");
								close(events[j].fd);
							}
						}
					}
				}
				
			}
		}
	}
	
	
	
	
	
}
