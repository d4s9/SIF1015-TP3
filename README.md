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



---

## Commandes
# Pour un utilisateur:
| Commande       | Description                          |
|---------------|-------------------------------------|
| `B` | Liste les fichiers OLC3 du dossier présent |
| `X # nom_fichier_olc3` | Lance l’exécution d’un fichier OLC3 avec affichage ou non (0 ou 1). Exemple : X 1 Mult-Numbers.olc3 |
| `Q`           | Quitte le programme |

# Pour un administrateur:
| Commande       | Description                          |
|---------------|-------------------------------------|
| `L #` | Liste les informations des VM (ex : L1-4 liste les infos des VM #1 à 4) |
| `E no_VM` | Efface la VM spécifiée (ex : E 2 efface la VM #2) |
| `P`           | Exécute ps –aux dans le terminal et affiche les processus actifs |
| `K #_de_TID`           | Termine l’exécution du fichier correspondant au TID (ex : K 2088) |

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
---
## Auteurs

Derek Stevens (STED71290300)
Yoan Tremblay (TREY87020409)
Nathan Pinard (PINN85050400)
Yannick Lemire (LEMY14307400)
Malyk Ratelle (RATM73060406)

---
Université du Québec à Trois-Rivières

SIF1015 - Système d’exploitation

---
Licence

Projet académique réalisé dans le cadre du cours SIF1015.
