#include<stdlib.h>
#include<sys/mman.h>
#include<fcntl.h>

/*
 * Defining no of access for different micro arch, No of access for different set are:
 * 	HASWELL: 4, 8, 9
 * 	BROADWELL: 4, 5, 7
 * 	KABYLAKE: 4, 10, 5
 */
#define PROFILE_TIME_START asm volatile( \
			    "lfence\n" \
			    "rdtsc\n" \
			    "mov %%eax, %%edi\n"

#define PROFILE_TIME_SET1  "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n"

#define PROFILE_TIME_HASWELL_SET2  "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" 
#define PROFILE_TIME_HASWELL_SET3  "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \

#define PROFILE_TIME_KABYLAKE_SET2  "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" 
#define PROFILE_TIME_KABYLAKE_SET3  "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" 

#define PROFILE_TIME_BROADWELL_SET2  "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" 

#define PROFILE_TIME_BROADWELL_SET3  "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" "mov (%1), %1\n" "mov (%1), %1\n" \
				   "mov (%1), %1\n" 

#define PROFILE_TIME_END \
		"lfence\n" \
		"rdtscp\n" \
		"sub %%edi, %%eax\n" \
		"mov %%eax, %0\n" \
		: "=r" (access_time[i]) \
		: "r" (probe)   \
		: "rax", "rbx", "rcx", "rdx", "rdi"); 

struct node{
    struct node *addr;
    uint64_t arr[7];
};

void print_list(struct node *temp){
    uint64_t mask = 0xffffffffffffffc0, var;
    while(temp != NULL){
	var  = (uint64_t)temp;
	var = var >> 6;
	printf(" %p[%lu]", temp, var%63);
	temp = temp->addr;
	if(temp != NULL)
	    printf("-->");
    }
    printf("\n");
    return;
}

void print_array(int *array, int size){
    for(int i=0; i<size; i++)
	printf("%d\t", array[i]);
    printf("\n\n");
}

/*
 * Allocate continuous virtual pages mapping to same file 
 * return the address of first allocated pages
 */
uint64_t allocate_pages(int pages){
    int *array, *array1, i, fd, diff;
    uint64_t probe;
    unsigned long request, recieved;
    diff = -1;
    fd = open("4kb.file", O_RDWR);
    if(fd == -1){
	printf("4kb.file opening failed.\n");
	exit(-1);
    }
    //array = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    array = mmap(NULL, pages*4096, PROT_READ | PROT_WRITE, MAP_SHARED|MAP_FILE, fd, 0);
    request = (unsigned long) array;
    probe = (uint64_t)array;    // Set the probe variable to address of the starting virtual address for continuous pages
    // DEBUG: printf("%p, %lx, %d\n", array, probe, (int)sizeof(probe));
    
    for(i=0; i<pages; i++){
	if(munmap((void *)request, 4096) == -1){
	    printf("Error for mapping of page no %d at address %lx\n", i, request);
	    close(fd);
	    exit(-1);
	}
	array1 = mmap((void *)request, 4096, PROT_READ | PROT_WRITE, MAP_SHARED|MAP_FILE|MAP_FIXED, fd, 0);
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
	//printf("[DEBUG] Requested=%p, Returned=%p, diff=%d\n", (void *)request, array1, diff);
   	//array1[1]=1;
	request += 4096;
    }
    close(fd);
    return probe;
}
