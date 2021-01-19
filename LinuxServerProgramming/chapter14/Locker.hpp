#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//封装信号量
class Sem
{
				public:
								Sem()
								{
												if(sem_init(&mSem, 0, 0)!=0)
																throw std::exception();
								}
								virtual ~Sem()
								{
												sem_destroy(&mSem);
								}

								bool wait()
								{
												return sem_wait(&mSem)==0;
								}
								bool post()
								{
												return sem_post(&mSem)==0;
								}
				private:
								sem_t mSem;
};

//互斥锁
class Locker
{
				public:
								Locker()
								{
												if(pthread_mutex_init(&mMutex, NULL)!=0)
																throw std::exception();
								}
								virtual ~Locker()
								{
												pthread_mutex_destroy(&mMutex);
								}
								bool lock()
								{
												return pthread_mutex_lock(&mMutex)==0;
								}
								bool unlock()
								{
												return pthread_mutex_unlock(&mMutex)==0;
								}
				private:
								pthread_mutex_t mMutex;
};

//条件变量
class Cond
{
				public:
								Cond()
								{
												if(pthread_mutex_init(&mMutex, NULL)!=0)
																throw std::exception();
												if(pthread_cond_init(&mCond, NULL)!=0)
												{
																pthread_mutex_destroy(&mMutex);
																throw std::exception();
												}
								}
								virtual ~Cond()
								{
												pthread_mutex_destroy(&mMutex);
												pthread_cond_destroy(&mCond);
								}
								bool wait()
								{
												int res=0;
												pthread_mutex_lock(&mMutex);
												res=pthread_cond_wait(&mCond, &mMutex);
												pthread_mutex_unlock(&mMutex);
												return res==0;
								}
								bool signal()
								{
												return pthread_cond_signal(&mCond)==0;
								}
				private:
								pthread_mutex_t mMutex;
								pthread_cond_t mCond;
};

#endif
