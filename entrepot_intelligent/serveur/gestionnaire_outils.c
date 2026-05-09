#include <stdio.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>
#include "../include/commun.h"

#pragma comment(lib, "ws2_32.lib")

#define TIMEOUT_SEC 10          // Délai avant libération automatique

// Structure d'un outil avec propriétaire et horodatage
typedef struct {
    int allocated;      // 1 = occupé, 0 = libre
    int owner_id;       // ID du client qui le détient
    time_t alloc_time;  // moment de l'allocation
} OutilInfo;

OutilInfo outils[NB_OUTILS];
HANDLE mutex;                   // Protège l'accès aux outils
FILE* log_file = NULL;          // Fichier de logs CSV
int next_client_id = 1;         // Pour attribuer un ID unique à chaque client
CRITICAL_SECTION cs_client_id;  // Protège next_client_id

// ------------------------------------------------------------
// Journalisation structurée (CSV)
// ------------------------------------------------------------
void log_message(const char* level, const char* format, ...) {
    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Console
    printf("[%s] [%s] %s\n", timestamp, level, buffer);
    // Fichier CSV : timestamp;level;message
    if (log_file) {
        fprintf(log_file, "%s;%s;%s\n", timestamp, level, buffer);
        fflush(log_file);
    }
}

// ------------------------------------------------------------
// Allocation d'un seul outil avec mécanisme wound-wait
// Retourne 1 si succès (alloué), 0 si refusé (attente)
// ------------------------------------------------------------
int allouer_outil(int id, int client_id) {
    if (id < 0 || id >= NB_OUTILS) return 0;

    if (!outils[id].allocated) {
        // Outil libre -> on prend
        outils[id].allocated = 1;
        outils[id].owner_id = client_id;
        outils[id].alloc_time = time(NULL);
        log_message("ALLOC", "Client %d a obtenu l'outil %d", client_id, id);
        return 1;
    } else {
        // Outil occupé : wound-wait
        // Si le demandeur a un ID plus petit (plus vieux) -> il blesse
        if (client_id < outils[id].owner_id) {
            log_message("WOUND", "Client %d blesse client %d sur l'outil %d",
                        client_id, outils[id].owner_id, id);
            // Libère l'outil pour le donner au blesseur
            outils[id].allocated = 1;
            outils[id].owner_id = client_id;
            outils[id].alloc_time = time(NULL);
            return 1;
        } else {
            // Sinon, le demandeur attend (refus)
            log_message("WAIT", "Client %d attend l'outil %d (detenu par %d)",
                        client_id, id, outils[id].owner_id);
            return 0;
        }
    }
}

// ------------------------------------------------------------
// Allocation de deux outils (ordre croissant pour éviter deadlock)
// Utilise allouer_outil pour chacun
// ------------------------------------------------------------
int allouer_deux_outils(int id1, int id2, int client_id) {
    if (id1 == id2) return 0;
    // Tri pour ordre fixe
    int a = id1, b = id2;
    if (a > b) { int tmp = a; a = b; b = tmp; }
    if (a < 0 || b >= NB_OUTILS) return 0;

    // Vérifier si on peut obtenir les deux en une fois (sans attendre)
    // On tente d'abord de blesser si nécessaire
    int ok1 = 1, ok2 = 1;
    if (outils[a].allocated && client_id >= outils[a].owner_id) ok1 = 0;
    if (outils[b].allocated && client_id >= outils[b].owner_id) ok2 = 0;
    if (!ok1 || !ok2) {
        log_message("WAIT", "Client %d ne peut pas obtenir les deux outils (%d,%d) immediatement",
                    client_id, a, b);
        return 0;
    }

    // Allocation effective avec wound-wait si nécessaire
    allouer_outil(a, client_id);
    allouer_outil(b, client_id);
    log_message("ALLOC", "Client %d a obtenu les deux outils %d et %d", client_id, a, b);
    return 1;
}

// ------------------------------------------------------------
// Libération d'un outil
// ------------------------------------------------------------
void liberer_outil(int id) {
    if (id >= 0 && id < NB_OUTILS) {
        int ancien_proprio = outils[id].owner_id;
        outils[id].allocated = 0;
        outils[id].owner_id = -1;
        outils[id].alloc_time = 0;
        log_message("LIBERE", "Client %d a libere l'outil %d", ancien_proprio, id);
    }
}

// ------------------------------------------------------------
// Thread watchdog : libère les outils dont le timeout est dépassé
// ------------------------------------------------------------
DWORD WINAPI watchdog(LPVOID param) {
    while (1) {
        Sleep(5000);  // vérification toutes les 5 secondes
        WaitForSingleObject(mutex, INFINITE);
        time_t now = time(NULL);
        for (int i = 0; i < NB_OUTILS; i++) {
            if (outils[i].allocated && (now - outils[i].alloc_time) > TIMEOUT_SEC) {
                log_message("WATCHDOG", "Timeout : liberation forcee de l'outil %d (client %d)",
                            i, outils[i].owner_id);
                outils[i].allocated = 0;
                outils[i].owner_id = -1;
                outils[i].alloc_time = 0;
            }
        }
        ReleaseMutex(mutex);
    }
    return 0;
}

// ------------------------------------------------------------
// Thread de gestion d'un client
// ------------------------------------------------------------
DWORD WINAPI handle_client(LPVOID param) {
    SOCKET client = *(SOCKET*)param;
    free(param);
    char buffer[1024];
    char response[64];

    // Attribuer un ID unique à ce client
    EnterCriticalSection(&cs_client_id);
    int client_id = next_client_id++;
    LeaveCriticalSection(&cs_client_id);

    log_message("INFO", "Client %d connecte", client_id);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) break;
        buffer[len] = '\0';
        log_message("RECV", "Client %d : %s", client_id, buffer);

        int id1, id2;

        // Commande DEMANDE_OUTIL
        if (sscanf(buffer, CMD_DEMANDE_OUTIL " %d", &id1) == 1) {
            WaitForSingleObject(mutex, INFINITE);
            int ok = allouer_outil(id1, client_id);
            ReleaseMutex(mutex);
            strcpy(response, ok ? REP_OK "\n" : REP_REFUS "\n");
            send(client, response, strlen(response), 0);
            log_message("SEND", "Client %d : %s", client_id, response);
        }
        // Commande LIBERATION_OUTIL
        else if (sscanf(buffer, CMD_LIBERATION_OUTIL " %d", &id1) == 1) {
            WaitForSingleObject(mutex, INFINITE);
            liberer_outil(id1);
            ReleaseMutex(mutex);
            strcpy(response, REP_LIBERE_OK "\n");
            send(client, response, strlen(response), 0);
            log_message("SEND", "Client %d : %s", client_id, response);
        }
        // Commande DEMANDE_DEUX_OUTILS
        else if (sscanf(buffer, CMD_DEMANDE_DEUX_OUTILS " %d %d", &id1, &id2) == 2) {
            WaitForSingleObject(mutex, INFINITE);
            int ok = allouer_deux_outils(id1, id2, client_id);
            ReleaseMutex(mutex);
            strcpy(response, ok ? REP_OK "\n" : REP_REFUS "\n");
            send(client, response, strlen(response), 0);
            log_message("SEND", "Client %d : %s", client_id, response);
        }
        else {
            send(client, "ERREUR_COMMANDE\n", 16, 0);
            log_message("ERROR", "Client %d : commande invalide", client_id);
        }
    }

    closesocket(client);
    log_message("INFO", "Client %d deconnecte", client_id);
    return 0;
}

// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------
int main() {
    WSADATA wsa;
    SOCKET server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    int addr_len;

    // Ouverture du fichier de logs CSV
    log_file = fopen("logs.csv", "a");
    if (!log_file) {
        printf("Warning: impossible d'ouvrir logs.csv\n");
    } else {
        // Écrire l'en-tête si le fichier est vide
        fseek(log_file, 0, SEEK_END);
        if (ftell(log_file) == 0) {
            fprintf(log_file, "timestamp;level;message\n");
        }
    }

    // Initialisation Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        log_message("ERROR", "WSAStartup a echoue");
        return 1;
    }

    // Création socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        log_message("ERROR", "socket() a echoue");
        WSACleanup();
        return 1;
    }

    // Option pour réutiliser l'adresse
    int reuse = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        log_message("ERROR", "bind() a echoue");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    if (listen(server_sock, 5) == SOCKET_ERROR) {
        log_message("ERROR", "listen() a echoue");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    log_message("INFO", "Serveur demarre sur le port %d", PORT);

    // Initialisation des structures de synchronisation
    mutex = CreateMutex(NULL, FALSE, NULL);
    InitializeCriticalSection(&cs_client_id);

    // Initialisation des outils
    for (int i = 0; i < NB_OUTILS; i++) {
        outils[i].allocated = 0;
        outils[i].owner_id = -1;
        outils[i].alloc_time = 0;
    }

    // Lancement du watchdog
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);

    // Boucle d'acceptation des clients
    while (1) {
        addr_len = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock == INVALID_SOCKET) {
            log_message("ERROR", "accept() a echoue");
            continue;
        }

        SOCKET* pclient = malloc(sizeof(SOCKET));
        if (pclient) {
            *pclient = client_sock;
            CreateThread(NULL, 0, handle_client, pclient, 0, NULL);
        } else {
            closesocket(client_sock);
            log_message("ERROR", "malloc echoue, client rejete");
        }
    }

    // Nettoyage (jamais atteint dans cet exemple)
    closesocket(server_sock);
    WSACleanup();
    if (log_file) fclose(log_file);
    return 0;
}