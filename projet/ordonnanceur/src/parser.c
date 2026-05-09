#include <stdio.h>
#include <string.h>
#include "../include/processus.h"

int lire_processus(const char *filename, Processus tab[]) {
    FILE *f = fopen(filename, "r");
    if (!f) return 0;

    char ligne[100];
    int i = 0;

    while (fgets(ligne, sizeof(ligne), f)) {
        if (ligne[0] == '#' || ligne[0] == '\n')
            continue;

        sscanf(ligne, "%s %d %d",
               tab[i].nom,
               &tab[i].arrivee,
               &tab[i].duree);

        tab[i].restant = tab[i].duree;
        i++;
    }

    fclose(f);
    return i;
}