#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include "../include/commun.h"

#pragma comment(lib, "ws2_32.lib")

// Structures pour la file de messages (thread-safe)
typedef struct Request {
    char command[128];
    int type;       // 1 = simple outil, 2 = deux outils
    int id1, id2;
    struct Request* next;
} Request;

typedef struct Response {
    char reply[64];
    int type;
    int id1, id2;
    struct Response* next;
} Response;

Request* req_head = NULL;
Request* req_tail = NULL;
CRITICAL_SECTION req_cs;
HANDLE req_event;

Response* resp_head = NULL;
Response* resp_tail = NULL;
CRITICAL_SECTION resp_cs;
HANDLE resp_event;

SOCKET sock;
int connected = 0;

// Identifiant du robot (pour distinguer les instances)
int robot_id = 1;

// Journalisation côté client (simple console)
void log_client(const char* level, const char* format, ...) {
    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    printf("[%s] [Robot %d] [%s] %s\n", timestamp, robot_id, level, buffer);
}

// Ajouter une requête
void add_request(const char* cmd, int t, int id1, int id2) {
    Request* req = (Request*)malloc(sizeof(Request));
    strcpy(req->command, cmd);
    req->type = t;
    req->id1 = id1;
    req->id2 = id2;
    req->next = NULL;

    EnterCriticalSection(&req_cs);
    if (req_tail) {
        req_tail->next = req;
        req_tail = req;
    } else {
        req_head = req_tail = req;
    }
    LeaveCriticalSection(&req_cs);
    SetEvent(req_event);
}

// Récupérer une requête (bloquant)
Request* get_request() {
    WaitForSingleObject(req_event, INFINITE);
    EnterCriticalSection(&req_cs);
    Request* req = req_head;
    if (req) {
        req_head = req->next;
        if (!req_head) req_tail = NULL;
    }
    LeaveCriticalSection(&req_cs);
    return req;
}

// Ajouter une réponse
void add_response(const char* rep, int t, int id1, int id2) {
    Response* resp = (Response*)malloc(sizeof(Response));
    strcpy(resp->reply, rep);
    resp->type = t;
    resp->id1 = id1;
    resp->id2 = id2;
    resp->next = NULL;

    EnterCriticalSection(&resp_cs);
    if (resp_tail) {
        resp_tail->next = resp;
        resp_tail = resp;
    } else {
        resp_head = resp_tail = resp;
    }
    LeaveCriticalSection(&resp_cs);
    SetEvent(resp_event);
}

// Récupérer une réponse (bloquant)
Response* get_response() {
    WaitForSingleObject(resp_event, INFINITE);
    EnterCriticalSection(&resp_cs);
    Response* resp = resp_head;
    if (resp) {
        resp_head = resp->next;
        if (!resp_head) resp_tail = NULL;
    }
    LeaveCriticalSection(&resp_cs);
    return resp;
}

// ================= THREAD COMMUNICATION =================
DWORD WINAPI comm_thread(LPVOID param) {
    char buffer[1024];
    while (connected) {
        Request* req = get_request();
        if (!req) continue;

        // Construire la commande
        if (req->type == 1)
            sprintf(buffer, "%s %d\n", req->command, req->id1);
        else
            sprintf(buffer, "%s %d %d\n", req->command, req->id1, req->id2);

        send(sock, buffer, strlen(buffer), 0);
        log_client("SEND", "%s", buffer);

        // Attendre la réponse
        memset(buffer, 0, sizeof(buffer));
        int len = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (len <= 0) {
            log_client("ERROR", "Perte de connexion avec le serveur");
            connected = 0;
            free(req);
            break;
        }
        buffer[len] = '\0';
        // Supprimer le saut de ligne
        buffer[strcspn(buffer, "\n")] = '\0';
        log_client("RECV", "%s", buffer);

        add_response(buffer, req->type, req->id1, req->id2);
        free(req);
    }
    return 0;
}

// ================= THREAD IDLE =================
DWORD WINAPI idle_thread(LPVOID param) {
    srand((unsigned)time(NULL) + robot_id);
    while (connected) {
        // Attente aléatoire entre 3 et 10 secondes
        Sleep((rand() % 8 + 3) * 1000);

        int choix = rand() % 2;  // 0 = simple, 1 = double
        if (choix == 0) {
            int id = rand() % NB_OUTILS;
            log_client("IDLE", "Demande simple outil %d", id);
            add_request(CMD_DEMANDE_OUTIL, 1, id, -1);
        } else {
            int id1 = rand() % NB_OUTILS;
            int id2 = rand() % NB_OUTILS;
            while (id2 == id1) id2 = rand() % NB_OUTILS;
            // Ordre croissant pour respecter le protocole serveur
            if (id1 > id2) { int tmp = id1; id1 = id2; id2 = tmp; }
            log_client("IDLE", "Demande double outil (%d,%d)", id1, id2);
            add_request(CMD_DEMANDE_DEUX_OUTILS, 2, id1, id2);
        }
    }
    return 0;
}

// ================= THREAD ASSEMBLAGE =================
DWORD WINAPI assembly_thread(LPVOID param) {
    while (connected) {
        Response* resp = get_response();
        if (!resp) continue;

        if (strcmp(resp->reply, REP_OK) == 0) {
            if (resp->type == 1) {
                log_client("ASSEMBLY", "Outil %d alloue, assemblage...", resp->id1);
                Sleep(2000);
                log_client("ASSEMBLY", "Liberation outil %d", resp->id1);
                add_request(CMD_LIBERATION_OUTIL, 1, resp->id1, -1);
            } else {
                log_client("ASSEMBLY", "Outils %d et %d alloues, assemblage...", resp->id1, resp->id2);
                Sleep(3000);
                log_client("ASSEMBLY", "Liberation outils %d et %d", resp->id1, resp->id2);
                add_request(CMD_LIBERATION_OUTIL, 1, resp->id1, -1);
                add_request(CMD_LIBERATION_OUTIL, 1, resp->id2, -1);
            }
        } else if (strcmp(resp->reply, REP_REFUS) == 0) {
            log_client("ASSEMBLY", "Demande refusee, reessai apres delai");
            Sleep(1000);
            // Réinsérer la même demande
            if (resp->type == 1)
                add_request(CMD_DEMANDE_OUTIL, 1, resp->id1, -1);
            else
                add_request(CMD_DEMANDE_DEUX_OUTILS, 2, resp->id1, resp->id2);
        }
        free(resp);
    }
    return 0;
}

// ================= MAIN =================
int main(int argc, char* argv[]) {
    if (argc > 1) robot_id = atoi(argv[1]);
    WSADATA wsa;
    struct sockaddr_in server_addr;

    log_client("INFO", "Demarrage du robot");

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        log_client("ERROR", "WSAStartup echoue");
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        log_client("ERROR", "socket() echoue");
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_client("ERROR", "Connexion au serveur echouee");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    connected = 1;
    log_client("INFO", "Connecte au serveur");

    // Initialisation des structures synchronisées
    InitializeCriticalSection(&req_cs);
    InitializeCriticalSection(&resp_cs);
    req_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    resp_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    // Création des threads
    HANDLE hComm = CreateThread(NULL, 0, comm_thread, NULL, 0, NULL);
    HANDLE hIdle = CreateThread(NULL, 0, idle_thread, NULL, 0, NULL);
    HANDLE hAssembly = CreateThread(NULL, 0, assembly_thread, NULL, 0, NULL);

    log_client("INFO", "Robot actif (appuyez sur Entree pour arreter)");
    getchar();

    connected = 0;
    SetEvent(req_event);
    SetEvent(resp_event);

    WaitForSingleObject(hComm, 5000);
    WaitForSingleObject(hIdle, 5000);
    WaitForSingleObject(hAssembly, 5000);

    CloseHandle(hComm);
    CloseHandle(hIdle);
    CloseHandle(hAssembly);
    DeleteCriticalSection(&req_cs);
    DeleteCriticalSection(&resp_cs);
    CloseHandle(req_event);
    CloseHandle(resp_event);

    closesocket(sock);
    WSACleanup();
    log_client("INFO", "Robot arrete");
    return 0;
}