#include <stdio.h>
#include "../include/processus.h"

void sjf(Processus tab[], int n) {
    int termine[n];
    for (int i = 0; i < n; i++) termine[i] = 0;

    int temps = 0, fini = 0;

    printf("\n===== SJF =====\n\n");
    printf("Gantt :\n");

    while (fini < n) {
        int min = -1;

        for (int i = 0; i < n; i++) {
            if (!termine[i] && tab[i].arrivee <= temps) {
                if (min == -1 || tab[i].duree < tab[min].duree)
                    min = i;
            }
        }

        if (min == -1) {
            temps++;
            continue;
        }

        tab[min].debut = temps;

        for (int t = 0; t < tab[min].duree; t++) {
            printf("| %s ", tab[min].nom);
            temps++;
        }

        tab[min].fin = temps;

        termine[min] = 1;
        fini++;
    }

    printf("|\n\n");

    float total_attente = 0, total_sejour = 0;

    printf("Processus | Att | Sej\n");

    for (int i = 0; i < n; i++) {
        int sejour = tab[i].fin - tab[i].arrivee;
        int attente = sejour - tab[i].duree;

        total_attente += attente;
        total_sejour += sejour;

        printf("%s\t   %d\t %d\n", tab[i].nom, attente, sejour);
    }

    printf("\nMoyenne attente = %.2f\n", total_attente / n);
    printf("Moyenne sejour  = %.2f\n", total_sejour / n);
}