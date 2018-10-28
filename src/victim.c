#define _GNU_SOURCE
#include<stdio.h>
#include "spy.h"
int target, target_add, target_mul;
// &target = 0x555555755014, Set no: 5 ( for 16 sets L1 dtlb)



void add(char ch){
    target_add = 0;
    int i, count;
    for(i=0; i<1000; i++)
        count++;
    target = 1;
    for(i=0; i<1000; i++)
        count++;
    target = 1;
    for(i=0; i<10000; i++)
        count++;
    target_add = 0;
    //printf("(Add) i:[%p] count:[%p] target:[%p]\n", &i, &count, &target);
}

void mul(char ch){
    target_mul = 0;
    int i, count;
    for(i=0; i<10000; i++)
        count++;
    target = 1;
    for(i=0; i<1000; i++)
        count++;
    target = 1;
    for(i=0; i<10000; i++)
        count++;
    target = 1;
    for(i=0; i<10000; i++)
        count++;
    target_mul = 0;
    //printf("(Mul) i:[%p] count:[%p] target:[%p]\n", &i, &count, &target);
    return;
}

void encrypt_key(char *key){
    for(int i=0; i<16; i++){
        int curr = 1;
        for(int j=0; j<8; j++){
            mul(key[i]);
            if((curr & key[i]) > 0){
                add(key[i]);
            //return;  // Remove This
            }
            curr *= 2;
        }
    }
    return;
}

int main(){
    int cpu_num, started;
    //int *sync_var;
    volatile struct sharestruct *myshare = get_sharestruct();
    //sync_var = (int *)get_shared_var();
    cpu_num = 1;
    pin_cpu(cpu_num);
    char key[17];
    for(int i=0; i<16; i++){
        key[i] = 'a';
    }
    //started = 0;
    printf("(Main) Target:[%p] add:[%p] mul:[%p]\n", &target, &target_add, &target_mul);
    //fgets(key, 16, stdin);
    //scanf("%s", &key);
    //*sync_var = 2;
   // printf("boundary\n");
    //while(*sync_var == 2);
    myshare->target_started = 2;
    while(myshare->target_started == 2);
    encrypt_key(key);
    //*sync_var = 2;
    myshare->target_started = 2;
    return 0;
}
