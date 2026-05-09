
# Projet : Gestion d’outils partagés pour entrepôt intelligent

## 🎥 Vidéo de présentation

🔗 **[Cliquez ici pour voir la vidéo explicative du projet (Gestion des outils)](https://drive.google.com/file/d/1ZtkxOn8Ur6xXYKE2VhVhiGPpKL2A-zio/view?usp=drive_link)**

---

## 1. Présentation

Ce projet simule un environnement industriel où plusieurs **bras robotiques** (clients) doivent assembler des produits en utilisant des **outils communs** (tournevis, pince, soudeuse…).  
Pour éviter les conflits et les interblocages, un **serveur central** gère l’allocation et la libération des outils. Chaque bras robotique est un client multithreadé communiquant avec le serveur via **sockets TCP**.

Le problème est une adaptation réaliste du **dîner des philosophes** : chaque bras a besoin de deux outils pour travailler, et les ressources sont limitées.

---

## 2. Architecture du système

```
┌─────────────────────────────────────────────────────────┐
│                         SERVEUR                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Gestionnaire d'outils (mutex + watchdog)       │   │
│  │  - État de chaque outil (libre/occupé)          │   │
│  │  - File d’attente implicite (refus + réessai)   │   │
│  │  - Logs structurés (CSV avec timestamp)         │   │
│  └─────────────────────────────────────────────────┘   │
│                         ▲ │                             │
│              ┌──────────┴─┴──────────┐                  │
│              │    Sockets TCP/IP     │                  │
│              └──────────┬─┬──────────┘                  │
└─────────────────────────┼─┼─────────────────────────────┘
                          │ │
        ┌─────────────────┼─┼─────────────────┐
        ▼                 ▼                   ▼
┌───────────────┐  ┌───────────────┐    ┌───────────────┐
│   Bras #1     │  │   Bras #2     │    │   Bras #n     │
│ 3 threads :   │  │ 3 threads :   │    │ 3 threads :   │
│ - Idle        │  │ - Idle        │    │ - Idle        │
│ - Comm        │  │ - Comm        │    │ - Comm        │
│ - Assemblage  │  │ - Assemblage  │    │ - Assemblage  │
└───────────────┘  └───────────────┘    └───────────────┘
```

---

## 3. Composants

### 3.1 Serveur (`gestionnaire_outils.c`)

- **Écoute** sur le port `8888` (modifiable dans `commun.h`).
- Pour chaque client, un **thread dédié** gère la communication.
- **Mutex** pour protéger l’état des outils (accès concurrent).
- **Watchdog** : libère automatiquement un outil non rendu après `TIMEOUT_SEC = 10` secondes.
- **Stratégie anti‑deadlock** : `wound-wait` + ordre fixe des outils.
- **Journalisation** : écriture dans `logs.csv` avec horodatage ISO.

### 3.2 Client (`bras_robotique.c`)

Chaque bras robotique est un **processus indépendant** qui lance **3 threads** :

| Thread          | Rôle                                                                 |
|-----------------|----------------------------------------------------------------------|
| `idle_thread`   | Simule une phase de réflexion (attente aléatoire 3‑10 s), puis génère une demande d’un ou deux outils. |
| `comm_thread`   | Envoie les requêtes au serveur, reçoit les réponses et les met dans une file. |
| `assembly_thread`| Récupère les réponses « OK », simule l’assemblage (`Sleep`), puis libère les outils. |

Les threads communiquent via **files d’attente thread-safe** (protégées par `CRITICAL_SECTION`).

### 3.3 Fichier d’en‑tête commun (`commun.h`)

Définit :
- le port,
- le nombre d’outils (`NB_OUTILS = 3`),
- les commandes texte : `DEMANDE_OUTIL`, `LIBERATION_OUTIL`, `DEMANDE_DEUX_OUTILS`,
- les réponses : `OK`, `REFUS`, `LIBERE_OK`.

---

## 4. Protocole de communication

Les échanges sont en **texte** (ligne terminée par `\n`).

### Commandes client → serveur

| Commande                       | Exemple                  | Signification                          |
|--------------------------------|--------------------------|----------------------------------------|
| `DEMANDE_OUTIL <id>`          | `DEMANDE_OUTIL 2`        | Demande l’outil n°2                    |
| `LIBERATION_OUTIL <id>`       | `LIBERATION_OUTIL 2`     | Libère l’outil n°2                     |
| `DEMANDE_DEUX_OUTILS <id1> <id2>` | `DEMANDE_DEUX_OUTILS 1 2` | Demande simultanée des deux outils     |

### Réponses serveur → client

- `OK` → la demande est acceptée.
- `REFUS` → les outils ne sont pas disponibles (le client doit réessayer plus tard).
- `LIBERE_OK` → confirmation de libération.
- `ID_INVALIDE` → identifiant d’outil incorrect.
- `ERREUR_COMMANDE` → message non reconnu.

---

## 5. Prévention des interblocages (deadlocks)

Le serveur implémente **trois mécanismes combinés** :

### 5.1 Hiérarchie (ordre fixe) des ressources
- Pour une demande double (`DEMANDE_DEUX_OUTILS a b`), le serveur **trie** automatiquement `(a,b)` en ordre croissant. Ainsi, tous les clients demandent les outils dans le même ordre, éliminant les cycles d’attente.

### 5.2 Algorithme « wound-wait »
- Chaque client reçoit un **identifiant unique** (`client_id` = 1, 2, 3…). Plus l’ID est petit, plus le client est « âgé ».
- Quand un client `C` demande un outil déjà détenu par `P` :
  - si `C < P` (C est plus âgé) → **wound** : C blesse P, l’outil est retiré à P et donné à C. P devra réessayer.
  - si `C > P` (C est plus jeune) → **wait** : C est refusé et devra réessayer plus tard.
- Cette politique garantit qu’un client plus âgé finira toujours par obtenir ses ressources.

### 5.3 Timeout (watchdog)
- Une thread `watchdog` parcourt tous les outils toutes les 5 secondes.
- Si un outil est alloué depuis plus de `TIMEOUT_SEC` (10 s) sans avoir été libéré, le watchdog le libère **forcément** et logue l’événement.
- Cela protège contre les clients qui planteraient sans libérer leurs outils.

---

## 6. Journalisation (logs)

Le serveur écrit dans un fichier **`logs.csv`** (format facilement exploitable).

Exemple de ligne :
```csv
2026-05-05T00:15:23;WOUND;Client 1 blesse client 2 sur l'outil 0
2026-05-05T00:15:23;ALLOC;Client 1 a obtenu l'outil 0
2026-05-05T00:15:33;WATCHDOG;Timeout : libération forcée de l'outil 2 (client 1)
```

Colonnes : `timestamp;niveau;message`

Niveaux utilisés :
- `INFO` (connexion/déconnexion)
- `RECV` / `SEND` (trafic)
- `ALLOC` / `LIBERE`
- `WOUND` / `WAIT` (décisions de prévention deadlock)
- `WATCHDOG` (timeout)
- `ERROR`

Ces logs permettent de **prouver** le bon fonctionnement des mécanismes anti‑blocage.

---

## 7. Installation et compilation

### Prérequis
- **Windows** avec MinGW (ou Visual Studio avec support C).
- Sous **Linux** : modifier le code pour utiliser `pthread` (remplacer `CreateThread` par `pthread_create`, etc.).

### Fichiers à placer
```
entrepot_intelligent/
├── include/
│   └── commun.h
├── serveur/
│   └── gestionnaire_outils.c
├── client/
│   └── bras_robotique.c
└── Makefile (optionnel)
```

### Compilation (sous Windows avec MinGW)

```bash
# Serveur
gcc serveur/gestionnaire_outils.c -lws2_32 -o serveur.exe

# Client
gcc client/bras_robotique.c -Iinclude -lws2_32 -o robot.exe
```

---

## 8. Exécution

1. **Démarrer le serveur** dans un terminal :
   ```bash
   ./serveur.exe
   ```
   Il affiche : `Serveur démarré sur le port 8888`

2. **Lancer un ou plusieurs clients** (dans des terminaux distincts) :
   ```bash
   ./robot.exe 1   # premier robot
   ./robot.exe 2   # deuxième robot
   ./robot.exe 3   # etc.
   ```

   Le paramètre numérique est optionnel ; il permet de distinguer les robots dans les logs du client (affichage `[Robot 1]`).

3. **Observation** :
   - Sur la console du serveur : toutes les actions, les décisions `WOUND`/`WAIT`, et les timeouts éventuels.
   - Sur chaque client : les phases `IDLE`, `SEND`/`RECV`, `ASSEMBLY`.
   - Le fichier `logs.csv` s’enrichit au fur et à mesure.

4. **Arrêt** :
   - Tapez `Entrée` dans la console de chaque client pour l’arrêter proprement.
   - Le serveur continue à tourner ; fermez sa console pour l’arrêter.

---

## 9. Exemple de scénario multi‑clients

**Deux robots** lancés en même temps :

- Robot 1 (`./robot.exe 1`) demande `DEMANDE_DEUX_OUTILS 0 1`
- Robot 2 (`./robot.exe 2`) demande `DEMANDE_DEUX_OUTILS 1 0`

Le serveur trie en `(0,1)` pour les deux.  
Il compare les âges (1 est plus vieux que 2) :
- Si le serveur a déjà alloué les outils au Robot 1, alors Robot 2 se voit refuser (`REFUS`) avec un log `WAIT`.

Log extrait :
```
[2026-05-05T00:15:23] [WOUND] Client 1 blesse client 2 sur l'outil 1
[2026-05-05T00:15:23] [ALLOC] Client 1 a obtenu l'outil 0
[2026-05-05T00:15:23] [ALLOC] Client 1 a obtenu l'outil 1
[2026-05-05T00:15:23] [WAIT] Client 2 attend l'outil 0 (détenu par 1)
```

Aucun deadlock ne se produit.

---

## 10. Extensions possibles (non implémentées dans la version de base)

L’énoncé suggère des ajouts pour aller plus loin :

- **Qualité de service (QoS)** : ajouter un champ `PRIORITÉ` aux demandes, et implémenter une file de priorité sur le serveur.
- **Interface graphique (GUI)** : avec `ncurses` (terminal) ou `GTK` (fenêtre) pour visualiser l’état des outils et les actions en temps réel.
- **Synchronisation globale** : phase de rendez‑vous où tous les bras doivent finir leur tâche avant de commencer la suivante.

---

## 11. Compétences mises en œuvre

Ce projet mobilise les notions avancées de programmation système :

- Sockets TCP/IP (client/serveur)
- Multithreading (création, synchronisation par mutex et sections critiques)
- Communication inter‑threads (files d’attente, événements)
- Algorithmes de prévention d’interblocage (wound‑wait, timeout, hiérarchie des ressources)
- Journalisation structurée
- Protocole de messagerie simple

---

## 12. Conclusion

Cette simulation d’entrepôt intelligent fournit une solution **robuste et exempte d’interblocages** pour la coordination de bras robotiques partageant des outils.  
Elle respecte l’intégralité des exigences de l’énoncé : communication TCP, multithreading client et serveur, synchronisation, prévention des deadlocks, et logs horodatés.  
Le code est modulaire et peut facilement être étendu avec les options de QoS ou d’interface graphique.

**Auteurs** : Noura Dadda Harmaz Alae

---

