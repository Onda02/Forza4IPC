#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <time.h>

#include "../inc/shared_memory.h"
#include "../inc/semaphore.h"
#include "../inc/errExit.h"    
/*DIMENSIONE SEMAFORI*/
#define SEM_DIM 9

#define EMPTY 0 
//EMPTY semaforo usato in modo che il server aspetta
//che venga riempito clientPid
#define MUTEX 1
//MUTEX viene usato in modo che un solo processo client alla volta
//possa entrare dentro l'area condivisa e riempire il clientPid
//inizializzato a 1 cosi il primo client che entra lo setta
#define WAIT_SECOND 2
#define WSC 3
#define ASSEST 4
#define BC 5
#define BS 6
#define BC1 7
#define BC2 8


void printMatrix(struct Matrix *matrix);
int isValid(int col,struct Matrix *matrix);
int currentRow(int col,struct Matrix *matrix);
void closeFunction();

char *M;
int main(int argc,char *argv[]){
    //argomento passato con exec
    srand(time(NULL));
    char *shape=argv[1];
    key_t keyMatrix = ftok("src/F4Server.c",4);
    if(keyMatrix==-1)
        errExit("ftok fallita");
    
    int shmidMatrix=alloc_shared_memory(keyMatrix, sizeof(struct Matrix));
    struct Matrix *matrix= (struct Matrix *)get_shared_memory(shmidMatrix,0); 

    key_t keyM=ftok("src/F4Server.c",9);
    if(keyM==-1){
        errExit("ftok fallito");
    }
    int shmidM=alloc_shared_memory(keyM,sizeof(char)*matrix->max_cols*matrix->max_rows);
    M=(char *)get_shared_memory(shmidM,0);
    
    int col;
    int row;
    int valid;
    do{
        col=rand()%(matrix->max_cols+1);
        row=currentRow(col,matrix);
        valid=isValid(col,matrix);
    }while(valid!=1);
    M[row*matrix->max_cols+col-1]=*shape;
    //close
    free_shared_memory(matrix);
    free_shared_memory(M);
    return 0;
}

int isValid(int col,struct Matrix *matrix){
    int i;
    //controllo non abbia inserito oltre
    if(col>matrix->max_cols)
        return -1;
    //controllo sia maggiore di 0
    if(col <=0)
        return -3;
    //controllo la colonna non sia piena
    for(i=0;i<matrix->max_rows;i++){
        if(M[i*matrix->max_cols+col-1]==' ')
            break;
    }

    if(i==matrix->max_rows)
        return -2;  
    
    return 1;
}

int currentRow(int col,struct Matrix *matrix){
    int i;
    for(i=1;i<matrix->max_rows;i++){
        if(M[i*matrix->max_cols+col-1]!=' ')
            break;
    }

    return i-1;
}

/************************************
*VR471795
*Alberto Ondei
*20/05/2023
*************************************/
