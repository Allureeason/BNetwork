#ifndef __BNET_THREAD_POOL_H__
#define __BNET_THREAD_POOL_H__

#include<queue>
#include<pthread.h>
#include<mutex>
#include <iostream>
#include <string.h>
using namespace std;
//该类型是一个函数指针，指向一个接受void*参数且没有返回值的函数。
using callback = void(*)(void*);
struct Task
{
	//两个构造函数
	Task() 
	{
		function = nullptr;
		arg = nullptr;
	}
	Task(callback f, void* arg)
	{
		function = f;
		this->arg = arg;
	}

	callback function; //任务自带函数指针
	void* arg;
};

//任务队列类
class Taskqueue
{
public:
	Taskqueue();
	~Taskqueue();

	//添加任务 两种方式传递任务
	void addTask(Task& task); //引用加速
	void addTask(callback f, void* arg);

	//取出一个任务
	Task takeTask();
	//获取当前任务队列里面任务的数量
	inline size_t GetTaskNum() {
		return m_queue.size();
	}

private:
	pthread_mutex_t m_mutex; //互斥锁 
	std::queue<Task> m_queue;
};
class Thread_pool
{
public:
	Thread_pool(int min, int max);
	~Thread_pool();
	//添加任务
	void addTask(Task s);
	//获取活着的线程数量
	int GetAliveNum();
	// 获取忙的任务
	int GetBusyNumber();

private:
	// 工作的线程的任务函数
	static void* worker(void* arg);
	// 管理者线程的任务函数
	static void* manage(void* arg);
	//线程退出 函数
	void threadExit();

private:
	Taskqueue *m_taskQ;				//一个任务队列 表示线程池当前正在处理的任务队列
	pthread_mutex_t m_lock;			//一把线程池的锁
	pthread_cond_t m_notEmpty;		//一个表示当前任务队列不为空的信号量
	pthread_t* m_threadIDs;			//表示当前线程池中的线程的指针
	pthread_t  m_mangerID;			//一个管理者线程 动态的控制线程池中线程的数量
	int m_maxnum; 
	int m_minnum;
	int m_busynum;
	int m_alivenum;
	int m_exitnum;					//表示需要删除的线程数量
	bool isShutDown = false;		//表示当前线程是否需要被删掉
};



#endif // __BNET_THREADPOOL_H__
