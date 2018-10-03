#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>
#include <iostream>
using namespace std;

/*
 * Create the eviction set.
 * Access them in loop.
 * Monitor the miss in dtlb by perf.
 */

void print_cpu()
{
    int cpuid_out;

    __asm__(
    "cpuid;"
        : "=b"(cpuid_out)
        : "a"(1)
        :);

    std::cout << "I am running on cpu " << std::hex << (cpuid_out >> 24) << std::dec << std::endl;
}

int main(){
    int child=0, flag=0;
    pid_t pid = getpid();
    if(fork()==0){
	child=1;
	pid = getpid();
	printf(" from child: %d, %d\n",pid, getpid());
    }
    else
	printf("%d, %d\n",pid, getpid());

   unsigned long mask;
    unsigned int len = sizeof(mask);
    if (sched_getaffinity(0, len, (cpu_set_t*)&mask) < 0){
	perror("sched_getaffinity");
	return -1;
    }
    printf("my affinity mask is: %08lx\n", mask);
    mask=4;
    if(sched_setaffinity(0, len, (cpu_set_t*)&mask) < 0){
	perror("sched_setaffinity");
    }
    if (sched_getaffinity(0, len, (cpu_set_t*)&mask) < 0){
	perror("sched_getaffinity");
	return -1;
    }
    printf("my affinity mask is: %08lx\n", mask);
    print_cpu();
   printf("Hello World\n"); 

   while(1);
    return 0;
}

