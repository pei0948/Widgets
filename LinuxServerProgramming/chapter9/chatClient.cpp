#define _GNU_SOURCE 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <string.h>

#define BUF_SIZE 64

int main(int argc, char *argv[])
{
				if(argc<3)
				{
								printf("ip port\n");
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);

				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int sockfd=socket(PF_INET, SOCK_STREAM, 0);
				assert(sockfd>=0);

				if(connect(sockfd, (sockaddr*)&serverAddr, sizeof(serverAddr))<0)
				{
								printf("connection fails\n");
								close(sockfd);
								return 1;
				}

				pollfd fds[2];
				fds[0].fd=0;//标准输入
				fds[0].events=POLLIN;
				fds[0].revents=0;
				fds[1].fd=sockfd;
				fds[1].events=POLLIN | POLLRDHUP;
				fds[1].revents=0;

				char readBuf[BUF_SIZE];
				int pipefd[2];
				int res=pipe(pipefd);
				assert(res!=-1);

				while(1)
				{
								res=poll(fds, 2, -1);
								if(res<0)
								{
												printf("poll fails\n");
												break;
								}
								if(fds[1].revents & POLLRDHUP)
								{
												printf("server closes the connection\n");
												break;
								}
								else if(fds[1].revents & POLLIN)
								{
												memset(readBuf, '\0', BUF_SIZE-1);
												res=recv(fds[1].fd, readBuf, BUF_SIZE-1, 0);
												printf("recv %d bytes: %s\n", res, readBuf);
								}
								if(fds[0].revents & POLLIN)
								{
												res=splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
												res=splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
								}
				}
				close(sockfd);
				return 0;
}

