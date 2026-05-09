# Ordonnanceur de Processus en C

## Description

Ce projet consiste à simuler un ordonnanceur de processus en langage C.  
Il permet de tester différentes politiques d’ordonnancement utilisées dans les systèmes d’exploitation.

---

## Objectifs

- Comprendre le fonctionnement de l’ordonnancement CPU
- Implémenter plusieurs algorithmes
- Analyser les performances (temps d’attente, temps de séjour)
- Visualiser l’exécution avec un diagramme de Gantt

---

## Algorithmes implémentés

### 1. FIFO (First In First Out)
- Exécution dans l’ordre d’arrivée
- Simple mais pas optimisé

### 2. SJF (Shortest Job First)
- Choisit le processus le plus court
- Réduit le temps moyen d’attente

### 3. Round Robin
- Partage du CPU avec un quantum
- Équitable entre les processus

---

## Structure du projet
ordonnanceur/
├── src/
│ ├── main.c
│ ├── parser.c
├── include/
│ ├── processus.h
│ ├── scheduler.h
├── policies/
│ ├── fifo.c
│ ├── sjf.c
│ ├── rr.c
├── config/
│ └── config.txt
├── Makefile
└── README.md

---

## ▶️ Compilation et exécution

### Sous Windows :

gcc src/main.c src/parser.c policies/*.c -o ordonnanceur.exe
.\ordonnanceur.exe



## 📄 Format du fichier config

Exemple :

Nom Arrivée Durée

P1 0 5
P2 1 3
P3 2 2


- Les lignes vides sont ignorées
- Les commentaires commencent par `#`

---

## 📊 Résultats affichés

- Diagramme de Gantt
- Temps d’attente
- Temps de séjour
- Moyennes globales

---

## 📚 Concepts utilisés

- Structures en C
- Tableaux
- Pointeurs de fonctions
- Simulation d’ordonnancement

---

## 🚀 Améliorations possibles

- Interface graphique (GTK ou ncurses)
- Chargement dynamique des politiques
- Ajout de priorités
- Sauvegarde des résultats

---

## 👨‍🎓 Auteur

Projet réalisé dans le cadre du module :  
**Conception des Systèmes d’Exploitation**
