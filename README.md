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
## Exemple d'utilisation

![image](https://github.com/user-attachments/assets/83be1cbd-5c10-485b-8e53-0307552b5fcc)
![image](https://github.com/user-attachments/assets/f2a2c077-446b-483a-96a0-5556d0b78fda)
![image](https://github.com/user-attachments/assets/7f426f1c-037b-4e26-9a6c-135de2bcf357)
![image](https://github.com/user-attachments/assets/d1dc86c7-87f0-4236-8b23-34754975a1f3)
![image](https://github.com/user-attachments/assets/3766887c-3100-480d-a20f-caf42aefa9cb)

---

## Compilation
```bash
gcc server.c -o server -lpthread
gcc client.c -o client
```

Exécution

Lancer le serveur :
```bash
./gestionVMS_MAIN
```
Lancer le client :
```bash
./main <adresse_ip_du_serveur>
```
Auteurs

Derek Stevens (STED71290300)
Yoan Tremblay (TREY87020409)
Nathan Pinard (PINN85050400)
Yannick Lemire (LEMY14307400)
Malyk Ratelle (RATM73060406)


Université du Québec à Trois-Rivières

SIF1015 - Système d’exploitation

Licence

Projet académique réalisé dans le cadre du cours SIF1015.
