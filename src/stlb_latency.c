#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "latency.h"

#define L1_SETS 64
#define L1_ASSOC 8
#define PAGE 4096
#define uint64_size 8
#define VTARGET 0x300000000000ULL
#define HASWELL 1
#define KABYLAKE 2
#define BROADWELL 3

/*
 * Author: Mohd Arsalaan Hameed, Aditya Rohan
 */

uint64_t array[50000];  /* array for thrashing data cache */


/* mmap the file by fd to virtual page number p */
uint64_t *allocate_buffer(unsigned long long p, int fd, int page_no)
{
	assert(p >= 0);
	volatile char *target = (void *) (VTARGET+p*PAGE);
	volatile char *ret;
    uint64_t *access_ptr;
	//printf("[DEBUG] allocating buffer at address %p\n", target);
	ret = mmap((void *) target, PAGE, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_FILE, fd, 0);
	if(ret == MAP_FAILED) {
		perror("mmap");
           exit(1);
	}
	if(ret != (volatile char *) target) { fprintf(stderr, "Wrong mapping\n"); exit(1); }
    access_ptr = (uint64_t *)ret;
    if(page_no == 0)
        memset((char *) ret, 0x00, PAGE); /* RETQ instruction */
    //*(access_ptr + page_no) = (uint64_t)(target) + page_no*sizeof(uint64_t);
	return (uint64_t *)ret;
}


/* Evict l1 by access very large array */
void evict_l1(){
    int i, j;
    for(i=0; i<L1_SETS; i++){
        for(j=0; j<L1_ASSOC; j++)
            array[i + (j*uint64_size*L1_SETS)] = 0;
    }
    return;
}


/* calclate l2 set number for a virtual page number based on architecture */
int calc_set(int page_no, int pages, int arch){
    int set_no, lower_bits, upper_bits;
    uint64_t mask;
    if(arch == HASWELL)
        set_no = page_no % 128;
    else if(arch == BROADWELL){
        mask =  (VTARGET+pages*PAGE) >> 12;
        lower_bits = mask & 0xFF;
        upper_bits = mask & 0xFF00;
        upper_bits = upper_bits >> 8;
        set_no = upper_bits ^ lower_bits;
    }
    else if(arch == KABYLAKE){
        page_no = rand()%21626930;
        mask =  (VTARGET+page_no*PAGE) >> 12;
        lower_bits = mask & 0x7F;
        upper_bits = mask & 0x3F80;
        upper_bits = upper_bits >> 7;
        set_no = upper_bits ^ lower_bits;
    }
    return set_no;
}


/* Create eviction set for l2 stlb based on architechure */
uint64_t *create_evict_set2(int pages, int set_no, int arch, int fd){
    assert(pages > 0);
    int upper_bits, lower_bits, count, l2_set_no;
    int page_no, offset, curr_page;
    uint64_t *head, *request;
    count = 0;
    curr_page  = 0;

    while(pages != 0){
        page_no = rand() % 65536;
        l2_set_no = calc_set(page_no, pages, arch);

        if( l2_set_no == set_no ){
            //printf("[DEBUG] allocating page no %d\n", page_no);
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


/* Print access time in summarize form */
void print_access_summary(int *access_time, int size){
    int count[1000] = {0};
    for (int i=0 ; i < size; i++)
        if(access_time[i] < 1000)
            count[access_time[i]]++;

    printf("[latency]:\tcount\n");
    for (int i=0; i < 1000; i++){
        if ( count[i] !=0 ){
            printf("[%d]:\t\t%d\n", i, count[i]);
        }
    } 
}


int main(int argc, char *argv[]){
    int outputFlag;
    if(argc != 3){
    	printf("[ERROR] Incorrect number of arguments!\n"
                "Run program as: ./stlb_latency outputFlag arch\n"
                "Parameters\n"
                "outputflag: 1- write output to terminal\n"
                "            0- write output to output directory\n"
                "arch: 1- HASWELL\n"
                "      2- KABYLAKE\n"
                "      3- BROADWELL\n"
                "e.g. ./stlb_latency 1 1\n");
        exit(1);
    	outputFlag = 1;
    }
    else
        outputFlag = atoi(argv[1]);
    assert(outputFlag == 1 || outputFlag == 0);


    int fd, core_num,  set_no , access_time[100], access_time1[100000];
    uint64_t *probe;

    fd = open("4kb.file", O_RDWR);
    if(fd == -1){
        printf("4kb.file opening failed. create a file named \"4kb.file\" of size 4kb and run again\n");
        exit(-1);
    }

    core_num = 5;
    pin_cpu(core_num);
    set_no = 8;
    probe = create_evict_set2(9, set_no, 1, fd);

    printf("Allocated address: %lx\t", (uint64_t)probe);
    for (int i=0; i<8; i++){
        printf("%lx\t", probe[i]);
    }
    printf("\n");
    
    evict_l1();
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    for(int i=0; i<100; i++){
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
                "mov (%2), %2\n" // Accessing first virtual page number again
                "lfence\n"
                "rdtscp\n"
                "sub %%edi, %%eax\n"
                "mov %%eax, %0\n"
                : "=r" (access_time[i])
                : "r" (probe), "r" (probe)
                : "rax", "rbx", "rcx", "rdx", "rdi");
    }
    access_time[0] = 75;

    evict_l1();
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    for(int i=0; i<100; i++){
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
                    "mov (%1), %1\n"
                    "lfence\n"
                    "rdtscp\n"
                    "sub %%edi, %%eax\n"
                    "mov %%eax, %0\n"
                    : "=r" (access_time1[i])
                    : "r" (probe) 
                    : "rax", "rbx", "rcx", "rdx", "rdi");
    }
    asm volatile("cpuid" ::: "rax","rbx","rcx");

    printf("Access time for eviction set 1:\n");
    print_array(access_time, 100, 1, outputFlag);//Flag 1 for L1 hit
    print_access_summary(access_time, 100);
    printf("\n");

    printf("Access time for eviction set 2:\t");
    print_array(access_time1, 100, 2, outputFlag);//Flag 2 for L2 hit
    print_access_summary(access_time1, 100);
    printf("\n");


    return 0;
}
