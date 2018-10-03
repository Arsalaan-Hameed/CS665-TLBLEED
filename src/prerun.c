#define _GNU_SOURCE
#include<stdio.h>
#include<sched.h>
#include<omp.h>

    int array[256][1024][1024] = {0};
int main(){
omp_set_num_threads(8);
#pragma omp parallel
    {
    int id = omp_get_thread_num();
    unsigned long mask = id;
    unsigned int len = sizeof(mask);
    //FILE *fp = fopen("/home/hadoop/hadoop/input/out", "r");
    /*if(sched_setaffinity(0, len, (cpu_set_t *)&mask) < 0){
	perror("sched_setaffinity");
	printf("%d\n", id);
    }*/
    //int fd = open("/home/hadoop/hadoop/input/out", 
    for(int x=0; x<10; x++){
	printf("%d\t", x);
    for(int i=0; i<256; i++)
	for(int j=0; j<1024; j++)
	    for(int k=0; k<1024; k++)
	    array[i][j][k] = i*j*k;
    }
    printf("\n");
    }
    return 0;
}

