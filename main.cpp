#include <cstdio>
#include <pthread.h>
#include <unistd.h>
#include "Thread_pool.h"


void  TaskFunc(void* arg)
{
    int num = *(int*)arg;
    printf("thread %ld is woring, number = %d \n",
        pthread_self(), num);
    sleep(1);
}

int main()
{
    //创建线程池 
    Thread_pool pool(3, 10);

    for (int i = 0; i < 100; ++i)
    {
        int* num = new int(i + 100);//初始化为i+100
        pool.addTask(Task(TaskFunc, num));                                                                                                                         
    }
    sleep(20);
    return 0;
}