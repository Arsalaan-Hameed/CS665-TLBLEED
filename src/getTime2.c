#define _GNU_SOURCE
#include<stdio.h>
#include<stdint.h>
#include<pthread.h>
#include<sched.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>

#include "getTime.h"

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
#define HASWELL 1

uint64_t array[50000];


struct node *allocate_buffer(unsigned long long p)
{
	assert(p >= 0);
	volatile char *target = (void *) (VTARGET+p*PAGE);
	volatile char *ret;
	//printf("[DEBUG] allocating buffer at address %p\n", target);
	ret = mmap((void *) target, PAGE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
	if(ret == MAP_FAILED) {
		perror("mmap");
           exit(1);
	}
	if(ret != (volatile char *) target) { fprintf(stderr, "Wrong mapping\n"); exit(1); }
    *ret;
	memset((char *) ret, 0x00, PAGE); /* RETQ instruction */
	return (struct node *)ret;
}



void evict_l1(){
    int i, j;
    for(i=0; i<L1_SETS; i++){
	for(j=0; j<L1_ASSOC; j++)
	    array[i + (j*uint64_size*L1_SETS)] = 0;
    }
    return;
}

struct node *create_evict_set1(int pages, int set_no){
    assert(pages == 4);
    int page_no, offset;
    struct node *head, *temp, *request;
    uint64_t mask;
    while(pages != 0){
	page_no = rand() % 65536;
	/*
	 * check if page is targetting an particular set in l1
	 */
	if((page_no % L1_DTLB_SETS) == set_no){
	    //printf("[DEBUG1] allocating page no %d\n", page_no);
	    request = allocate_buffer(page_no);
	    request->addr = NULL;
	    offset = rand() % 63;
	    offset = offset << 6;
	    mask = (uint64_t)request;
	    mask += offset;
	    request = (struct node *)mask;
	    //printf("[DEBUG5] mask %lx, offset: %x request addr: %p\n", mask, offset, request);
	    if(head == NULL)
		head = request;
	    else{
		request->addr = head;
		head = request;
	    }
	    pages--;
	}
	   
    }
    return head;
}


struct node *create_evict_set2(int pages, int set_no){
    assert(pages > 0);
    int page_no, offset, upper_bits, lower_bits, count, l2_set_no;
    uint64_t mask;
    struct node *head, *request;
    count = 0;
    while(pages != 0){
	page_no = rand() % 65536;
#ifdef BROADWELL
	mask =  (VTARGET+pages*PAGE) >> 12;
	lower_bits = mask & 0xFF;
	upper_bits = mask & 0xFF00;
	upper_bits = upper_bits >> 8;
	l2_set_no = upper_bits ^ lower_bits;
#endif
#ifdef KABYLAKE
	mask =  (VTARGET+pages*PAGE) >> 12;
	lower_bits = mask & 0x7F;
	upper_bits = mask & 0x3F40;
	upper_bits = upper_bits >> 7;
	l2_set_no = upper_bits ^ lower_bits;
#endif
#ifdef HASWELL
	l2_set_no = (page_no % L2_STLB_SETS); 
#endif
	if((page_no % L1_DTLB_SETS) == set_no && l2_set_no == set_no ){
	    //printf("[DEBUG] allocating page no %d\n", page_no);
	    request = allocate_buffer(page_no);
	    request->addr = NULL;
	    offset = rand() % 63;
	    offset = offset << 6;
	    mask = (uint64_t)request;
	    mask += offset;
	    request = (struct node *)mask;
	    //printf("[DEBUG] adding address %p to list 2\n", request);
	    if(head == NULL){
		count++;
		head = request;
	    }
	    else{
		request->addr = head;
		head = request;
		count++;
	    }
	    pages--;
	}
    }
    printf("[DEBUG] ");
    print_list(head);
    printf("[DEBUG] Pages: %d, count: %d\n", pages, count);
    return head;
}

static inline uint32_t profile_time_template(uint64_t probe){
    uint32_t time;
    asm volatile(
	    "lfence\n"
	    "rdtsc\n"
	    "mov %%eax, %%edi\n"
	    "mov (%1), %1\n"
	    "mov (%1), %1\n"
	    "mov (%1), %1\n"
	    "mov (%1), %1\n"
	    "lfence\n"
	    "rdtscp\n"
	    "sub %%edi, %%eax\n"
	    "mov %%eax, %0\n"
	    : "=r" (time)
	    : "r" (probe)
	    : "rax", "rbx", "rcx", "rdx", "rdi");
    return time;
}

void *pin_cpu(void *arg){
    int arg_value = *(int *)arg;
    pthread_t curr = pthread_self();
    //pid_t pid = getpid();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(arg_value, &cpuset);
    if(pthread_setaffinity_np(curr, sizeof(cpu_set_t), &cpuset)){
	fprintf(stderr, "[ERROR] scheduling affinity for arg: %d (thread: %lu) failed\n", arg_value, curr);
    }
    CPU_ZERO(&cpuset);
    if(pthread_getaffinity_np(curr, sizeof(cpu_set_t), &cpuset)){
	fprintf(stderr, "[ERROR] retriving affinity for arg: %d (thread: %lu) failed\n", arg_value, curr);
    }
    if(CPU_ISSET(arg_value, &cpuset))
	fprintf(stdout, "[DEBUG] cpu affinity successfully set for arg: %d (thread: %lu)\n", arg_value, curr);
    else
	fprintf(stdout, "[DEBUG] cpu affinity not correctly set for arg: %d (thread: %lu)\n", arg_value, curr);
    if(arg_value == 1){
	evict_l1();
	//profile_time();
    }
    return NULL;
}


int main(){
    int core1, core5, set_no, access_time[100];
    struct node *head_set1, *temp, *head_set2, *head_set3;
    uint64_t probe;
    uint32_t time;
    //probe = allocate_pages(PAGES);
    pthread_t thread1;
    core1 = 1;
    core5 = 5;
    set_no = 2;
    if(pthread_create(&thread1, NULL, pin_cpu, &core1)){
	fprintf(stderr, "Error creating thread\n");
	return 1;
    }
    pin_cpu(&core5);
    head_set1 = create_evict_set1(L1_DTLB_ASSOC, set_no);
    head_set2 = create_evict_set2(6, set_no);
    head_set3 = create_evict_set2(9, set_no);

    printf("[OUT] Set1: ");
    print_list(head_set1);
    printf("[OUT] Set2: ");
    print_list(head_set2);
    printf("[OUT] Set3: ");
    print_list(head_set3);

    probe = (uint64_t)head_set1;
    printf("[OUT] Access Time for first Set:\t");
    evict_l1();
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    for(int i=0; i<100; i++){
	PROFILE_TIME_START
	PROFILE_TIME_SET1
	PROFILE_TIME_END
    }
    print_array(access_time, 100);

    printf("[OUT] Access Time for second Set:\t");
    probe = (uint64_t)head_set2;
    evict_l1();
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    for(int i=0; i<100; i++){
	PROFILE_TIME_START
#ifdef BROADWELL
	PROFILE_TIME_HASWELL_SET2
#endif
#ifdef KABYLAKE
	PROFILE_TIME_KABYLAKE_SET2
#endif
#ifdef HASWELL
	PROFILE_TIME_HASWELL_SET2
#endif
	PROFILE_TIME_END
    }
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    print_array(access_time, 100);


    printf("[OUT] Access Time for Third Set:\t");
    probe = (uint64_t)head_set3;
    evict_l1();
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    for(int i=0; i<100; i++){
	PROFILE_TIME_START
#ifdef BROADWELL
	PROFILE_TIME_HASWELL_SET3
#endif
#ifdef KABYLAKE
	PROFILE_TIME_KABYLAKE_SET3
#endif
#ifdef HASWELL
	PROFILE_TIME_HASWELL_SET3
#endif
	PROFILE_TIME_END
    }
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    print_array(access_time, 100);

    if(pthread_join(thread1, NULL)){
	fprintf(stderr, "Error joining thread\n");
	return 2;
    }
    return 0;
}
