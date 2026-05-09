#include <stdio.h>
#include "../include/processus.h"
#include "../include/scheduler.h"

// fonctions externes
int lire_processus(const char *filename, Processus tab[]);

// politiques
void fifo(Processus*, int);
void sjf(Processus*, int);
void round_robin(Processus*, int, int);

int main() {
    Processus tab[100];
    int n, choix;

    // lecture fichier
    n = lire_processus("config/config.txt", tab);

    if (n == 0) {
        printf("Erreur : aucun processus charge\n");
        return 1;
    }

    printf("=================================\n");
    printf("     ORDONNANCEUR DE PROCESSUS\n");
    printf("=================================\n");
    printf("Nombre de processus : %d\n\n", n);

    // menu
    printf("Choisir l'algorithme :\n");
    printf("1. FIFO (First In First Out)\n");
    printf("2. SJF (Shortest Job First)\n");
    printf("3. Round Robin\n");
    printf("Votre choix : ");
    scanf("%d", &choix);

    printf("\n===== EXECUTION =====\n");

    switch (choix) {
        case 1:
            fifo(tab, n);
            break;

        case 2:
            sjf(tab, n);
            break;

        case 3: {
            int quantum;
            printf("Entrer le quantum : ");
            scanf("%d", &quantum);

            if (quantum <= 0) {
                printf("Quantum invalide\n");
                return 1;
            }

            round_robin(tab, n, quantum);
            break;
        }

        default:
            printf("Choix invalide\n");
    }

    return 0;
}