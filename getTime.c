#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<unistd.h>

uint64_t probe;

void map_file(int pages){
    int *array, *array1,i;
    unsigned long addr, prev, retVal;
    int fd = open("4kb.file", O_RDWR);
    if(fd == -1){
	printf("4kb.file opening failed.\n");
	exit(-1);
    }
    //getchar();
    array = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    addr = (unsigned long) array;
    addr += 12288;
    prev = addr;
    printf("%p\n", array);
    array += 4096;
    probe = array;
    printf("%p, %x, %d\n", array, probe, sizeof(probe));
    return;
    for(i=0; i<=pages; i++){
	addr += 4096;
	array1 = mmap((void *)addr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if( array1 == MAP_FAILED){
	    printf("Mapping failed\n");
	    exit(-1);
	}
	printf("Requested=%p, Returned=%p, diff=%ld\n", (void *)addr, array1, (prev - (unsigned long)array1));
   	array1[1]=1;
	prev = (unsigned long)array1;
    }
    printf("%d\t%d\n", array1[2], array1[1]);
    array[1]=2;
    printf("%d\t%d\n", array1[2], array1[1]);
    //getchar();
    close(fd);
}
void print_time(){
    //int i, *array=(int *)probe;
    //unsigned long addr = (unsigned long) probe;
    int i;
    uint32_t time1,time2;
    //printf("%d %x\n", array[1], array);
    /* Training: Bring all the entries to the TLB */
    /*for(i=0; i<64; i++){
	array = (int *)addr;
	array[0]=i;
    }*/
    for(i=0; i<64; i++){
    asm volatile (
	"cpuid\n"
	"lfence\n"
	"cpuid\n"
	"rdtsc\n"
	"mov %%eax, %%edi\n"
	"mov $2, %2\n"
	"lfence\n"
	"rdtscp\n"
	"mov %%edi, %0\n"
	"mov %%eax, %1\n"
	"cpuid\n"
	: "=r" (time1), "=r" (time2)
	: "r" (probe)
	: "rax", "rbx", "rcx",
	"rdx", "rdi");
    printf("time1=%x, time2=%x, diff=%d\n", time1, time2, (time2-time1));
    }
   
}

int main(){
    int x=5;
    //map_file(64);
    probe = &x;
    print_time();
    printf("%d\n", x);
    return 0;
}
