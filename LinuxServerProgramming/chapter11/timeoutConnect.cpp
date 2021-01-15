#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

int timeoutConnect(const char *ip, int port, int time)
{
				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int sockfd=socket(PF_INET, SOCK_STREAM, 0);
				assert(sockfd>=0);

				struct timeval timeout;
				timeout.tv_sec=time;
				timeout.tv_usec=0;
				socklen_t len=sizeof(timeout);
				int res=setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
				assert(res!=-1);

				res=connect(sockfd, (sockaddr*)&serverAddr, sizeof(serverAddr));
				if(res==-1)
				{
								//超时对应的错误为EINPROGRESS
								if(errno==EINPROGRESS)
								{
												printf("connecting timeout, process timeout logic\n");
												return -1;
								}
								printf("error occur when connecting to server\n");
								return -1;
				}
				return sockfd;
}

int main(int argc, char *argv[])
{
				if(argc<3)
				{
								printf("ip port\n");
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);

				int sockfd=timeoutConnect(ip, port, 10);
				if(sockfd<0)
				{
								return 1;
				}
				char buf[1024];
				memset(buf, 'a', 1023);
				buf[1023]='\0';
				send(sockfd, buf, 1023, 0);
				memset(buf, '\0', 1024);
				recv(sockfd, buf, 1023, 0);
				printf("recv %s\n", buf);
				close(sockfd);
				return 0;
}
