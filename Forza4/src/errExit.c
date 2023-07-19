#include "../inc/errExit.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

void errExit(const char *msg) {
    perror(msg);
    exit(1);
}

/************************************
*VR471795
*Alberto Ondei
*20/05/2023
*************************************/

