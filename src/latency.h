#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <assert.h>

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

#define SYNC_FILE "/home/arsalaan/.sync_file"
#define PAGE 4096

struct node{
    struct node *addr;
    uint64_t arr[7];
};

struct sharestruct {
	volatile int target_started;
    volatile int bit_position;
    volatile int sample;
    volatile int groundtruth_boundaries;   /* only set bit_position if groundtruth is set */
    volatile int groundtruth_bits;   /* only set bit_position if groundtruth is set */
    volatile int magic;
    volatile int securemode;

};



/*
 * Create a file based on the argumnet fn and return file descriptor to that file
 */
static int createfile(const char *fn)
{
        int fd;
#ifndef NO_PTHREAD
        struct stat sb;
        char sharebuf[PAGE];
        if(stat(fn, &sb) != 0 || sb.st_size != PAGE) {
                fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, 0644);
                if(fd < 0) {
			perror("open");
                        fprintf(stderr, "createfile: couldn't create shared file %s\n", fn);
                        exit(1);
                }
                if(write(fd, sharebuf, PAGE) != PAGE) {
                        fprintf(stderr, "createfile: couldn't write shared file\n");
                        exit(1);
                }
                return fd;
        }

        assert(sb.st_size == PAGE);
#endif

        fd = open(fn, O_RDWR, 0644);
        if(fd < 0) {
            perror(fn);
                fprintf(stderr, "createfile: couldn't open shared file\n");
                exit(1);
        }
        return fd;

}



/*
 * Create a sharing file (filename describe by MACRO SYNC_FILE) for attacker victim synchronisation, 
 * if exist use that same file and return the virtual address that maps to the file
 *
 */ 
static volatile struct sharestruct *get_sharestruct()
{
    int fd = createfile(SYNC_FILE);
	volatile struct sharestruct *ret = (volatile struct sharestruct *) mmap(NULL, PAGE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FILE, fd, 0);
	if(ret == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	return ret;
}


/*
 * pin current process cpu affinity to cpu_num
 */
int pin_cpu(int cpu_num){

    int flag = 0;
    pid_t pid = getpid();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_num, &cpuset);
    if(sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset)){
        fprintf(stderr, "[ERROR] scheduling affinity for arg: %d (pid: %d) failed\n", cpu_num, pid);
    }
    CPU_ZERO(&cpuset);
    if(sched_getaffinity(pid, sizeof(cpu_set_t), &cpuset)){
        fprintf(stderr, "[ERROR] retriving affinity for arg: %d (pid: %d) failed\n", cpu_num, pid);
    }
    if(CPU_ISSET(cpu_num, &cpuset)){
	    fprintf(stdout, "[DEBUG] cpu affinity successfully set for arg: %d (pid: %d)\n", cpu_num, pid);
        flag = 1;
    }
    //else
	//fprintf(stdout, "[DEBUG] cpu affinity not correctly set for arg: %d (pid: %lu)\n", arg_value, curr);
    return flag;
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


void print_array(int *array, int size, int flag, int outputFlag){
	if( outputFlag == 0){
		FILE *pFile;
		if(flag == 1)
			pFile = fopen("../output/l1hit.txt", "a");
		else if(flag == 2)
			pFile = fopen("../output/l2hit.txt", "a");
		else if(flag == 3)
			pFile = fopen("../output/l2miss.txt", "a");
		else
			printf("[ERROR] Unknown flag!\n" );
    	for(int i=0; i<size; i++)
			fprintf(pFile, "%d\t", array[i]);
    	fprintf(pFile, "\n");
    	fclose(pFile);
    }
    else{
    	for(int i=0; i<size; i++)
			printf( "%d\t", array[i]);
    	printf("\n");
    }
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
