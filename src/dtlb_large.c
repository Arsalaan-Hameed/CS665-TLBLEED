#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>

#define TARGET 0x300000000000ULL
#define PAGE 2*1024*1024

/*
 * Author: Mohd Arsalaan Hameed, Aditya Rohan
 */

/*
 * Code to allocate no of data pages and access them.
 * Run it with high number of repetition to get observable difference in miss at dtlb.
 * 
 * run as ./dtlb repetition number_of_virtual_pages(p) p_args_specifying_page_number
 * With perf tool: perf stat -e dtlb_load_misses.stlb_hit ./dtlb 50000 5 0 16 32 48 64 
 *
 */

void allocate_buffer(uint64_t addr, int fd)
{
	assert(addr >= 0);
	volatile char *ret;
	ret = mmap((void *) addr, PAGE, PROT_READ|PROT_WRITE, MAP_HUGE_2MB | MAP_SHARED | MAP_FILE | MAP_FIXED, fd, 0);
	if(ret == MAP_FAILED) {
		perror("mmap");
           exit(1);
	}
	if(ret != (volatile char *) addr) { fprintf(stderr, "Wrong mapping\n"); exit(1); }
    return;
}

int main(int argc, char *argv[]){
    int fd, pages, repetitions;
    uint64_t virt_pages[20];
    const char *filename = "2mb.file";

    assert(argc > 3);
    repetitions = atoi(argv[1]);
    pages = atoi(argv[2]);
    assert(pages > 0 && pages <20);
    fd = open(filename, O_RDWR);
    if(fd == -1){
        printf("4kb.file opening failed. create a file named \"4kb.file\" of size 4kb and run again\n");
        exit(-1);
    }


    for (int p=0; p<pages; p++) {
        uint64_t curr_addr = TARGET + atoi(argv[3+p])*PAGE;
        virt_pages[p] = curr_addr;
        printf("[DEBUG] Address %lx\n", curr_addr);
        allocate_buffer(curr_addr, fd);
    }

    for (int i=0; i < repetitions; i++){
        for (int p=0; p<pages; p++){
            volatile int *probe = (volatile int *)virt_pages[p];
            *probe;
        }
    }
    return 0;
}

