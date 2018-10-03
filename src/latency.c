#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#define SIZE 1024*512

struct node {
    size_t data; 
};

char array[5000];
int main (int argc, char** argv)
{
    int x = 0;
    long unsigned int start, end;
    uint64_t a, d, e=0, f=0;

    array[0] = 0;
    for(int i = 0; i < 256; i++){
        asm volatile ("cpuid");
        asm volatile ("lfence");
        asm volatile ("rdtscp");
	asm volatile("mov %%rax, %%rdi"::: "rax");
        asm volatile ("rdtsc" : "=a" (a), "=d" (d));
	asm volatile ("mov %%rdi,%0":"=r"(e));
        asm volatile ("lfence");
        end = (d<<32) | a;
        start = (d << 32) | e;
        printf("block %d: %lu\n",array[0], (end-start));
	array[0] +=1;
    }

	return 0;
}
