#include "Thread_pool.h"
#include <unistd.h>


Taskqueue::Taskqueue()
{
	pthread_mutex_init(&m_mutex, NULL);
}

Taskqueue::~Taskqueue()
{
	pthread_mutex_destroy(&m_mutex);
}

void Taskqueue::addTask(Task& task)
{
	pthread_mutex_lock(&m_mutex);
	m_queue.push(task);
	pthread_mutex_unlock(&m_mutex);
}

void Taskqueue::addTask(callback f, void* arg)
{
	pthread_mutex_lock(&m_mutex);

	Task task(f,arg);
	m_queue.push(task);

	pthread_mutex_unlock(&m_mutex);

}

Task Taskqueue::takeTask()
{
	Task t;
	pthread_mutex_lock(&m_mutex);
	if (m_queue.size() > 0)
	{
		t = m_queue.front();
		m_queue.pop();
	}
	pthread_mutex_unlock(&m_mutex);
	return t;

}

Thread_pool::Thread_pool(int min, int max)
{
	//实例化任务队列
	m_taskQ = new Taskqueue();
	//这里使用do while的好处是 任何一个初始化条件不满足的时候 可以直接break 不用使用goto
	do {
		//初始化线程池
		m_minnum = min;
		m_maxnum = max;
		m_busynum = 0;
		m_alivenum = min;

		// 根据线程的最大上限给线程数组分配内存
		m_threadIDs = new pthread_t[max];
		if (m_threadIDs == nullptr)
		{
			cout << "malloc thread_t[] 失败...." << endl;
			break;
		}
		//初始化
		memset(m_threadIDs, 0, sizeof(pthread_t) * max);
		//初始化互斥锁和条件变量
		if (pthread_mutex_init(&m_lock, NULL) != 0 ||
			pthread_cond_init(&m_notEmpty, NULL) != 0)
		{
			cout << "init mutex or  cond failed !" << endl;
			break;
		}

		/////////////////////////创建线程//////////////////////////////
		//根据最小线程个数 ，创建线程
		for (int i = 0; i < min; ++i)
		{
			pthread_create(&m_threadIDs[i], NULL, worker, this);
			cout << "---------create child thread----------，ID:" << to_string(m_threadIDs[i]) << endl;
		}
		//创建管理者线程 1个
		pthread_create(&m_mangerID, NULL, manage, this);
	} while (0);
}

Thread_pool::~Thread_pool()
{
	isShutDown = 1; 
	//销毁管理者线程 这里主线程等待管理者线程结束
	pthread_join(m_mangerID, NULL);
	//唤醒所有消费者线程
	for (int i = 0; i < m_alivenum; ++i)
	{
		pthread_cond_signal(&m_notEmpty);
	}
	if (m_taskQ) delete m_taskQ;
	if (m_threadIDs) delete[]m_threadIDs;
	pthread_mutex_destroy(&m_lock);
	pthread_cond_destroy(&m_notEmpty);
}

void Thread_pool::addTask(Task s)
{
	if (isShutDown)
	{
		return;
	}
	//添加任务不需要加锁 任务队列中有锁
	m_taskQ->addTask(s);
	//唤醒工作线程
	pthread_cond_signal(&m_notEmpty);
}

int Thread_pool::GetAliveNum()
{
	int threadNum = 0;
	pthread_mutex_lock(&m_lock);
	threadNum = m_alivenum;
	pthread_mutex_unlock(&m_lock);
	return threadNum;

}

int Thread_pool::GetBusyNumber()
{
	int threadNum = 0;
	pthread_mutex_lock(&m_lock);
	threadNum = m_busynum;
	pthread_mutex_unlock(&m_lock);
	return threadNum;
}


void* Thread_pool::worker(void* arg)
{
	Thread_pool* pool = static_cast<Thread_pool*>(arg);
	//work and work 工作线程访问任务队列来工作
	while (1)
	{
		//访问共享资源---队列
		pthread_mutex_lock(&(pool->m_lock));
		//判断任务队列是否为空 如果为空(但是线程池没有关闭) 阻塞工作线程阻塞
		while (pool->m_taskQ->GetTaskNum() == 0 && !pool->isShutDown)
		{
			//打印在等待的线程号
			cout << "thread" << to_string(pthread_self()) << "-------------------waiting-------------" << endl;
			//阻塞线程 
			//这里要去拿一把线程池的锁的原因 wait函数内部实现会释放一下这个锁在锁住
			//告诉cond信号量里面的一个值 当前阻塞队列 等待响应
			pthread_cond_wait(&pool->m_notEmpty, &pool->m_lock);
			
			//执行到这里的时候 线程被释放了 停滞阻塞了
			//这时查看是应为任务队列不空释放的 还是线程池关闭释放的 还是空闲太多需要有线程被释放
			if (pool->m_exitnum > 0)
			{
				pool->m_exitnum--;
				if (pool->m_alivenum > pool->m_minnum)
				{
					cout << "thread" << to_string(pthread_self()) << "-------------no task so Exit-------------" << endl;
					pool->m_alivenum--;
					pthread_mutex_unlock(&(pool->m_lock)); //释放前先把锁释放了
					pool->threadExit();//退出线程
				}
			}
		}
		//如果线程池关闭了
		if (pool->isShutDown)
		{
			pthread_mutex_unlock(& (pool ->m_lock));
			pool->threadExit();
		}

		// 从任务队列中取出任务
		Task task = pool->m_taskQ->takeTask();
		//工作线程+1
		pool->m_busynum++;
		// 线程池解锁  线程池公共资源访问已经结束
		pthread_mutex_unlock(&(pool->m_lock));
		//执行任务
		cout << " thread " << to_string(pthread_self()) << "starting working..." << endl;
		task.function(task.arg);
		delete task.arg;
		task.arg = nullptr;

		// 任务处理结束
		cout << "thread " << to_string(pthread_self()) << " end working...";
		pthread_mutex_lock(&pool->m_lock);
		pool->m_busynum--;
		pthread_mutex_unlock(&pool->m_lock);
	}
	return nullptr;
}

void* Thread_pool::manage(void* arg)
{
	Thread_pool* pool = static_cast<Thread_pool*>(arg);
	// 如果线程池没有关闭, 就一直检测
	while (!pool->isShutDown)
	{
		// 每隔5s检测一次
		sleep(5);
		// 取出线程池中的任务数和线程数量
		//  取出工作的线程池数量
		pthread_mutex_lock(&pool->m_lock);
		int queueSize = pool->m_taskQ->GetTaskNum();
		int liveNum = pool->m_alivenum;
		int busyNum = pool->m_busynum;
		pthread_mutex_unlock(&pool->m_lock);

		// 创建线程
		const int NUMBER = 2;
		// 当前任务个数>存活的线程数 && 存活的线程数<最大线程个数
		if (queueSize > liveNum && liveNum < pool->m_maxnum)
		{
			// 线程池加锁
			pthread_mutex_lock(&pool->m_lock);
			int num = 0;
			for (int i = 0; i < pool->m_maxnum && num < NUMBER
				&& pool->m_alivenum < pool->m_maxnum; ++i)
			{
				if (pool->m_threadIDs[i] == 0)
				{
					pthread_create(&pool->m_threadIDs[i], NULL, worker, pool);
					num++;
					pool->m_alivenum++;
				}
			}
			pthread_mutex_unlock(&pool->m_lock);
		}

		// 销毁多余的线程  唤醒线程 让他自杀
		// 忙线程*2 < 存活的线程数目 && 存活的线程数 > 最小线程数量
		if (busyNum * 2 < liveNum && liveNum > pool->m_minnum)
		{
			pthread_mutex_lock(&pool->m_lock);
			pool->m_exitnum = NUMBER;
			pthread_mutex_unlock(&pool->m_lock);
			for (int i = 0; i < NUMBER; ++i)
			{
				pthread_cond_signal(&pool->m_notEmpty);
			}
		}
	}
	return nullptr;
}

void Thread_pool::threadExit()
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < m_maxnum; ++i)
	{
		if (m_threadIDs[i] == tid)
		{
			cout << "threadExit() function: thread "
				<< to_string(pthread_self()) << " exiting..." << endl;
			m_threadIDs[i] = 0;
			break;
		}
	}
	pthread_exit(NULL);
}


