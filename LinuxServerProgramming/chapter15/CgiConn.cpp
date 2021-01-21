#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "ProcessPool.hpp"

class CgiConn
{
				public:
								CgiConn(){}
								virtual ~CgiConn(){}
								void init(int epollFd, int sockFd, const sockaddr_in &clientAddr)
								{
												mEpollFd=epollFd;
												mSockFd=sockFd;
												mAddress=clientAddr;
												
												memset(mBuf, '\0', BUF_SIZE);
												mReadIdx=0;
								}

								void process()
								{
												int idx=0;
												int res=-1;
												while(true)
												{
																idx=mReadIdx;
																res=recv(mSockFd, mBuf+idx, BUF_SIZE-1-idx, 0);
																if(res<0)
																{
																				if(errno!=EAGAIN)
																								removeFd(mEpollFd, mSockFd);
																				break;
																}
																else if(res==0)
																{
																				removeFd(mEpollFd, mSockFd);
																				break;
																}
																else
																{
																				mReadIdx+=res;
																				printf("user content is %s\n", mBuf);
																				for(; idx<mReadIdx; ++idx)
																								if(idx>=1 && (mBuf[idx-1]=='\r') && mBuf[idx]=='\n')
																												break;
																				if(idx==mReadIdx)
																								continue;
																				mBuf[idx-1]='\0';

																				char* fileName=mBuf;
																				
																				//判断客户要运行的CGI程序是否存在
																				if(access(fileName, F_OK)==-1)
																				{
																								removeFd(mEpollFd, mSockFd);
																								break;
																				}
																				res=fork();
																				if(res==-1)
																				{
																								removeFd(mEpollFd, mSockFd);
																								break;
																				}
																				else if(res>0)
																				{
																								removeFd(mEpollFd, mSockFd);
																								break;
																				}
																				else
																				{
																								close(STDOUT_FILENO);
																								dup(mSockFd);
																								execl(mBuf, mBuf, NULL);
																								exit(0);
																				}
																}
												}
								}
				private:
								static const int BUF_SIZE=1024;
								static int mEpollFd;
								int mSockFd;
								sockaddr_in mAddress;
								char mBuf[BUF_SIZE];
								int mReadIdx;
};

int CgiConn::mEpollFd=-1;

int main(int argc, char* argv[])
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

				int listenSock=socket(PF_INET, SOCK_STREAM, 0);
				assert(listenSock>=0);

				int res=bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenSock, 5);
				assert(res!=-1);

				ProcessPool<CgiConn> *pool=ProcessPool<CgiConn>::create(listenSock);
				if(pool!=nullptr)
				{
								pool->run();
								delete pool;
				}
				close(listenSock);//谁创建的,谁负责销毁
				return 0;
}
