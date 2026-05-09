#ifndef PROCESSUS_H
#define PROCESSUS_H

typedef struct {
    char nom[10];
    int arrivee;
    int duree;
    int restant;

    int debut;
    int fin;
} Processus;

#endif