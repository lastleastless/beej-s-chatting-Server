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
#include <string.h>
#include <pthread.h>

#define MAXEVENTS 20
#define MAXIDLEN 10
#define MAXDATALEN 50
#define MAXCLIENTNUM 20000
#define PORT "3490"
#define BACKLOG 10
#define MAXBUFFERSIZE 100
#define MAXPACKETLEN (2 + 2 + MAXIDLEN + 2 + MAXDATALEN)

pthread_mutex_t list_m;
pthread_mutex_t m;
pthread_cond_t fill;
pthread_cond_t empty;


struct taskstruct
{
	int sender_fd;
	char packet[MAXPACKETLEN];
	int len;
	int* fd_list;
	int* fd_count;

};


struct consumestruct
{
	int tid;
	int* packet_num;
	struct taskstruct* t_queue;

};



int add_to_fdlist(int fd,int **fdlist,int *fd_count,int *fd_size)
{
	if(*fd_count == *fd_size)
	{
		*fd_size *= 2;
		if((*fd_size) > MAXCLIENTNUM)
			return 1;
		int *temp = realloc(*fdlist,sizeof (int) * (*fd_size));
		if(temp == NULL)
			return -1;
		*fdlist = temp;
		fprintf(stdout,"Successfully expand userlist to %d\n",*fd_size);
	}
	(*fdlist)[*fd_count] = fd;
	(*fd_count)++;
	return 0;
}

void delete_from_fdlist(int **fdlist,int idx,int *fd_count)
{
	(*fdlist)[idx] = (*fdlist)[(*fd_count)-1];
	(*fd_count)--;
}


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
				{
					usleep(1000);
					continue;
				}
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

void broadcast(struct taskstruct* t)
{
	int sender = t->sender_fd;
	pthread_mutex_lock(&list_m);
	int fd_count = *(t->fd_count);
	int* fd_list = t->fd_list;
	for(int j = 1; j < fd_count ; j++)
	{
		if(fd_list[j] != sender)
		{
			if(sendall(fd_list[j],t->packet,t->len) <=0)
				continue;
		}
	}
	pthread_mutex_unlock(&list_m);
}
void* consumer(void* arg)
{
	while(1)
	{
		struct consumestruct *info = (struct consumestruct*)arg;
		printf("Worker %d is waiting for job..\n",info->tid);
		pthread_mutex_lock(&m);
		while(*(info->packet_num) == -1)
			pthread_cond_wait(&fill,&m);
		printf("Worker %d get work from queue!\n",info->tid);
		printf("Queue number : %d\n",*(info->packet_num));
		struct taskstruct* t = &((info->t_queue)[*(info->packet_num)]);
		int *modifypacketnum = info->packet_num;
		*modifypacketnum -= 1;
		pthread_mutex_unlock(&m);
		pthread_cond_signal(&empty);
		broadcast(t);
		printf("Worker %d done with broadcasting.\n",info->tid);
	}

}
int main()
{
	int fd_size = 5;
	int fd_count = 0;

	int *fdlist = malloc(sizeof (int) * fd_size);
	struct taskstruct task_queue[MAXBUFFERSIZE];
	int packet_num = -1;
	int listener_fd;
	int rv;
	struct addrinfo hints,*servinfo,*p;
	struct epoll_event ev;
	struct epoll_event events[MAXEVENTS];
	if(pthread_mutex_init(&m,NULL)==-1)
	{
		perror("pthread_mutex_init:");
		exit(1);
	}
	if(pthread_mutex_init(&list_m,NULL)==-1)
	{
		perror("pthread_mutex_init:");
		exit(1);
	}
	if(pthread_cond_init(&fill,NULL)==-1)
	{
		perror("pthread_cond_init:fill:");
		exit(1);
	}
	if(pthread_cond_init(&empty,NULL)==-1)
	{
		perror("pthread_cond_init:empty:");
		exit(1);
	}

	socklen_t clientaddrlen;
	int yes = 1;
	memset(&hints,0,sizeof hints);
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	if((rv = getaddrinfo(NULL,PORT,&hints,&servinfo))==-1)
	{
		fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(rv));
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
	if(add_to_fdlist(listener_fd,&fdlist,&fd_count,&fd_size)!=0)
	{
		fprintf(stderr,"fail to get fdlist.\n");
		exit(1);
	}

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
	pthread_t worker[4];
	struct consumestruct worker_info[4];
	for(int i = 0; i < 4 ; i++)
	{
		worker_info[i].tid = i;
		worker_info[i].packet_num = &packet_num;
		worker_info[i].t_queue = task_queue;
		pthread_create(&worker[i],NULL,consumer,(void*)&worker_info[i]);
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
			if(events[i].data.fd == listener_fd)
			{
				while(1)
				{
					char clientip[INET6_ADDRSTRLEN];
					struct sockaddr_storage clientaddr;
					socklen_t clientaddrlen = sizeof clientaddr;
					int newfd = accept(listener_fd,(struct sockaddr*)&clientaddr,&clientaddrlen);
					if(newfd == -1)
					{
						if(errno == EAGAIN ||errno == EWOULDBLOCK)
						{break;}
						else
						{
							perror("accept");
							break;
						}
					}
					fcntl(newfd,F_SETFL,O_NONBLOCK);
					pthread_mutex_lock(&list_m);
					add_to_fdlist(newfd,&fdlist,&fd_count,&fd_size);
					pthread_mutex_unlock(&list_m);
					ev.events = EPOLLIN | EPOLLET;
					ev.data.fd = newfd;
					inet_ntop(clientaddr.ss_family,get_in_addr((struct sockaddr*)&clientaddr),clientip,sizeof clientip);
					printf("Successfully connect to %s, socket number: %d\n",clientip,newfd);
					if(epoll_ctl(epfd,EPOLL_CTL_ADD,newfd,&ev)==-1)
					{
						perror("client epoll_clt");
						epoll_ctl(epfd,EPOLL_CTL_DEL,newfd,NULL);
						close(newfd);
					}
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
					pthread_mutex_lock(&list_m);
					perror("recv");
					epoll_ctl(epfd,EPOLL_CTL_DEL,sender_fd,NULL);
					close(sender_fd);
					delete_from_fdlist(&fdlist,i,&fd_count);
					pthread_mutex_unlock(&list_m);
					break;
				}
				else if(header_res == 0)
				{
					pthread_mutex_lock(&list_m);
					printf("Connection closed from socket %d\n",sender_fd);
					epoll_ctl(epfd,EPOLL_CTL_DEL,sender_fd,NULL);
					close(sender_fd);
					delete_from_fdlist(&fdlist,i,&fd_count);
					pthread_mutex_unlock(&list_m);
					break;
				}
				else
				{
					int totalsize = ntohs(nettotal);
					int bodysize = totalsize - 2;
					int offset = 2;
					printf("total packet size: %d,",totalsize);
					if(totalsize < 4)
					{
						pthread_mutex_lock(&list_m);
						fprintf(stderr,"Security alert: invaild packet size on %d\n",sender_fd);
						epoll_ctl(epfd,EPOLL_CTL_DEL,sender_fd,NULL);
						close(sender_fd);
						delete_from_fdlist(&fdlist,i,&fd_count);
						pthread_mutex_unlock(&list_m);
						break;
					}
					int body_res = recvall(sender_fd,packet+offset,bodysize);
					if(body_res <= 0)
					{
						pthread_mutex_lock(&list_m);
						fprintf(stderr,"Connection loss from %d\n",sender_fd);
						epoll_ctl(epfd,EPOLL_CTL_DEL,sender_fd,NULL);
						delete_from_fdlist(&fdlist,i,&fd_count);
						close(sender_fd);
						pthread_mutex_unlock(&list_m);
						break;

					}
					uint16_t netidlen;
					memcpy(&netidlen,packet+offset,2);
					int idlen = ntohs(netidlen);
					if(idlen > MAXIDLEN || offset + idlen > totalsize)
					{
						pthread_mutex_lock(&list_m);
						fprintf(stderr,"Security alert: invaild id length: %d on fd: %d\n",idlen,sender_fd);
						epoll_ctl(epfd,EPOLL_CTL_DEL,sender_fd,NULL);
						delete_from_fdlist(&fdlist,i,&fd_count);
						close(sender_fd);
						pthread_mutex_unlock(&list_m);
						break;
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

						pthread_mutex_lock(&list_m);
						fprintf(stderr,"Security alert: invaild data size: %d on fd %d\n:",datalen,sender_fd);
						epoll_ctl(epfd,EPOLL_CTL_DEL,sender_fd,NULL);
						delete_from_fdlist(&fdlist,i,&fd_count);
						close(sender_fd);
						pthread_mutex_unlock(&list_m);
						break;
					}
					offset += 2;
					char data[MAXDATALEN+1];
					memcpy(data,packet+offset,datalen);
					offset += datalen;
					data[datalen] = '\0';
					printf("%s : %s\n",id,data);
					memcpy(packet,&nettotal,2);
					pthread_mutex_lock(&m);
					while(packet_num == MAXBUFFERSIZE -1)
						pthread_cond_wait(&empty,&m);
					packet_num++;
					task_queue[packet_num].sender_fd = sender_fd;
					task_queue[packet_num].fd_list =fdlist;
					task_queue[packet_num].fd_count = &fd_count;
					memcpy(task_queue[packet_num].packet,packet,totalsize);
					task_queue[packet_num].len = totalsize;
					pthread_cond_signal(&fill);
					pthread_mutex_unlock(&m);

				}
				
			}
		}
	}
	
	
	
	
	
}
