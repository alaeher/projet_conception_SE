#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "processus.h"

typedef void (*policy_func)(Processus*, int);

typedef struct {
    char nom[50];
    policy_func fonction;
} Policy;

#endif