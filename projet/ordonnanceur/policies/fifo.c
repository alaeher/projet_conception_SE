#include <stdio.h>
#include "../include/processus.h"

void fifo(Processus tab[], int n) {
    int temps = 0;

    printf("\n===== FIFO =====\n\n");

    // GANTT
    printf("Gantt :\n");

    for (int i = 0; i < n; i++) {

        if (temps < tab[i].arrivee)
            temps = tab[i].arrivee;

        tab[i].debut = temps;

        for (int t = 0; t < tab[i].duree; t++) {
            printf("| %s ", tab[i].nom);
            temps++;
        }

        tab[i].fin = temps;
    }
    printf("|\n\n");

    // STATISTIQUES
    float total_attente = 0, total_sejour = 0;

    printf("Processus | Arr | Dur | Deb | Fin | Att | Sej\n");

    for (int i = 0; i < n; i++) {
        int sejour = tab[i].fin - tab[i].arrivee;
        int attente = sejour - tab[i].duree;

        total_attente += attente;
        total_sejour += sejour;

        printf("%s\t   %d\t %d\t %d\t %d\t %d\t %d\n",
               tab[i].nom,
               tab[i].arrivee,
               tab[i].duree,
               tab[i].debut,
               tab[i].fin,
               attente,
               sejour);
    }

    printf("\nMoyenne attente = %.2f\n", total_attente / n);
    printf("Moyenne sejour  = %.2f\n", total_sejour / n);
}