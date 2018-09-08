#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<sys/mman.h>
#include<sched.h>
#include<fcntl.h>
#include<unistd.h>

uint64_t probe;
int NO_OF_PAGES = 65;
int iteration = 100;
uint32_t times[100][65];

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
   	//array1[1]=1;
	request += 4096;
    }
    //getchar();
    close(fd);
}
void print_time(){
    int i, j,  *array = (int *)probe;
    uint32_t start,end;
    uint64_t temp = probe;
    for(j=0; j<100; j++){
	for(i=0; i<65; i++){
	    asm volatile (
		// "mfence\n"
		"lfence\n"
		// "cpuid\n"
		"rdtsc\n"
		"mov %%eax, %%edi\n"
		"mov (%2), %%rcx\n"
		"mov (%2), %%rcx\n"
		"mov (%2), %%rcx\n"
		"mov (%2), %%rcx\n"
		"lfence\n"
		"rdtscp\n"
		"mov %%eax, %1\n"
		"mov %%edi, %0\n"
		: "=r" (start), "=r" (end)
		: "r" (temp)
		: "rax", "rbx", "rcx",
		"rdx", "rdi");
	    times[j][i] = end - start;
	    /* DEBUG: printf("%d: diff=%d, probe addr = %lx \n", i, (end-start), temp); */
	    temp += 4096;
	}
	temp = probe;
    }
   
}

int var_calc(uint32_t *inputs, int size){
    int i;
    uint32_t acc = 0, prev = 0, temp = 0;
    for(i=0; i<size; i++){
	if(acc < prev) 
	    printf("overflow\n");
	prev = acc;
	acc += inputs[i];
    }
    acc = acc*acc;
    if(acc < prev)
	printf("overflow\n");
    prev = 0;
    for(i=0; i<size; i++){
	if(temp < prev)
	    printf("overflow\n");
	prev = temp;
	temp += (inputs[i]*inputs[i]);
    }
    temp = temp*size;
    if(temp < prev)
	printf("overflow\n");
    temp = (temp - acc)/(((uint32_t)(size))*((uint32_t) (size)));
    return temp;
}

struct str{
    int value, freq;
} unique[65];

int cmpfunc(const void *a, const void *b){
    struct str *aa, *bb;
    aa = (struct str *)a;
    bb = (struct str *)b;
    if(aa->value > bb->value){
	return 1;
    }
    else{
	return 0;
    }
    return 0;
}
int main(){

    int min_values[iteration], variances[iteration], prev_min,  max_dev, min_time, max_time;
    int  i, j, k, spurious, max_dev_all, tot_var, var_of_mins, var_of_vars;
    int diff[NO_OF_PAGES], freq[NO_OF_PAGES], pos, curr, flag;
    // Set the cpu affinity to 4.
    unsigned long mask = 4;
    unsigned int len = sizeof(mask);
    if(sched_setaffinity(0, len, (cpu_set_t *)&mask) < 0)
	perror("sched_setaffinity");

    // Map 65 virtual contiguous pages to same physical page.
    map_file(65);

    // Probe first addr of each virtual page and monitor time.
    print_time();
    //getchar();
    spurious = 0;
    prev_min = 0; 
    tot_var = 0;
    max_dev_all  = 0;
    var_of_mins = 0;
    var_of_vars = 0;
    /* DEBUG:
     *
    */
    // for(i=0; i<100; i++){
	// for(j=0; j<65; j++)
	    // printf("%d ", times[i][j]);
	// printf("\n");
    // }
    for(i=0; i<iteration; i++){
	max_dev = 0; 
	min_time = 0; 
	max_time = 0;

	for(j = 0; j<NO_OF_PAGES; j++){
	    if((min_time == 0) || (min_time > times[i][j]))
		min_time = times[i][j];
	    if(max_time < times[i][j])
		max_time = times[i][j];
	}
	max_dev = max_time - min_time;
	min_values[i] = min_time;
	if(prev_min > min_time)
	    spurious++;
	if(max_dev > max_dev_all)
	    max_dev_all = max_dev;
	variances[i] = var_calc(times[i], NO_OF_PAGES);
	tot_var += variances[i];
	// printf("%d: variance(cycles): %d,\tmax_deviation: %d,\tmin_time = %d,\tmax_time=%d\n",i, variances[i], max_dev, min_time, max_time);
	prev_min = min_time;
    }

	for(int i=0;i<iteration; i++){
		pos=0;
		unique[0].value = times[i][0];
		unique[0].freq = 1;
		for(int j=0;j<NO_OF_PAGES; j++){
			curr = times[i][j];
			for(int k=0;k<pos; k++){
				if(unique[k].value == curr){
					unique[k].freq++;
					flag = 1;
					break;
				}
			}
			if(flag == 0){
				unique[pos].value = curr;
				unique[pos].freq = 1;
				pos++;
				flag = 1;
			}
			flag = 0;
		}

	qsort(unique, pos, sizeof(struct str), cmpfunc);
	printf("%2d:\t", i+1);
		printf("[%d]\t",pos );
	int tot_freq=0;	
	for(j=0; j<pos; j++){
		    printf("%d(%d)\t", unique[j].value, unique[j].freq); 
			tot_freq +=unique[j].freq;
		}
		printf("<<%d>>", tot_freq);
		printf("\n");
    }

    return 0;
}

