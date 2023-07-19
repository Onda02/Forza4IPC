#ifndef _SHARED_MEMORY_HH
#define _SHARED_MEMORY_HH
#include <stdbool.h>
#include <stdlib.h>

//Structure shared from client and server to comincate the pid
struct setClient {
    pid_t pid;  
    char playerName[50];    
    bool wantBot;
};

struct Information{
    pid_t pid_c1;
    pid_t pid_c2;
    pid_t pid_server;
    char c1Name[50];
    char c2Name[50];
};

struct Matrix{
    int max_rows;
    int max_cols;
};

struct Shape{
    char sh1;
    char sh2;
};

// The alloc_shared_memory method creates, if it does not exist, a shared
// memory segment with size bytes and shmKey key.
// It returns the shmid on success, otherwise it terminates the calling process
int alloc_shared_memory(key_t shmKey, size_t size);

int server_alloc_shared_memory(key_t shmKey, size_t size);

// The get_shared_memory attaches a shared memory segment in the logic address space
// of the calling process.
// It returns a pointer to the attached shared memory segment,
// otherwise it terminates the calling process
void *get_shared_memory(int shmid, int shmflg);

// The free_shared_memory detaches a shared memory segment from the logic
// address space of the calling process.
// If it does not succeed, it terminates the calling process
void free_shared_memory(void *ptr_sh);

// The remove_shared_memory removes a shared memory segment
// If it does not succeed, it terminates the calling process
void remove_shared_memory(int shmid);


#endif

/************************************
*VR471795
*Alberto Ondei
*20/05/2023
*************************************/
