static bool switch2user(uid_t userId, gid_t gId)//将root身份启动的进程切换为普通用户身份运行
{
				if(userId==0 && gId==0)
				{
								return false;
				}
				/*
				 * 确保当前用户为合法用户:root或者目标用户
				 * */
				gid_t currGId=getgid();
				uid_t currUId=getuid();
				if(((currGId!=0 || currUId!=0)
							&& (currGId!=gId || currUId!=userId))
				{
						return false;
				}
				if(currUId!=0)
				{
						return true;
				}

				//切换到目标用户
				if(setgid(gId)<0 || setuid(userId)<0)
				{
						return false;
				}
				return true;
}
