#include<stdlib.h>
#include<sys/mman.h>
#include<fcntl.h>



static inline uint32_t profile_time2(uint64_t probe){
    uint32_t time;
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
	    : "=r" (time)
	    : "r" (probe)
	    : "rax", "rbx", "rcx", "rdx", "rdi");
    return time;
}



/*
 * Function from where the newly created thread will start executing
 */

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
