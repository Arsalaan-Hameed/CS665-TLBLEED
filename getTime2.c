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
#define L1_WAYS 8
#define L1_DTLB_SETS 16
#define L1_DTLB_WAY 4
#define L2_STLB_SETS 128
#define L2_STLB_WAY 8
#define uint64_size 8
#define PAGES 4096
#define PAGE 4096
#define VTARGET 0x300000000000ULL

uint64_t array[50000];
int sync_var = 0;

struct node{
    struct node *addr;
};

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
	for(j=0; j<L1_WAYS; j++)
	    array[i + (j*uint64_size*L1_SETS)] = 0;
    }
    return;
}
/*
 * old implementation
struct node{
    struct node *next;
};
struct node *create_evict_set1(){
    struct node *freelist, *head, *temp;
    uint64_t mask = 0xfffffffffffff000;
    freelist = NULL;
    head = NULL;
    temp = (struct node *)malloc(sizeof(struct node));
    mask &= (uint64_t)temp;
    return head;
}
*/

struct node *create_evict_set1(int pages, int set_no){
    assert(pages == 4);
    int page_no, offset;
    struct node *head, *temp, *request;
    uint64_t mask;
   mask = 0xffffffffffffffc0;
    while(pages != 0){
	mask = 0xffffffffffffffc0;
	page_no = rand() % 65536;
	/*
	 * check if page is targetting an particular set in l1
	 */
	if((page_no % L1_DTLB_SETS) == set_no){
	    printf("[DEBUG1] allocating page no %d\n", page_no);
	    request = allocate_buffer(page_no);
	    printf("[DEBUG2] adding address %p to list 1\n", request);
	    request->addr = NULL;
	    mask &=  (uint64_t)request;
	    printf("[DEBUG3] mask: %lx \n", mask);
	    mask = mask >> 6; 
	    printf("[DEBUG4] mask %lx \n", mask);
	    offset = rand() % 63;
	    mask = mask + offset;
	    request += offset;
	    printf("[DEBUG5] mask %lx \n", mask);
	    //printf("[DEBUG3] adding address %lx to list 1\n", mask);
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
    int page_no, offset;
    struct node *head, *temp, *request;
    while(pages != 0){
	page_no = rand() % 65536;
	/*
	 * check if page is targetting an particular set in l1
	 */
	if((page_no % L1_DTLB_SETS) == set_no && (page_no % L2_STLB_SETS) == set_no ){
	    //printf("[DEBUG] allocating page no %d\n", page_no);
	    request = allocate_buffer(page_no);
	    request->addr = NULL;
	    offset = rand() % 511;
	    request += offset;
	    //printf("[DEBUG] adding address %p to list 2\n", request);
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

static inline uint32_t profile_time(uint64_t probe){
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

void *start_func(void *arg){
    int arg_value = *(int *)arg;
    pthread_t curr = pthread_self();
    //pid_t pid = getpid();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(arg_value, &cpuset);
    if(pthread_setaffinity_np(curr, sizeof(cpu_set_t), &cpuset)){
	fprintf(stderr, "scheduling affinity for arg: %d (thread: %lu) failed\n", arg_value, curr);
    }
    CPU_ZERO(&cpuset);
    if(pthread_getaffinity_np(curr, sizeof(cpu_set_t), &cpuset)){
	fprintf(stderr, "retriving affinity for arg: %d (thread: %lu) failed\n", arg_value, curr);
    }
    if(CPU_ISSET(arg_value, &cpuset))
	fprintf(stdout, "cpu affinity successfully set for arg: %d (thread: %lu)\n", arg_value, curr);
    else
	fprintf(stdout, "cpu affinity not correctly set for arg: %d (thread: %lu)\n", arg_value, curr);
    if(arg_value == 1){
	evict_l1();
	sync_var = 1;
	//profile_time();
    }
    return NULL;
}

void print_list(struct node *temp){
    uint64_t mask = 0xffffffffffffffc0, var;
    var  = (uint64_t)temp;
    var = var >> 6;
    while(temp != NULL){
	printf("%p(%lu)", temp, var%64);
	temp = temp->addr;
	if(temp != NULL)
	    printf("->");
    }
    printf("\n");
    return;
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
    if(pthread_create(&thread1, NULL, start_func, &core1)){
	fprintf(stderr, "Error creating thread\n");
	return 1;
    }
    start_func(&core5);
    head_set1 = create_evict_set1(L1_DTLB_WAY, set_no);
    set_no = 3;
    head_set2 = create_evict_set2(6, set_no);
    set_no = 4;
    head_set3 = create_evict_set2(9, set_no);

    printf("[DEBUG] Set1: ");
    print_list(head_set1);

    printf("[DEBUG] Set2: ");
    print_list(head_set2);

    printf("[DEBUG] Set3: ");
    print_list(head_set3);

    while(sync_var != 1);
    probe = (uint64_t)head_set1;
    printf("Access Time for first Set:\t");
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
		"lfence\n"
		"rdtscp\n"
		"sub %%edi, %%eax\n"
		"mov %%eax, %0\n"
		: "=r" (access_time[i])
		: "r" (probe)
		: "rax", "rbx", "rcx", "rdx", "rdi");
    }
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    for(int i=0; i<100; i++)
	printf("%d\t", access_time[i]);
    printf("\n");

    printf("\n");
    printf("Access Time for second Set:\t");
    probe = (uint64_t)head_set2;
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
		"lfence\n"
		"rdtscp\n"
		"sub %%edi, %%eax\n"
		"mov %%eax, %0\n"
		: "=r" (access_time[i])
		: "r" (probe)
		: "rax", "rbx", "rcx", "rdx", "rdi");
	//access_time[i] = profile_time2(probe);
    }
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    for(int i=0; i<100; i++)
	printf("%d\t", access_time[i]);
    printf("\n");



    printf("\n");
    printf("Access Time for Third Set:\t");
    probe = (uint64_t)head_set3;
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
		: "=r" (access_time[i])
		: "r" (probe)
		: "rax", "rbx", "rcx", "rdx", "rdi");
	//access_time[i] = profile_time2(probe);
    }
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    for(int i=0; i<100; i++)
	printf("%d\t", access_time[i]);
    printf("\n");

    if(pthread_join(thread1, NULL)){
	fprintf(stderr, "Error joining thread\n");
	return 2;
    }
    return 0;
}
