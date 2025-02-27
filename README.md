# SIF1015-TP3
Projet de développement client-serveur en C

## Description
Ce projet consiste à développer un serveur de fichiers concurrent basé sur des sockets (AF_INET/SOCK_STREAM) pour gérer des transactions entre plusieurs clients. Chaque client peut demander des informations sur des fichiers résidant sur le serveur via des commandes envoyées par socket.

Le serveur démarre un **thread** distinct pour chaque client afin de traiter ses requêtes simultanément.

---

## Fonctionnalités
- Connexion client/serveur par socket TCP.
- Gestion de plusieurs clients de façon concurrente avec **threads**.
- Récupération des informations sur les fichiers avec `stat()`.
- Affichage des informations suivantes :
  - Numéro d'inode (`st_ino`)
  - Nombre de liens (`st_nlink`)
  - Taille en octets (`st_size`)
  - Nombre de blocs alloués (`st_blocks`)
- Liste des fichiers ouverts avec la commande `F`
- Mapping mémoire du processus avec la commande `M`
- Gestion des requêtes multiples par un même client.

---

## Commandes

| Commande       | Description                          |
|---------------|-------------------------------------|
| `s <filename>` | Demande des informations sur un fichier. |
| `F`           | Affiche la liste des fichiers ouverts du processus serveur. |
| `M`           | Affiche les informations des régions mémoire du processus serveur. |
| `quit`        | Déconnecte le client du serveur. |

---

## Architecture
1. **Client :**  
   - Envoie des commandes via socket.  
   - Reçoit les informations et les affiche dans la console.  
   
2. **Serveur :**  
   - Accepte les connexions des clients.
   - Crée un **thread** pour chaque client.
   - Lit la commande et appelle la fonction associée :
     - `fileSTAT()` pour les informations de fichier.
     - `listFileTable()` pour les fichiers ouverts.
     - `listMemoryMap()` pour le mapping mémoire.

---

## Compilation
```bash
gcc server.c -o server -lpthread
gcc client.c -o client
