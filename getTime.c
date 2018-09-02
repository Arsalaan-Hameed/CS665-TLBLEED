#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<unistd.h>

uint64_t probe;

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
    probe = (uint64_t)array;    // Set the probe variable to address of the starting virtual address for continuous pages
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
   	array1[1]=1;
	request += 4096;
    }
    close(fd);
}
void print_time(){
    int i, j,  *array = (int *)probe;
    uint32_t time1,time2;
    uint64_t temp = probe;
    /*
     * Training: Access all the virtual pages before monitoring them.
     * Bring all the entries to the TLB
     * how will time1, time2 variable access will affect time monitor?
     * Instead of calling printf try to store time1 & time2 in an array.
     */
    for(i=0; i<64; i++){
	array[0]=i;
	array += 1024;
    }
    int minTime[100];
    for(int k=0; k<10; k++){
    	printf("RUN No. %d\n", k);
    for(j=0; j<100; j++)
    	minTime[j] = 999;
    for(j=0; j<100; j++){
    	int tempTime =0;
    	temp = probe;
    	// printf("%d\n",j );
		for(i=0; i<64; i++){
		    /*
		     * TO-DO:
		     * change probe value in each iteration to point to next virtual page
		     */
		    asm volatile (
			"lfence\n"
			"cpuid\n"
			"rdtsc\n"
			"mov %%eax, %%edi\n"
			"mov (%2), %2\n"
			// "lfence\n"
			"rdtscp\n"
			"mov %%edi, %0\n"
			"mov %%eax, %1\n"
			"cpuid\n"
			: "=r" (time1), "=r" (time2)
			: "r" (temp)
			: "rax", "rbx", "rcx",
			"rdx", "rdi");
		    tempTime = time2-time1;
		    if(tempTime < minTime[j])
		    	minTime[j] = tempTime;
		    // printf("%d: diff=%d, probe addr = %lx \n", i, tempTime, temp);
		    temp += 4096;
		}
		printf("%d: minTime =%d\n",j, minTime[j] );
    }
    printf("\n");
   }
}

int main(){
    int x=5;
    map_file(64);
    print_time();
    return 0;
}
