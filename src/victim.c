#define _GNU_SOURCE
#include <stdio.h>
#include "latency.h"



int target, target_add, target_mul;
// &target = 0x555555754014, Set no: 5 ( for 16 sets L1 dtlb)



void add(char ch){
    int i, count;
    target = 1;
    for(i=0; i<50; i++)
        count++;
    target = 1;
    for(i=0; i<35; i++)
        count++;
    target = 1;
    for(i=0; i<50; i++)
        count++;
}

void mul(char ch){
    int i, count;
    for(i=0; i<35; i++)
        count++;
    target = 1;
    for(i=0; i<50; i++)
        count++;
    target = 1;
    for(i=0; i<35; i++)
        count++;
    target = 1;
    for(i=0; i<50; i++)
        count++;
    return;
}

void encrypt_key(char *key){
    for(int i=0; i<16; i++){
        int curr = 1;
        for(int j=0; j<8; j++){
            mul(key[i]);
            if((curr & key[i]) > 0){
                add(key[i]);
            }
            curr *= 2;
        }
    }
    return;
}

int main(){
    int cpu_num, started;
    volatile struct sharestruct *myshare = get_sharestruct();
    char key[17];

    cpu_num = 1;
    pin_cpu(cpu_num);
    for(int i=0; i<16; i++){
        key[i] = 'x';
    }

    myshare->target_started = 2;
    while(myshare->target_started == 2);
    encrypt_key(key);
    myshare->target_started = 2;
    return 0;
}
