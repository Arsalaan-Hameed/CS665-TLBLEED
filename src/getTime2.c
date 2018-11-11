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
//#define KABYLAKE 1

uint64_t array[50000];

int target = 0;
int target1 = 0;
pthread_mutex_t lock;
int sharedVar = -1;
uint64_t probeX;

int target_add, target_mul;
// &target = 0x555555755014, Set no: 5 ( for 16 sets L1 dtlb)


void map_file(int pages){
    int *array, *array1, i, diff=-1;
    unsigned long request, recieved;
    int fd = open("4kb.file", O_RDWR);
    if(fd == -1){
	printf("4kb.file opening failed.\n");
	exit(-1);
    }
    //array = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    array = mmap(NULL, pages*4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    request = (unsigned long) array;
    probeX = (uint64_t)array;    // Set the probe variable to address of the starting virtual address for continuous pages
    // DEBUG: printf("%p, %lx, %d\n", array, probe, (int)sizeof(probe));
    for(i=0; i<pages; i++){
	if(munmap((void *)request, 4096) == -1){
	    printf("Error for mapping of page no %d at address %lx\n", i, request);
	    close(fd);
	    exit(-1);
	}
	array1 = mmap((void *)request, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if( array1 == MAP_FAILED){
	    printf("Error for mapping of page no %d at address %lx\n", i, request);
	    close(fd);
	    exit(-1);
	}
	recieved = (unsigned long)array1;
	diff = recieved - request;
	// Check for diff value, if diff is not 4096 abort the execution as the virtual address are not contiguous.
	if(diff != 0){
	    printf("Contiguous page allocation failed, Requested Addr = %lx, Returned Addr = %lx, Page no = %d. EXITING\n", request, recieved, i+1);
	    close(fd);
	    exit(-1);
	}
	// DEBUG: printf("Requested=%p, Returned=%p, diff=%d\n", (void *)request, array1, diff);
   	//array1[1]=1;
	request += 4096;
    }
    //getchar();
    close(fd);
}

void add(char ch){
    //target_add = 0;
    int i, count;
    for(i=0; i<1000; i++)
        count++;
    target = 1;
    for(i=0; i<1000; i++)
        count++;
    target = 1;
    for(i=0; i<10000; i++)
        count++;
    //target_add = 0;
    //printf("(Add) i:[%p] count:[%p] target:[%p]\n", &i, &count, &target);
}

void mul(char ch){
    //target_mul = 0;
    int i, count;
    for(i=0; i<100; i++)
        count++;
    target = 1;
    for(i=0; i<100; i++)
        count++;
    target = 1;
    for(i=0; i<100; i++)
        count++;
    target = 1;
    for(i=0; i<100; i++)
        count++;
    //target_mul = 0;
    //printf("(Mul) i:[%p] count:[%p] target:[%p]\n", &i, &count, &target);
    return;
}

void encrypt_key(char *key){
    for(int i=0; i<16; i++){
        int curr = 1;
        for(int j=0; j<8; j++){
            mul(key[i]);
            /*if((curr & key[i]) > 0){
                add(key[i]);
            //return;  // Remove This
            }*/
            curr *= 2;
        }
    }
    return;
}

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
    printf(" 1one one\n");
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
        page_no = rand()%21626930;
        mask =  (VTARGET+page_no*PAGE) >> 12;
        lower_bits = mask & 0x7F;
        upper_bits = mask & 0x3F80;
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


void *pin_thread(void *arg){
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
        char key[17] = "aaaaaaaaaaaaaaaa";
        target = 0;
        evict_l1();
        printf("thread %lu waiting for sync\n", curr);
        pthread_mutex_lock(&lock);
        sharedVar = 0;
        pthread_mutex_unlock(&lock);
        while(sharedVar == 0);
        printf("thread %lu synced\n", curr);
        int curr = 0;
        encrypt_key(key);
        sharedVar = 2;
        /*while(sharedVar != 0){
            for(int j=0; j<20; j++)
                curr++;
            target++;
        }*/
        printf("target accessed = %d\n", target);
	//profile_time();
    }
    return NULL;
}

int cmpfunc(const void *first,const void *second){
    int x, y;
    x = *(int *)first;
    y = *(int *)second;
    if(x < y)
        return 0;
    else
        return 1;
}

int main(int argc, char *argv[]){
    int outputFlag;
    if(argc != 2){
    	printf("[ERROR] Incorrect number of arguments!\n");
    	outputFlag = 0;
    }
    else
	outputFlag = atoi(argv[1]);
    assert(outputFlag == 1 || outputFlag == 0);
    int core1, core5,  set_no , access_time[100], access_time1[100000], curr_access_time;
    struct node *head_set1, *temp, *head_set2, *head_set3;
    uint64_t probe, temp_var;
    uint32_t time;
    //probe = allocate_pages(PAGES);
    pthread_t thread1;
    core1 = 1;
    core5 = 5;
    temp_var = (uint64_t)&target;
    //printf("set_no %d\n", (temp_var >> 12)%16);
    set_no = 7;

    printf("set_no: %d for target address: %p\n", set_no, (void *)temp_var);
    if(pthread_mutex_init(&lock, NULL)!=0){
        printf("mutex initialisation failed\n");
        return 1;
    }
    if(pthread_create(&thread1, NULL, pin_thread, &core1)){
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }
    printf("one\n");
    pin_thread(&core5);
    //head_set1 = create_evict_set1(L1_DTLB_ASSOC, set_no);
    head_set1 = create_evict_set1(4, set_no);
    printf("one one\n");
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
    //print_array(access_time, 100, 1, outputFlag);//Flag 1 for L1 hit

    printf("[OUT] Access Time for second Set:\t");
    probe = (uint64_t)head_set2;
    while(sharedVar == -1);
    printf("main thread executing\n");
        pthread_mutex_lock(&lock);
        sharedVar = 1;
        pthread_mutex_unlock(&lock);
    evict_l1();
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    //for(int i=0; i<100; i++){
        int z=0;
    //while(sharedVar != 2 && z < 100000){
    while(z < 100){
        z++;
        PROFILE_TIME_START
        PROFILE_TIME_SET1
        PROFILE_TIME_END1
        target = 1;
    }
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    //print_array(access_time, 100, 2, outputFlag);//Flag 2 for L2 hit

    printf("Setting sharedVar to zero\n");
    sharedVar = 0;

    qsort(access_time, 100, sizeof(int), cmpfunc);
    qsort(access_time1, z, sizeof(int), cmpfunc);
    printf("Array \n");
    print_array(access_time, 100, 2, outputFlag);//Flag 2 for L2 hit
    int sum = 0;
    int thres;
    int flag1 = 0;
    for(int i=0; i<100; i++){
        int curr = access_time[i];
        int j = i;
        while(j<99 && curr == access_time[j+1]){
            j++;
            if(!flag1)
                sum++;
        }
        if(sum >= 90){
            thres = access_time[i];
            flag1 = 1;
            sum = 0;
        }
        printf("[%d]: %d\n", access_time[i], j-i+1);
        i = j;
    }
    printf("Array 1\n");
    print_array(access_time1, 100, 2, outputFlag);//Flag 2 for L2 hit
    int ans = 0;
    for(int i=0; i<z; i++){
        int curr = access_time1[i];
        int j = i;
        while(j<z && curr == access_time1[j+1]){
            j++;
        }
        if(access_time1[i] >= thres)
            ans += j-i+1; 
        printf("[%d]: %d\n", access_time1[i], j-i+1);
        i = j;
    }
    printf(" thres = %d, ans = %d\n", thres,  ans);
    /*
    printf("[OUT] Access Time for Third Set:\t");
    probe = (uint64_t)head_set3;
    evict_l1();
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    for(int i=0; i<100; i++){
        PROFILE_TIME_START
        PROFILE_TIME_HASWELL_SET3
        PROFILE_TIME_END
    }
    asm volatile("cpuid" ::: "rax","rbx","rcx");
    print_array(access_time, 100, 3, outputFlag); //Flag 3 for L2 miss

    if(pthread_join(thread1, NULL)){
        fprintf(stderr, "Error joining thread\n");
        return 2;
    }
    */
    return 0;
}
