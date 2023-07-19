#include <sys/sem.h>
#include <errno.h>
#include "../inc/semaphore.h"
#include "../inc/errExit.h"

void semOp (int semid, unsigned short sem_num, short sem_op) {
    struct sembuf sop = {.sem_num = sem_num, .sem_op = sem_op, .sem_flg = 0};
    errno=0;
    int res;
    do{
        res=semop(semid,&sop,1);
    }while(res==-1 && errno==EINTR);
    if(res==-1)
        errExit("semop failed");
}

/************************************
*VR471795
*Alberto Ondei
*20/05/2023
*************************************/
