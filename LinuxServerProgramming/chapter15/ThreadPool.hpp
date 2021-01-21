#ifndef THREAD_H
#define THREAD_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../chapter14/Locker.hpp"

template <typename T>
class ThreadPool
{
				public:
								ThreadPool(int threadNum=8, int maxReqNum=10000);
								virtual ~ThreadPool();

								bool append(T *req);
				private:
								static void* worker(void* arg);
								void run();
				private:
								int mThreadNum;//线程池中的线程数量
								int mMaxReqNum;//请求队列中允许的最大请求数
								pthread_t* mThreads;//线程数组
								std::list<T*> mWorkQueues;//请求队列
								Locker mQueueLocker;//保护请求队列的互斥锁
								Sem mQueueStat;//是否有任务需要处理
								bool mStop;//是否结束线程
};

template <typename T>
static void* worker(void* arg)
{
				ThreadPool<T> *pool=(ThreadPool<T>*)arg;
				pool->run();
				return pool;
}

template <typename T>
void ThreadPool<T>::run()
{
				while(!mStop)
				{
								mQueueStat.wait();
								mQueueLocker.lock();
								if(mWorkQueues.empty())
								{
												mQueueLocker.unlock();
												continue;
								}
								T* req=mWorkQueues.front();
								mWorkQueues.pop_front();
								mWorkQueues.unlock();
								if(!req)
												continue;
								req->process();
				}
}

template <typename T>
ThreadPool<T>::ThreadPool(int threadNum, int maxReqNum):mThreadNum(threadNum), mMaxReqNum(maxReqNum), mThreads(nullptr), mStop(false)
{
				if(mThreadNum<=0 || mMaxReqNum<=0)
								throw std::exception();
				mThreads=new pthread_t[mThreadNum];
				if(mThreads==nullptr)
								throw std::exception();
				for(int i=0; i<mThreadNum; ++i)
				{
								printf("create the %d thread\n", i);
								if(pthread_create(mThreads+i, NULL, worker, this)!=0)
								{
												delete []mThreads;
												throw std::exception();
								}
								if(pthread_detach(mThreads[i]))
								{
												delete []mThreads;
												throw std::exception();
								}
				}
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
				delete []mThreads;
				mStop=true;
}

template <typename T>
bool ThreadPool<T>::append(T *req)
{
				mQueueLocker.lock();
				if(mWorkQueues.size()>=mMaxReqNum)
				{
								mQueueLocker.unlock();
								return false;
				}
				mWorkQueues.push_back(req);
				mQueueLocker.unlock();
				mQueueStat.post();
				return true;
}


#endif
