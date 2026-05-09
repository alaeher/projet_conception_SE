#ifndef COMMUN_H
#define COMMUN_H

#define PORT 8888
#define NB_OUTILS 3

// Commandes du protocole
#define CMD_DEMANDE_OUTIL      "DEMANDE_OUTIL"
#define CMD_LIBERATION_OUTIL   "LIBERATION_OUTIL"
#define CMD_DEMANDE_DEUX_OUTILS "DEMANDE_DEUX_OUTILS"

// Réponses du serveur
#define REP_OK                 "OK"
#define REP_REFUS              "REFUS"
#define REP_ID_INVALIDE        "ID_INVALIDE"
#define REP_LIBERE_OK          "LIBERE_OK"

#endif