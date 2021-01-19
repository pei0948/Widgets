#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <algorithm>

static const int CONTROL_LEN=CMSG_LEN(sizeof(int));

//发送文件描述符
void sendFd(int fd, int fdToSend)
{
				iovec iov[1];
				msghdr msg;
				char buf[0];

				iov[0].iov_base=buf;
				iov[0].iov_len=1;
				msg.msg_name=NULL;
				msg.msg_namelen=0;
				msg.msg_iov=iov;
				msg.msg_iovlen=1;

				cmsghdr cm;
				cm.cmsg_len=CONTROL_LEN;
				cm.cmsg_level=SOL_SOCKET;
				cm.cmsg_type=SCM_RIGHTS;
				*(int*)CMSG_DATA(&cm)=fdToSend;
				msg.msg_control=&cm;
				msg.msg_controllen=CONTROL_LEN;
				
				sendmsg(fd, &msg, 0);
}

//接收目标文件描述符
int recvFd(int fd)
{
				iovec iov[1];
				msghdr msg;
				char buf[0];

				iov[0].iov_base=buf;
				iov[0].iov_len=1;
				msg.msg_name=NULL;
				msg.msg_namelen=0;
				msg.msg_iov=iov;
				msg.msg_iovlen=1;

				cmsghdr cm;
				msg.msg_control=&cm;
				msg.msg_controllen=CONTROL_LEN;

				recvmsg(fd, &msg, 0);

				int fdRecv=*(int*)CMSG_DATA(&cm);

				return fdRecv;
}

int main(int argc, char* argv[])
{
				int pipeFd[2];
				int fdToPass=0;

				int res=socketpair(PF_UNIX, SOCK_STREAM, 0, pipeFd);
				assert(res!=-1);

				pid_t pid=fork();
				assert(pid>=0);

				if(pid==0)
				{
								close(pipeFd[0]);
								fdToPass=open("test.txt", O_RDWR, 0666);
								sendFd(pipeFd[1], std::max(fdToPass, 0));
								close(fdToPass);
								exit(0);
				}

				close(pipeFd[1]);
				fdToPass=recvFd(pipeFd[0]);

				char buf[1024];
				memset(buf, '\0', 1024);
				read(fdToPass, buf, 1023);
				printf("I got fd %d and data %s\n", fdToPass, buf);

				close(fdToPass);
				return 0;
}
