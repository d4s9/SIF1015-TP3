/* Pour le cours : SIF1015
 * Travail : TP3
 * Session : AUT 2024
 * Étudiants:   Yoan Tremblay
 *              Nathan Pinard
 *              Derek Stevens
 *              Malyk Ratelle
 *              Yannick Lemire
 *
 *  client.c : Gestionnaire du client, on gère la fenêtre, les E/S et les socket
 *
 */

#include "client.h"
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

void sendInput(const char *input, int socket_fd);
/* Fonction : Création d'une fenêtre avec certains paramètres
 * Entrée : Paramètres de la fenêtre, Bordure ?, mode d'accès, titre, début de la fenêtre
 * Sortie : Un pointeur de fenêtre console
*/


/* Fonction : setupTerminalWindow
 * Description : Crée une fenêtre terminal personnalisée avec des options d'affichage et d'entrée.
 * Entrée :
 *    - boxed (bool const) : Si vrai, ajoute une bordure autour de la fenêtre.
 *    - writable (bool const) : Si vrai, active la saisie utilisateur dans la fenêtre.
 *    - title (char[]) : Le titre à afficher en haut de la fenêtre.
 *    - beginX (int const) : La position horizontale de départ de la fenêtre.
 * Sortie :
 *    - (WINDOW*) : Un pointeur vers la fenêtre créée.
 * Détails :
 * 1. Crée une fenêtre de largeur égale à la moitié de l'écran (`COLS/2`) et hauteur `LINES`.
 * 2. Si `boxed` est vrai, ajoute une bordure avec des caractères `|` (vertical) et `-` (horizontal).
 * 3. Affiche le titre centré horizontalement dans la partie supérieure de la bordure.
 * 4. Rafraîchit la fenêtre pour appliquer les changements.
 * 5. Crée une nouvelle sous-fenêtre à l'intérieur, réduite en taille pour éviter d'écraser la bordure.
 * 6. Si `writable` est vrai :
 *    - Active la prise en charge des touches spéciales (ex. flèches) avec `keypad`.
 *    - Désactive l'affichage immédiat des caractères saisis (`noecho`).
 * 7. Active le défilement du contenu de la fenêtre avec `scrollok`.
 * 8. Retourne un pointeur vers la fenêtre créée.
 */
WINDOW* setupTerminalWindow(bool const boxed, bool const writable, char title[], int const beginX) {
    WINDOW* win = newwin(LINES, COLS/2, 0, beginX); //Création de la fenêtre

    if(boxed) { //est-ce qu'on met des bordures
        box(win, '|', '-');
    }

    mvwprintw(win, 1, COLS/4-(int)strlen(title)/2, "%s",title); //Affichage du titre
    wrefresh(win); 

    win = newwin(LINES-3, COLS/2-2, 2, beginX+1); //Nouvelle fenêtre

    if(writable) { // Mode d'accès R/W  ?
        keypad(win, true);
        noecho();
    }
    scrollok(win, true); // active le scrolling

    return win;
}


/* Fonction : readInput
 * Description : Gère les entrées utilisateur depuis une fenêtre dédiée et les envoie au serveur via un socket.
 * Entrée :
 *    - args (void*) : Un pointeur vers une structure `window_socket` contenant :
 *        - win (WINDOW*) : Fenêtre pour la saisie utilisateur.
 *        - fd (int) : Descripteur de fichier pour le socket de communication.
 *        - address (struct sockaddr_in) : Adresse associée au socket (non utilisée dans cette fonction).
 * Sortie :
 *    - void* : Aucun retour utile, la fonction termine le thread avec `pthread_exit`.
 * Détails :
 * 1. Récupère les paramètres passés via `args` et libère immédiatement la mémoire allouée pour ces paramètres.
 * 2. Alloue dynamiquement un tampon (`input`) pour stocker la ligne de commande saisie.
 * 3. Affiche un indicateur de saisie (`> `) dans la fenêtre.
 * 4. Boucle pour capturer les entrées utilisateur :
 *    - Si l'utilisateur appuie sur **Entrée** (`key == 10`) :
 *        a) Vérifie si l'entrée est `q` ou `Q` pour quitter.
 *        b) Envoie la ligne saisie au serveur via `sendInput`.
 *        c) Réinitialise le tampon `input` et ajoute un nouvel indicateur de ligne dans la fenêtre.
 *    - Si l'utilisateur appuie sur **Backspace** (`key == 263`) :
 *        a) Supprime le dernier caractère dans le tampon `input`.
 *        b) Met à jour la fenêtre pour refléter la suppression.
 *    - Si l'utilisateur entre un caractère valide :
 *        a) Ajoute le caractère au tampon `input`.
 *        b) Affiche le caractère dans la fenêtre.
 * 5. Continue la boucle tant qu'aucune erreur (`key != ERR`) n'est détectée.
 * 6. Avant de quitter :
 *    - Envoie le dernier tampon d'entrée au serveur.
 *    - Libère la mémoire allouée pour `input`.
 *    - Ferme le socket avec `close`.
 *    - Termine le thread avec `pthread_exit`.
 */
void* readInput(void* args) {
    /* 
     * Étape : Récupération et initialisation des paramètres de la structure `window_socket`
     * 
     * 1. Conversion de l'argument `args` :
     *    - L'argument `args` est un pointeur générique (`void*`) passé au thread.
     *    - Il est converti en un pointeur vers une structure `window_socket`, car c'est ainsi
     *      que les données nécessaires sont transmises à cette fonction.
     *    - Cette structure contient trois éléments essentiels pour le fonctionnement du thread :
     *      - Un pointeur vers la fenêtre console (`WINDOW* win`) pour gérer l'affichage et les entrées.
     *      - Un descripteur de socket (`int fd`) pour la communication avec le serveur.
     *      - Une adresse réseau (`struct sockaddr_in address`) associée au socket (bien que non utilisée ici).
     */
    struct window_socket* ptr = args;
    /*
     * Étape : Récupération de la fenêtre console
     * - `ptr->win` contient un pointeur vers la fenêtre console utilisée pour la saisie utilisateur.
     * - Cette fenêtre a été créée et initialisée dans la fonction principale (ex. avec `setupTerminalWindow`).
     * - Le pointeur est assigné à `input_window` pour simplifier l'accès dans le reste de la fonction.
     */
    WINDOW* input_window = ptr->win;
    /*
     * Étape : Récupération du descripteur de fichier du socket
     * - `ptr->fd` est le descripteur de fichier du socket TCP établi pour communiquer avec le serveur.
     * - Ce socket est utilisé pour envoyer les commandes utilisateur (ex. via `sendInput`).
     * - Le descripteur est stocké dans `socket_fd` pour faciliter son utilisation dans la fonction.
     */
    int socket_fd = ptr->fd; 
    /*
     * Étape : Récupération de l'adresse réseau (non utilisée ici)
     * - `ptr->address` contient l'adresse réseau associée au socket, sous la forme d'une structure `sockaddr_in`.
     * - Cette structure contient des informations comme :
     *     - `sin_family` : le type de protocole réseau (ex. IPv4 avec `AF_INET`).
     *     - `sin_addr` : l'adresse IP (ex. "127.0.0.1" pour localhost).
     *     - `sin_port` : le numéro de port utilisé par le serveur.
     * - Bien que l'adresse soit extraite ici dans la variable `address`, elle n'est pas utilisée dans cette fonction.
     */
    struct sockaddr_in address = ptr->address;

    free(args);

    char* input = malloc(200*sizeof(char)); // réserve la mémoire pour le command line

    wprintw(input_window, "> "); // Met un pointeur de ligne
    int key = wgetch(input_window); // attend la touche

    while(key != ERR) { // Tant que l'entrée de la clé n'est pas une erreur
        if(key == 10) { // Enter key
            if(input[0] == 'q' || input[0] == 'Q') break; // est-ce qu'on sort, quit?
            /*
             * Étape 1 : Envoi de la commande utilisateur au serveur
             * - `sendInput(input, socket_fd);`
             *    - Appelle la fonction `sendInput` pour envoyer la commande actuellement saisie au serveur.
             *    - Paramètres passés :
             *        - `input` : La commande utilisateur (sous forme de chaîne de caractères).
             *        - `socket_fd` : Le descripteur de fichier du socket, qui identifie la connexion active avec le serveur.
             *    - La fonction `sendInput` encapsule cette commande dans une structure 
             *      (`Info_FIFO_Transaction`) avec le PID du processus client et l'écrit dans le socket.
             */
            sendInput(input, socket_fd); 
            /*
             * Étape 2 : Réinitialisation du tampon d'entrée
             * - `memset(input, 0, strlen(input) * sizeof(char));`
             *    - Utilise `memset` pour réinitialiser le contenu de la chaîne `input` à zéro.
             *    - Paramètres :
             *        - `input` : Le tampon à réinitialiser.
             *        - `0` : Valeur à attribuer (ici, `0` pour chaque octet).
             *        - `strlen(input) * sizeof(char)` : Taille des données actuelles dans le tampon.
             *    - Cela garantit que l'ancien contenu du tampon est effacé avant de lire une nouvelle commande.
             *    - Important pour éviter des données résiduelles dans le tampon si l'utilisateur tape une commande plus courte ensuite.
             */
            memset(input, 0, strlen(input) * sizeof(char));

            wprintw(input_window,"\n> "); // simule le retour de ligne à l'écran
        } else { // on gère l'entrée de la console
            const char* check = unctrl(key); // valide la clé

            if(key == 263) {
                /*
                 * Étape 1 : Gestion du caractère "Backspace"
                 * - Vérifie si l'utilisateur a appuyé sur la touche "Backspace" (code 263).
                 * - Si oui, supprime le dernier caractère du tampon `input` :
                 *     - Réduit la longueur effective de la chaîne en remplaçant le dernier caractère
                 *       par le caractère nul (`\0`).
                 */
                input[strlen(input)-1] = '\0'; // efface 
                int x, y;
                getyx(input_window, y, x); // va chercher la position de la souris 
                (void) y; //Suppress the not used warning
                if(x > 2) {
                    wprintw(input_window, "\b \b"); // simule le backspace à l'écran
                    wrefresh(input_window); //affiche la fenêtre 
                }
            } // Sinon caractère valide et on l'affiche
            else if(check != 0 && strlen(check) == 1) {
                input[strlen(input)] = check[0]; 
                waddch(input_window, check[0]);// ajoute le car à l'écran
            }
        }
        /*
         * Étape 1 : Rafraîchissement de la fenêtre
         * - `wrefresh(input_window);`
         *    - Met à jour l'affichage de la fenêtre `input_window` pour refléter les changements récents.
         *    - Cela inclut :
         *        - L'ajout de nouveaux caractères saisis par l'utilisateur.
         *        - Les modifications visuelles, comme l'effacement avec Backspace.
         *    - Sans cette étape, les changements effectués dans la fenêtre ne seraient pas visibles à l'écran.
         *    - `wrefresh` force le système à synchroniser le contenu du tampon de la fenêtre avec l'affichage réel.
         */
        wrefresh(input_window);
        /*
         * Étape 2 : Lecture de la prochaine entrée utilisateur
         * - `key = wgetch(input_window);`
         *    - Attend que l'utilisateur appuie sur une touche dans la fenêtre `input_window`.
         *    - Retourne le code de la touche pressée, qui est stocké dans la variable `key`.
         *    - Cas spécifiques :
         *        - Si une touche alphanumérique ou spéciale est pressée, son code est capturé (par ex., `10` pour Entrée, `263` pour Backspace).
         *        - Si aucune touche n'est pressée avant une certaine durée et que la fenêtre est configurée en mode non-bloquant, retourne `ERR`.
         *    - Cela permet au programme de réagir dynamiquement aux saisies utilisateur et de boucler jusqu'à ce que la commande soit terminée.
         */
        key = wgetch(input_window);
    }
    /*
     * Étape 1 : Envoi de la commande au serveur
     * - `sendInput(input, socket_fd);`
     *    - Envoie la commande utilisateur saisie au serveur via le socket.
     *    - La fonction `sendInput` encapsule la commande dans une structure (avec le PID du client)
     *      et écrit cette structure sur le socket identifié par `socket_fd`.
     *    - Cette étape finalise la transmission de la commande au serveur, pour qu'elle puisse être exécutée.
     */
    sendInput(input, socket_fd);
    /*
     * Étape 2 : Libération de la mémoire
     * - `free(input);`
     *    - Libère l'espace mémoire alloué dynamiquement pour le tampon `input` (200 caractères).
     *    - Cette mémoire avait été réservée en début de fonction pour stocker la commande utilisateur.
     *    - Libérer cette mémoire est important pour éviter une fuite mémoire,
     *      surtout dans un programme multi-threadé où de nombreuses commandes peuvent être saisies.
     */
    free(input);
    /*
     * Étape 3 : Fermeture du socket
     * - `close(socket_fd);`
     *    - Ferme le socket de communication avec le serveur.
     *    - Cela signifie que ce client a terminé ses interactions avec le serveur.
     *    - Une fois le socket fermé, aucune autre donnée ne peut être envoyée ou reçue via cette connexion.
     *    - Important pour libérer les ressources système associées au socket.
     */
    close(socket_fd);
    /*
     * Étape 4 : Fermeture du thread
     * - `pthread_exit(0);`
     *    - Termine proprement le thread en cours.
     *    - Retourne une valeur de sortie (ici `0`), indiquant que le thread s'est terminé normalement.
     *    - Cette étape est essentielle pour informer le système que le thread a fini son exécution,
     *      évitant ainsi des problèmes liés à des threads "zombies".
     */
    pthread_exit(0);
}

/*
 * Fonction : sendInput
 * Description : Envoie une commande utilisateur au serveur via un socket.
 * Entrée :
 *    - `input` (const char*) : Chaîne représentant la commande à envoyer au serveur.
 *    - `socket_fd` (const int) : Descripteur du socket utilisé pour la communication.
 * Sortie : Aucune (void).
 * Détails :
 */
void sendInput(const char *input,const int socket_fd) {
    /*
     * Étape 1 : Allocation de mémoire pour une structure transactionnelle
     * - `struct Info_FIFO_Transaction* ptr = malloc(sizeof(struct Info_FIFO_Transaction));`
     *    - Alloue dynamiquement une structure `Info_FIFO_Transaction` en mémoire.
     *    - Cette structure est utilisée pour encapsuler les données nécessaires :
     *        - Le PID (identifiant unique du processus client).
     *        - La commande utilisateur à transmettre.
     */
    struct Info_FIFO_Transaction* ptr = malloc(sizeof(struct Info_FIFO_Transaction));
    /*
     * Étape 2 : Remplissage de la structure
     * - `ptr->pid_client = getpid();`
     *    - Le champ `pid_client` est défini avec le PID du processus appelant.
     *    - Cela permet au serveur d'identifier la source de la commande.
     * 
     * - `strcpy(ptr->transaction, input);`
     *    - Copie le contenu de la commande `input` dans le champ `transaction` de la structure.
     *    - Assure que la commande est bien encapsulée pour être transmise au serveur.
     */
    ptr->pid_client = getpid();
    strcpy(ptr->transaction, input);
    /*
     * Étape 3 : Envoi des données via le socket
     * - `write(socket_fd, ptr, sizeof(*ptr));`
     *    - Utilise la fonction système `write` pour envoyer les données au serveur via le socket.
     *    - Les données envoyées incluent :
     *        - Le PID du client.
     *        - La commande encapsulée dans la structure `Info_FIFO_Transaction`.
     *    - Le descripteur `socket_fd` identifie la connexion réseau active avec le serveur.
     */
    write(socket_fd, ptr, sizeof(*ptr));
    /*
     * Étape 4 : Libération de la mémoire
     * - `free(ptr);`
     *    - Libère l'espace mémoire alloué dynamiquement pour la structure `Info_FIFO_Transaction`.
     *    - Cela évite une fuite mémoire, essentielle dans les programmes qui utilisent des allocations dynamiques répétées.
     */
    free(ptr);
}

/*
 * Fonction : readOutput
 * Description : Lit les données envoyées par le serveur via le socket et les affiche dans une fenêtre.
 * Entrée :
 *    - args (void*): Un pointeur vers une structure `window_socket` contenant les informations nécessaires :
 *        - win (WINDOW*): La fenêtre de terminal dans laquelle afficher les données.
 *        - fd (int): Le descripteur de fichier du socket.
 *        - address (struct sockaddr_in): Adresse associée au socket (non utilisée ici).
 * Sortie : void* (le thread se termine avec `pthread_exit`).
 * Détails :
 */
void* readOutput(void* args) {
    //Comme dans read input
    struct window_socket* ptr = args;
    WINDOW* win = ptr->win;
    const int socket_fd = ptr->fd;
    struct sockaddr_in address = ptr->address;
    free(args);
    /*
     * Étape 2 : Boucle de lecture et affichage des données
     * - `ssize_t read_res;`
     *    - Déclare une variable pour stocker le résultat de la fonction `read` (nombre de bytes lus).
     *
     * - `do { char output[200] = {0}; read_res = read(socket_fd, output, sizeof(output));`
     *    - Boucle qui lit les données du socket tant que des données sont disponibles (`read_res > 0`).
     *    - `read` lit les données envoyées par le serveur sur le socket `socket_fd` et les stocke dans le tampon `output`.
     *    - `sizeof(output)` définit la taille maximale du tampon à 200 octets.
     *
     * - `if(read_res == -1) { perror("Error"); break; }`
     *    - Si `read` retourne `-1`, cela indique une erreur lors de la lecture.
     *    - `perror("Error")` affiche un message d'erreur pour aider au débogage.
     *    - La boucle s'arrête en cas d'erreur.
     *
     * - `wprintw(win, "%s", output);`
     *    - Affiche les données lues (`output`) dans la fenêtre terminal.
     *    - Utilise `wprintw` pour écrire directement dans la fenêtre de terminal.
     *
     * - `wrefresh(win);`
     *    - Rafraîchit la fenêtre pour s'assurer que les nouvelles données sont affichées à l'écran.
     */
    ssize_t read_res;
    do{ // Boucle 
        char output[200] = {0};
        read_res = read(socket_fd, output, sizeof(output));
        if(read_res == -1) {
            perror("Error");
            break;
        }

        wprintw(win,"%s", output);// Sinon écrit dans la fenêtre
        wrefresh(win); // Affiche la fenêtre
    }while(read_res > 0); //  tant que la lecture du fichier retourne qqchose 
    pthread_exit(0); 
}

#pragma once
#include <ncurses.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

WINDOW* setupTerminalWindow(bool boxed, bool writable, char title[], int beginX);
void* readInput(void* args);
void* readOutput(void* args);

struct window_socket {
    int fd;
    struct sockaddr_in address;
    WINDOW* win;
};

struct Info_FIFO_Transaction {
    int pid_client;
    char transaction[200];
};

/* Pour le cours : SIF1015
 * Travail : TP3
 * Session : AUT 2024
 * Étudiants:   Yoan Tremblay
 *              Nathan Pinard
 *              Derek Stevens
 *              Malyk Ratelle
 *              Yannick Lemire
 *
 *  main.c : Fichier principal pour le démarrage des fenêtres et des files de processus
 *
 */

#include "client.h"
#include <stdlib.h>
/* Fonction : main
 * Description : Fonction principale du programme client pour établir une connexion avec le serveur, gérer les interactions utilisateur et gérer les threads pour la lecture des entrées et l'affichage des sorties.
 * Entrée :
 *    - argc (int) : Le nombre d'arguments passés via la ligne de commande.
 *    - argv (char*[]) : Les arguments passés via la ligne de commande.
 * Sortie :
 *    - int : 0 si le programme s'exécute correctement, ou un code d'erreur si un problème survient.
 * Détails :
 *
 * 1. **Initialisation du terminal en mode ncurses** :
 *    - `initscr()` initialise la bibliothèque ncurses, qui permet de gérer les fenêtres et l'interaction en mode console.
 *
 * 2. **Création des threads pour la lecture et l'affichage** :
 *    - `pthread_t read_input_thread` et `pthread_t read_output_thread` : Création des threads pour gérer respectivement la lecture des entrées utilisateur et l'affichage des données reçues du serveur.
 *    - `setupTerminalWindow(true, true, "TRANSMISSION", 0)` et `setupTerminalWindow(true, true, "RECEPTION", COLS/2)` : Crée deux fenêtres console, l'une pour la saisie (`TRANSMISSION`) et l'autre pour l'affichage des réponses du serveur (`RECEPTION`).
 *
 * 3. **Connexion au serveur via un socket TCP/IP** :
 *    - `socket_fd = socket(AF_INET, SOCK_STREAM, 0)` : Crée un socket de type TCP/IPv4.
 *    - `connect(socket_fd, (struct sockaddr*)&address, sizeof(address))` : Établit une connexion avec le serveur sur `127.0.0.1` à port `1236`.
 *    - Si la connexion échoue, une erreur est signalée via `perror`, et le thread se termine.
 *
 * 4. **Création et initialisation des structures de communication** :
 *    - `struct window_socket* inputArg` et `outputArg` : Structures contenant le descripteur de socket, l'adresse et les fenêtres.
 *    - Ces structures sont passées aux fonctions `readInput` et `readOutput` via `pthread_create`.
 *
 * 5. **Création et attachement des threads** :
 *    - `pthread_create(&read_input_thread, NULL, &readInput, inputArg)` : Crée un thread pour gérer la saisie des commandes utilisateur.
 *    - `pthread_create(&read_output_thread, NULL, &readOutput, outputArg)` : Crée un thread pour gérer l'affichage des données reçues du serveur.
 *    - `pthread_join(read_input_thread, NULL)` et `pthread_join(read_output_thread, NULL)` : Attend la fin de ces threads pour assurer une exécution synchronisée.
 *
 * 6. **Nettoyage du terminal** :
 *    - `endwin()` : Libère les ressources utilisées par ncurses et restaure le terminal.
 */
int main(int argc, char* argv[]){
    /*
     * Étape 1. **Initialisation du terminal en mode ncurses** :
     *    - `initscr()` initialise la bibliothèque ncurses, qui permet de gérer les fenêtres et l'interaction en mode console.
     */
    initscr();
    /* Étape 2. **Création des threads pour la lecture et l'affichage** :
     *    - `pthread_t read_input_thread` et `pthread_t read_output_thread` : Création des threads pour gérer 
     *       respectivement la lecture des entrées utilisateur et l'affichage des données reçues du serveur.
     *    - `setupTerminalWindow(true, true, "TRANSMISSION", 0)` et `setupTerminalWindow(true, true, "RECEPTION", COLS/2)` :
     *       Crée deux fenêtres console, l'une pour la saisie (`TRANSMISSION`) et l'autre pour l'affichage des réponses du serveur (`RECEPTION`).
     */
    pthread_t read_input_thread;
    pthread_t read_output_thread;

    WINDOW* inputWindowPtr = setupTerminalWindow(true, true, "TRANSMISSION", 0); 
    WINDOW* outputWindowPtr = setupTerminalWindow(true, true, "RECEPTION", COLS/2);
    /*
     * Étape 1 : Initialisation de la structure sockaddr_in
     * - `struct sockaddr_in address;`
     *    - Déclare une structure `sockaddr_in` qui représente une adresse réseau IPv4.
     *    - Cette structure est utilisée pour stocker l'adresse IP, le port et la famille de protocole.
     */
    struct sockaddr_in address;
    /*
     * Étape 2 : Création d'un socket IPV4 de type TCP
     * - `int socket_fd = socket(AF_INET, SOCK_STREAM, 0);`
     *    - Crée un socket de type TCP/IP de la famille `AF_INET` (IPv4).
     *    - `socket()` retourne un descripteur de fichier du socket.
     *    - `AF_INET` spécifie que le socket utilise IPv4.
     *    - `SOCK_STREAM` indique que le type de socket est de type TCP (connexion orientée).
     *    - Le `0` est pour indiquer que le système choisit le protocole approprié (TCP par défaut).
     */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0); 
    /*
     * Étape 3 : Configuration de l'adresse du serveur
     * - `address.sin_family = AF_INET;`
     *    - Spécifie la famille de protocole (IPv4).
     * - `address.sin_addr.s_addr = inet_addr("127.0.0.1");`
     *    - Définie l'adresse IP (`127.0.0.1`) de loopback pour la communication locale.
     *    - `inet_addr` convertit la chaîne `"127.0.0.1"` en format binaire.
     * - `address.sin_port = 1236;`
     *    - Définie le port utilisé pour la communication (`1236` ici).
     */
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    address.sin_port = 1236; 
    /*
     * Étape 4 : Établissement de la connexion avec le serveur
     * - `int result = connect(socket_fd, (struct sockaddr*)&address, sizeof(address));`
     *    - Utilise la fonction `connect` pour établir une connexion avec le serveur.
     *    - Le `socket_fd` est le descripteur du socket créé précédemment.
     *    - `address` contient les informations du serveur (adresse et port).
     *    - `sizeof(address)` donne la taille de la structure `sockaddr_in`.
     *    - `connect` retourne `0` en cas de succès ou `-1` en cas d'erreur.
     */
    int result = connect(socket_fd, (struct sockaddr*)&address, sizeof(address));
    /*
     * Étape 5 : Gestion d'une erreur d'ouverture de socket
     * - `if(result == -1) { perror("Error opening file"); pthread_exit(0); }`
     *    - Si la connexion échoue (`result == -1`), une erreur est signalée via `perror`.
     *    - `pthread_exit(0)` termine le thread courant proprement.
     *    - Cette étape garantit que si la connexion ne peut pas être établie, le programme se termine proprement pour éviter des comportements inattendus.
     */
    if(result == -1) { // erreur d'ouverture de socket 
        perror("Error opening file"); 
        pthread_exit(0); 
    }
    /*
     * Étape 1 : Allocation de mémoire pour les structures `window_socket`
     * - `struct window_socket* inputArg = malloc(sizeof(struct window_socket));`
     *    - Alloue dynamiquement la mémoire pour une structure `window_socket` pour stocker les paramètres liés aux entrées utilisateur.
     *    - Alloue dynamiquement la mémoire pour une structure `window_socket` pour stocker les paramètres liés aux données affichées par le serveur.
     */
    struct window_socket* inputArg = malloc(sizeof(struct window_socket)); // Création de structures pour contenir les commandes entrées
    struct window_socket* outputArg = malloc(sizeof(struct window_socket)); // Création de structures pour contenir les données à afficher
    /*
     * Étape 2 : Initialisation des structures `window_socket` pour les entrées et les sorties
     * - `inputArg->fd = socket_fd;`
     *    - Initialise la structure `inputArg` avec le descripteur de fichier du socket (pour les entrées utilisateur).
     * - `inputArg->address = address;`
     *    - Initialise la structure `inputArg` avec l'adresse du serveur.
     * - `inputArg->win = inputWindowPtr;`
     *    - Associe la fenêtre d'entrée (`inputWindowPtr`) à la structure pour afficher les entrées utilisateur.
     */
    inputArg->fd = socket_fd;
    inputArg->address = address;
    inputArg->win = inputWindowPtr;

    outputArg->address = address; // l'adresse du client
    outputArg->fd = socket_fd; // le socket utilisé en sortie
    outputArg->win = outputWindowPtr; // un lien vers la fenêtre de sortie 
    /*
     * Étape 3 : Création de threads pour la gestion des entrées et des sorties
     * - `pthread_create(&read_input_thread, NULL, &readInput, inputArg);`
     *    - Crée un thread pour exécuter la fonction `readInput`, qui gère la lecture des commandes utilisateur.
     *    - `inputArg` passe les paramètres nécessaires (socket et fenêtre) à la fonction.
     *
     * - `pthread_create(&read_output_thread, NULL, &readOutput, outputArg);`
     *    - Crée un thread pour exécuter la fonction `readOutput`, qui gère l'affichage des réponses du serveur.
     *    - `outputArg` passe les paramètres nécessaires (socket et fenêtre) à la fonction.
     */
    pthread_create(&read_input_thread, NULL, &readInput, inputArg); 
    pthread_create(&read_output_thread, NULL, &readOutput, outputArg);
     /*
     * Étape 4 : Attente de la fin des threads
     * - `pthread_join(read_input_thread, NULL);`
     *    - Attend la fin du thread `read_input_thread` pour s'assurer que toutes les opérations de lecture des entrées utilisateur sont complètes.
     * - `pthread_join(read_output_thread, NULL);`
     *    - Attend la fin du thread `read_output_thread` pour s'assurer que toutes les opérations d'affichage des réponses du serveur sont complètes.
     */   
    pthread_join(read_input_thread, NULL);
    pthread_join(read_output_thread, NULL);
    /*
     * Étape 5 : Nettoyage du terminal
     * - `endwin();`
     *    - Libère les ressources utilisées par la bibliothèque `ncurses` pour restaurer l'état du terminal.
     */
    endwin(); 
    return 0;
}

#include <stdio.h>
#include <math.h>

int main() {
    FILE *fp;         // Fichier source binaire.
    FILE *fpBin;      // Fichier de sortie binaire.
    int i, c;         // Variables pour la boucle.
    unsigned short sum = 0; // Variable pour stocker le nombre décimal converti.

    // Ouverture du fichier source en mode lecture.
    fp = fopen("Mult-NumbersErreur.bin", "r");
    if (fp == NULL) {
        // Si l'ouverture du fichier échoue, affiche une erreur et retourne -1.
        printf("Error in opening file");
        return -1;
    }

    // Ouverture du fichier de sortie en mode écriture binaire.
    fpBin = fopen("Mult-NumbersErreur.olc3", "wb");
    if (fpBin == NULL) {
        // Si l'ouverture du fichier de sortie échoue, ferme le fichier source et affiche une erreur.
        fclose(fp);
        printf("Error in opening output file");
        return -1;
    }

    // Positionne le curseur à la fin du fichier source pour déterminer sa taille.
    fseek(fp, 0, SEEK_END);
    long nbcar = ftell(fp); // Calcul de la taille du fichier.
    printf("nbcar = %ld\n", nbcar); // Affiche la taille du fichier.
    fseek(fp, 0, SEEK_SET); // Remet le curseur au début du fichier.

    // Boucle principale pour lire chaque byte.
    while (nbcar) {
        c = fgetc(fp); // Lecture du prochain byte.

        // Si ce n'est pas un '0' ou '1', ignore le byte.
        if (!(c == '0' || c == '1')) {
            nbcar--; // Diminue le compteur de bytes restants.
            printf("nbcar = %ld\n", nbcar); // Affiche le nombre de bytes restants.
        } else {
            sum = 0;  // Initialisation de la somme.
            i = 0;

            // Convertit chaque byte de bits (16 bits max).
            while (i < 16) {
                if (c == '0' || c == '1') { 
                    sum += (unsigned short)((c - 30) * pow(2.0, 15 - i)); // Conversion de binaire à décimal.
                    i++;
                }
                nbcar--; // Diminue le compteur de bytes restants.
                printf("nbcar = %ld\n", nbcar);
                c = fgetc(fp); // Lecture du prochain byte.
            }

            // Écrit la somme décimale convertie dans le fichier de sortie.
            fwrite(&sum, 2, 1, fpBin);
        }
    }

    // Fermeture des fichiers.
    fclose(fp);
    fclose(fpBin);

    return 0;
}


/* Pour le cours : SIF1015
 * Travail : TP3
 * Session : AUT 2024
 * Étudiants:   Yoan Tremblay
 *              Nathan Pinard
 *              Derek Stevens
 *              Malyk Ratelle
 *              Yannick Lemire
 *
 *  gestionlisteChaineeVMS.c :  Basé sur l'utilitaires Liste Chainee et CVS LINUX Automne 24 de François Meunier
 *                              avec modifications pour la gestion de communication client/serveur
 *
 */

#include "gestionListeChaineeVMS.h"
#include "gestionVMS.h"
#include <unistd.h>

//Pointeur de tête de liste
extern struct noeudVM* head;
//Pointeur de queue de liste pour ajout rapide
extern struct noeudVM* queue;
// nombre de VM actives
extern int nbVM;

extern sem_t semNbVM, semQ, semH, semC;

struct client_FIFO_Info* getClientPathName(int client_pid);
void sendOutput(int client_fifo_fd, char const * msg);

//#######################################
//#
//# Affiche une série de retour de ligne pour "nettoyer" la console
//#
void cls(void){
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    }

//#######################################
//#
//# Affiche un message et quitte le programme
//#
void error(const int exitcode, const char * message){
    printf("\n-------------------------\n%s\n",message);
    exit(exitcode);
    }
    
/* Sign Extend */
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

/* Swap */
uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

/* Update Flags */
void update_flags(uint16_t reg[R_COUNT], uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}


/* Validation of an adress generated by a program  */
int validAdress(uint16_t * memory,uint16_t  offset, struct noeudVM * ptr, char accessType)
{
    
    if(((memory+offset) > (memory+65535)) || ((memory) > (memory+offset)) )
    {
        sem_wait(&semC);
        printf("\n Adresse invalide");
        sem_post(&semC);
        return 0; 
    }   
    if (accessType == 'W') { // validate if in code region
            if(((memory+offset) > (memory+ptr->VM.offsetDebutCode)) && ((memory+ptr->VM.offsetFinCode) > (memory+offset)))
            {
                sem_wait(&semC);
                printf("\n Adresse invalide en Write");
                sem_post(&semC);
                return 0;
            }               
    }       
    return 1;
}

/* Read Image File */
int read_image_file(uint16_t * memory, char* image_path,uint16_t * origin,  struct noeudVM * ptr)
{
     char fich[200];
     strcpy(fich,image_path);
     
     if(fich[strlen(fich)-1]=='\r')
        fich[strlen(fich)-1]='\0';
     FILE* file = fopen(fich, "rb");
 
    if (!file) { return 0; }
    /* the origin tells us where in memory to place the image */
    *origin=0x3000;

    /* we know the maximum file size so we only need one fread */
    uint16_t max_read = UINT16_MAX - *origin;
    uint16_t* p = memory + *origin;
    ptr->VM.offsetDebutCode = *origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);
    ptr->VM.offsetFinCode = *origin+read-1;
    ptr->VM.DimRamUsed = read;// Ram utilisee par la VM 
    /* swap to little endian ???? */
    while (read-- > 0)
    {
    //  printf("\n p * BIG = %x",*p);
       // *p = swap16(*p);
        // printf("\n p * LITTLE = %x",*p);
        ++p;
    }
    return 1;
}


/* Check Key */
uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

/* Memory Access */
void mem_write(uint16_t * memory, uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t mem_read( uint16_t * memory, uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}

/* Input Buffering */
struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

/* Handle Interrupt */
void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}


/* Fonction : Execute le fichier de code .olc3
 * Entrée : une struct contenant les arguments de l'appel
 * Sortie : un pointeur vers une struct nulle ou non
*/
void* executeFile(void* args){
    /* Désérialisation des arguments transmis à la fonction */
    struct paramX* param = args;// Récupération de la structure paramX contenant les arguments.
    char sourcefname[100]; // Tableau de caractères pour stocker le nom du fichier source.

    int client_socket_fd = param->client_socket_fd; // Récupération du descripteur de socket du client.
    int p = param->boolPrint; // Récupération de l'indicateur pour la sortie d'impression.
    strcpy(sourcefname, param->fileName); // Copie du nom du fichier source.
    free(param); // Libération de la mémoire allouée pour la structure paramX.

/* Memory Storage */
/* 65536 locations */
    uint16_t *  memory;
    uint16_t origin;
    uint16_t PC_START;
    
/* Register Storage */
    uint16_t reg[R_COUNT];
    struct noeudVM * ptr = findFreeVM();

    if(ptr == NULL) // Aucune VM libre 
    {
        sendOutput(client_socket_fd, "Virtual Machine ajoutee\n");
        ptr = addItem();// Crée une nouvelle VM.
    }

    memory = ptr->VM.ptrDebutVM; // Stocke le pointeur vers la mémoire de la VM.

    //Chargement du fichier image dans la mémoire de la VM 
    if (!read_image_file(memory, sourcefname, &origin, ptr))
    {
        // Allocation de mémoire pour stocker le message d'erreur.
        char *msg = malloc(100*sizeof(char));
        // Formattage du message d'erreur indiquant que le chargement de l'image a échoué.
        sprintf(msg, "Failed to load image: %s\n", sourcefname);
        // Envoie du message d'erreur au client via sendOutput.
        sendOutput(client_socket_fd, msg);
        // Libération de la mémoire allouée pour le message d'erreur.
        free(msg);
        // Signal que la tâche de chargement est terminée, que la VM est disponible pour d'autres processus.
        sem_post(&ptr->semN);
        // Terminaison du thread avec exit code 0, signalant que l'exécution a échoué.
        pthread_exit(0);
    }
    /* 
     * Étape : Acquisition de l'accès à la VM.
     * - `ptr->VM.busy = 1;` : Marque la machine virtuelle comme occupée pour signaler qu'elle est en cours d'utilisation.
     * - `ptr->VM.tid = pthread_self();` : Stocke l'ID du thread actuel (tid) dans la VM. Ce TID est utilisé pour identifier le thread qui utilise la VM.
     * - `sem_post(&ptr->semN);` : Libère le sémaphore pour signaler que l'accès à la VM est maintenant acquis.
     */
    // Acquiring access to the VM
    ptr->VM.busy = 1;
    ptr->VM.tid = pthread_self(); // Doit récupérer le pthread ID directement puisque le TID réfère à une autre table
    sem_post(&ptr->semN);
    
    /* Setup */
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    /* set the PC to starting position */
    /* at  ptr->VM.ptrDebutVM + 0x3000 is the default  */
    //enum { PC_START = origin };
    PC_START = origin;
    reg[R_PC] = PC_START;
    uint16_t instr;
    uint16_t op;
    int running = 1;
    while (running)
    {
        pthread_testcancel();
        // FETCH
        if(validAdress(memory, reg[R_PC], ptr, 'R'))
        {
            instr = mem_read(memory, reg[R_PC]++);
        }
        else //invalid adress
        {
            running = 0;
            sendOutput(client_socket_fd, "Program abort memory out of bound\n");
            continue;
        }
        op = instr >> 12;
        switch (op)
        {
            case OP_ADD:
                // ADD
                {
                    // destination register (DR)
                    uint16_t r0 = (instr >> 9) & 0x7;
                    // first operand (SR1)
                    uint16_t r1 = (instr >> 6) & 0x7;
                    // whether we are in immediate mode
                    uint16_t imm_flag = (instr >> 5) & 0x1;
                
                    if (imm_flag)
                    {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[r0] = reg[r1] + imm5;
                    }
                    else
                    {
                        uint16_t r2 = instr & 0x7;
                        reg[r0] = reg[r1] + reg[r2];
                        if(p){
                            char *msg = malloc(100*sizeof(char));
                            sprintf(msg, "add reg[r0] (sum) = %d\n", reg[r0]);
                            sendOutput(client_socket_fd, msg);
                            free(msg);
                        }
                    }
                    update_flags(reg, r0);
                }
                break;
            case OP_AND:
                // AND
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t imm_flag = (instr >> 5) & 0x1;
                
                    if (imm_flag)
                    {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[r0] = reg[r1] & imm5;
                    }
                    else
                    {
                        uint16_t r2 = instr & 0x7;
                        reg[r0] = reg[r1] & reg[r2];
                    }
                    update_flags(reg, r0);
                }
                break;
            case OP_NOT:
                // NOT
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                
                    reg[r0] = ~reg[r1];
                    update_flags(reg, r0);
                }
                break;
            case OP_BR:
                // BR
                {
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    uint16_t cond_flag = (instr >> 9) & 0x7;
                    if (cond_flag & reg[R_COND])
                    {
                        reg[R_PC] += pc_offset;
                    }
                }
                break;
            case OP_JMP:
                // JMP
                {
                    // Also handles RET
                    uint16_t r1 = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[r1];
                }
                break;
            case OP_JSR:
                // JSR
                {
                    uint16_t long_flag = (instr >> 11) & 1;
                    reg[R_R7] = reg[R_PC];
                    if (long_flag)
                    {
                        uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                        reg[R_PC] += long_pc_offset;  // JSR
                    }
                    else
                    {
                        uint16_t r1 = (instr >> 6) & 0x7;
                        reg[R_PC] = reg[r1]; // JSRR
                    }
                    break;
                }
                break;
            case OP_LD:
                // LD
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    if(validAdress(memory, reg[R_PC]+ pc_offset, ptr, 'R'))
                    {
                        reg[r0] = mem_read(memory, reg[R_PC] + pc_offset); // valider adresse afficher message erreur mem et running = 0
                        update_flags(reg, r0);
                    }
                    else //invalid adress
                    {
                        running = 0;
                        sendOutput(client_socket_fd, "Program abort memory out of bound\n");
                    }
                }
                break;
            case OP_LDI:
                // LDI
                {
                    // destination register (DR)
                    uint16_t r0 = (instr >> 9) & 0x7;
                    // PCoffset 9
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    // add pc_offset to the current PC, look at that memory location to get the final address
                    if(validAdress(memory, reg[R_PC]+ pc_offset, ptr, 'R'))
                    {
                        reg[r0] = mem_read(memory, mem_read(memory, reg[R_PC] + pc_offset)); // valider adresse afficher message erreur mem et running = 0
                        update_flags(reg, r0);
                    }
                    else //invalid adress
                    {
                        running = 0;
                        sendOutput(client_socket_fd, "Program abort memory out of bound\n");
                    }
                }
                break;
            case OP_LDR:
                // LDR
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t offset = sign_extend(instr & 0x3F, 6);
                    if(validAdress(memory, reg[r1]+ offset, ptr, 'R'))
                    {
                        reg[r0] = mem_read(memory, reg[r1] + offset); // valider adresse afficher message erreur mem et running = 0
                        update_flags(reg, r0);
                    }
                    else //invalid adress
                    {
                        running = 0;
                        sendOutput(client_socket_fd, "Program abort memory out of bound\n");
                    }
                }
                break;
            case OP_LEA:
                 //LEA
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    reg[r0] = reg[R_PC] + pc_offset;
                    update_flags(reg, r0);
                }
                break;
            case OP_ST:
                // ST
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    if(validAdress(memory, reg[R_PC]+ pc_offset, ptr, 'W'))
                    {
                        mem_write(memory, reg[R_PC] + pc_offset, reg[r0]); // valider adresse afficher message erreur mem et running = 0
                    }
                    else //invalid adress
                    {
                        running = 0;
                        sendOutput(client_socket_fd, "Program abort memory out of bound or access violation\n");
                    }                
                }
                break;
            case OP_STI:
                // STI
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    if(validAdress(memory, reg[R_PC] + pc_offset, ptr, 'R'))
                    {
                        if(validAdress(memory, mem_read(memory, reg[R_PC] + pc_offset), ptr, 'W'))
                        {
                            mem_write(memory, mem_read(memory, reg[R_PC] + pc_offset), reg[r0]); // valider adresse afficher message erreur mem et running = 0
                        }
                        else //invalid adress
                        {
                            running = 0;
                            sendOutput(client_socket_fd, "Program abort memory out of bound or access violation\n");
                        }
                    }
                    else //invalid adress
                    {
                        running = 0;
                        sendOutput(client_socket_fd, "Program abort memory out of bound\n");
                    }                       
                }
                break;
            case OP_STR:
                //STR
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t offset = sign_extend(instr & 0x3F, 6);
                    if(validAdress(memory, reg[r1] + offset, ptr, 'W'))
                    {
                        mem_write(memory, reg[r1] + offset, reg[r0]); // valider adresse afficher message erreur mem et running = 0
                    }
                    else //invalid adress
                    {
                        running = 0;
                        sendOutput(client_socket_fd, "Program abort memory out of bound or access violation\n");
                    }                                
                }
                break;
            case OP_TRAP:
                // TRAP
                switch (instr & 0xFF)
                {
                    case TRAP_GETC:
                       // TRAP GETC
                        // read a single ASCII char
                        reg[R_R0] = (uint16_t)getchar();

                        break;
                    case TRAP_OUT:
                        // TRAP OUT
                        putc((char)reg[R_R0], stdout);
                        fflush(stdout);

                        break;
                    case TRAP_PUTS:
                        // TRAP PUTS
                        {
                            // one char per word
                            uint16_t* c = memory + reg[R_R0]; // valider adresse afficher message erreur mem et running = 0
                            if(validAdress(c, 0, ptr, 'R'))
                            {
                                while (*c)
                                {
                                    putc((char)*c, stdout);
                                    ++c; // valider adresse afficher message erreur mem et running = 0
                                    if(!validAdress(c, 0, ptr, 'R'))
                                    {
                                        running = 0;
                                        sendOutput(client_socket_fd, "Program abort memory out of bound\n");
                                        *c = 0;
                                    }     
                                }
                                fflush(stdout);
                            }
                            else //invalid adress
                            {
                                running = 0;
                                sendOutput(client_socket_fd, "Program abort memory out of bound\n");
                            }     
                        }
                        break;
                    case TRAP_IN:
                        // TRAP IN
                        {
                            sendOutput(client_socket_fd, "Enter a character\n");
                            char c = getchar();
                            putc(c, stdout);
                            reg[R_R0] = (uint16_t)c;
                        }
                        break;
                    case TRAP_PUTSP:
                        // TRAP PUTSP
                            {
                            // one char per byte (two bytes per word)
                             //  here we need to swap back to
                             //  big endian format
                            uint16_t* c = memory + reg[R_R0]; // valider adresse afficher message erreur mem et running = 0
                            if(validAdress(c, 0, ptr, 'R'))
                            {
                                while (*c)
                                {
                                    char char1 = (*c) & 0xFF;
                                    putc(char1, stdout);
                                    char char2 = (*c) >> 8;

                                    if (char2) putc(char2, stdout);
                                    ++c; // valider adresse afficher message erreur mem et running = 0
                                    if(!validAdress(c, 0, ptr, 'R'))
                                    {
                                        running = 0;
                                        sendOutput(client_socket_fd, "Program abort memory out of bound\n");
                                        *c = 0;
                                    }     
                                }
                                fflush(stdout);
                            }
                            else //invalid adress
                            {
                                running = 0;
                                sendOutput(client_socket_fd, "Program abort memory out of bound\n");
                            }     
                        }
                        break;
                    case TRAP_HALT:
                        // TRAP HALT
                        sendOutput(client_socket_fd, "HALT\n");
                        running = 0;

                        break;
                }
                break;
            case OP_RES:
            case OP_RTI:
            default:
                // BAD OPCODE
                abort();
                break;
        }
    }
    /* 
     * Étape : Initialisation des variables pour la réutilisation de la VM.
     * - `sem_wait(&ptr->semN);` : Attend sur le sémaphore pour s'assurer que l'accès exclusif à la VM est verrouillé avant modification.
     * - `ptr->VM.busy = 0;` : Libère la VM en indiquant qu'elle n'est plus occupée.
     * - `ptr->VM.DimRamUsed = 0;` : Réinitialise la mémoire utilisée pour que la VM puisse être réutilisée proprement.
     * - `sem_post(&ptr->semN);` : Libère le sémaphore après les modifications.
     * - `restore_input_buffering();` : Restaurer le mode de saisie du terminal après la fin de l'exécution.
     * - `pthread_exit(0);` : Termine le thread proprement avec une exit code 0.
     */
    sem_wait(&ptr->semN);
    ptr->VM.busy = 0;
    ptr->VM.DimRamUsed = 0;
    sem_post(&ptr->semN);
    restore_input_buffering();
    pthread_exit(0);
}

//#######################################
//# Recherche une VM libre dans la liste chaînée
//# ENTREE: 
//# RETOUR: Un pointeur vers la premiere VM libre   
//#         Retourne NULL dans le cas où aucune VM disponible
struct noeudVM * findFreeVM(){
    sem_wait(&semH);
    sem_wait(&semQ);
    /* 
     * Recherche d'une VM libre parmi une liste de VM.
     * - `sem_wait(&semH);` : Attend sur le sémaphore principal pour garantir l'accès exclusif à la liste de VMs.
     * - `sem_wait(&semQ);` : Attend sur le sémaphore pour gérer l'accès à la file d'attente de VMs.
     * 
     * Si la liste des VMs est vide, retourne NULL.
     */
    if ((head == NULL) && (queue == NULL)) {
        sem_post(&semQ);
        sem_post(&semH);
        return NULL;
    }

    //Obtient un accès exclusif à la tête de la liste de VMs.
    sem_wait(&head->semN);
    struct noeudVM *ptr = head;

    //Relâche les sémaphores pour permettre l'accès concurrent à la file d'attente et à la liste.
    sem_post(&semQ);
    sem_post(&semH);

    if (!ptr->VM.busy) // premier noeudVM voir si Not busy
        return ptr; // Retourne la VM libre trouvée.
    do {
        if(ptr->suivant!=NULL){
            sem_wait(&(ptr->suivant->semN)); // Attend sur le sémaphore suivant si la VM est occupée.
        }
        else{
            sem_post(&(ptr->semN)); // Libère le sémaphore de la VM précédente si la liste se termine.
            return NULL;
        }
        //on se garde un copie temporaire avant de changer ptr pour pouvoir post le semN
        struct noeudVM* temp = ptr;
        ptr=ptr->suivant; // Passe au noeud suivant.
        sem_post(&(temp->semN)); // Libère le sémaphore du noeud précédent.
    }while(ptr->VM.busy); // Continue jusqu'à trouver une VM libre.

    return ptr; // Retourne la VM libre trouvée.
}

//#######################################
//#
//# Recherche le PRÉDÉCESSEUR d'un item dans la liste chaînée
//# ENTREE: Numéro de la ligne a supprimer
//# RETOUR: Le pointeur vers le prédécesseur est retourné       
//#         Retourne NULL dans le cas où l'item est introuvable
//#
struct noeudVM * findPrev(const int no){
    //La liste est vide 
    if ((head==NULL)&&(queue==NULL)) return NULL;

    //Pointeur de navigation
     
    //Obtient un accès exclusif à la tête de la liste et au noeud suivant.
    sem_wait(&head->semN);
    struct noeudVM * ptr = head;

    if(ptr->suivant!=NULL) {
        sem_wait(&ptr->suivant->semN); // Attend sur le sémaphore suivant.
    }else {
        sem_post(&ptr->semN); // Libère le sémaphore de la tête si le suivant est NULL.
        return NULL;
    }
    /* 
     * Recherche du noeud ayant le numéro spécifié.
     * Tant que le numéro du noeud suivant n'est pas celui recherché :
     */
    while(ptr->suivant->VM.noVM!=no){
        //on se garde un variable temporaire du noeud pour pouvoir poster le sémaphore plus tard
        struct noeudVM* temp = ptr; // Stocke temporairement le noeud précédent.
        ptr=ptr->suivant; // Passe au noeud suivant.
        sem_post(&temp->semN); // Libère le sémaphore du noeud précédent.
        if(ptr->suivant!=NULL) {
            sem_wait(&ptr->suivant->semN); // Attend sur le sémaphore suivant.
        }else {
            //Dans le cas ou le suivant est null, il n'y a aucune vm avec le numéro demandé
            sem_post(&ptr->semN); // Libère le sémaphore du dernier noeud.
            return NULL; // Aucun noeud avec le numéro demandé.
        }
    }
    //Le précédent du noeud ayant le numéro `no` est trouvé.
    sem_post(&ptr->suivant->semN); // Libère le sémaphore du noeud suivant.
    return ptr; // Retourne le noeud précédent.
} 

/* 
 * Fonction : addItem
 * Description : Ajoute un nouveau noeud VM à la liste et initialise ses valeurs.
 * Retour : Un pointeur vers le nouveau noeud VM ajouté.
 */
struct noeudVM* addItem(){
    //Création de l'enregistrement en mémoire
    struct noeudVM* ni = (struct noeudVM*)malloc(sizeof(struct noeudVM));
    //Affectation des valeurs des champs
    sem_wait(&semNbVM);  // Attend sur le sémaphore pour accéder à nbVM
    ni->VM.noVM = ++nbVM;  // Attribue un numéro unique à la VM (incrémente le compteur nbVM).
    sem_post(&semNbVM);  // Libère le sémaphore.
    ni->VM.busy = 0;  // Initialise la machine virtuelle comme non occupée.
    ni->VM.DimRam = 65536;  // Taille de la mémoire de la VM.
    ni->VM.DimRamUsed = 0;  // Initialise la mémoire utilisée à 0.
    ni->VM.tid = pthread_self();  // Stocke l'ID du thread actuel.
    ni->VM.ptrDebutVM = (unsigned short*)malloc(sizeof(unsigned short) * 65536);  // Alloue mémoire pour la RAM virtuelle.
    sem_init(&ni->semN, 0, 1);  // Initialise le sémaphore pour la synchronisation.

    sem_wait(&semH);  // Attend sur le sémaphore de la tête.
    sem_wait(&semQ);  // Attend sur le sémaphore de la file d'attente.
    if ((head == NULL) && (queue == NULL)) {  // Si la liste est vide.
    ni->suivant = NULL;
    queue = head = ni;  // Le nouveau noeud devient la tête et la queue.
    sem_wait(&ni->semN);  // Attend sur le sémaphore du nouveau noeud.
    sem_post(&semH);  // Libère le sémaphore de la tête.
    sem_post(&semQ);  // Libère le sémaphore de la file d'attente.
    return ni;  // Retourne le nouveau noeud VM.
    }
    struct noeudVM* tptr = queue;  // Stocke la queue actuelle.
    ni->suivant = NULL;  // Le nouveau noeud n’a pas de suivant.
    queue = ni;  // Mise à jour de la queue.
    tptr->suivant = ni;  // Le noeud précédent pointe vers le nouveau noeud.
    sem_wait(&ni->semN);  // Attend sur le sémaphore du nouveau noeud.
    sem_post(&tptr->semN);  // Libère le sémaphore du noeud précédent.
    sem_post(&semQ);  // Libère le sémaphore de la file d'attente.
    return queue;  // Retourne la nouvelle queue.
}

//#######################################
//# Retire un item de la liste chaînée en exclusion mutuelle
//# ENTREE: noVM: numéro du noeud a retirer 
void* removeItem(void* args){
    struct paramE* param = args;
    int const noVM = param -> noVM;
    free(param);
    struct noeudVM * ptr;
    struct noeudVM * tptr;
    struct noeudVM * optr;

    sem_wait(&semH);
    sem_wait(&semQ);
    //Vérification sommaire (noVM>0 et liste non vide)  
    if ((noVM<1)||((head==NULL)&&(queue==NULL)))
    {
        sem_post(&semH);
        sem_post(&semQ);
        return NULL;
    }

    //Pointeur de recherche
    if(noVM==1){
        sem_wait(&head->semN);
        ptr = head; // suppression du premier element de la liste
    }
    else{
        ptr = findPrev(noVM);
    }
    //L'item a été trouvé
    if (ptr!=NULL) {
        if(noVM != 1 && ptr->suivant->VM.busy) // ne pas supprimer une VM busy
        {
            sem_post(&ptr->semN);
            sem_post(&semH);
            sem_post(&semQ);
            return NULL;
        }
        // Memorisation du pointeur de l'item en cours de suppression
        // Ajustement des pointeurs
        if((head == ptr) && (noVM==1)) // suppression de l'element de tete
        {
            if(ptr->VM.busy) // ne pas supprimer une VM busy
            {
                sem_post(&ptr->semN);
                sem_post(&semH);
                sem_post(&semQ);
                return NULL;
            }
            sem_wait(&semNbVM);
            nbVM--;
            sem_post(&semNbVM);
            sem_t temp = ptr->semN;
            if(head==queue) // un seul element dans la liste
            {
                free(ptr->VM.ptrDebutVM);
                free(ptr);
                sem_post(&temp);
                queue = head = NULL;
                sem_post(&semH);
                sem_post(&semQ);
                return NULL;
            }
            sem_wait(&ptr->suivant->semN);
            tptr = ptr->suivant;
            head = tptr;
            free(ptr->VM.ptrDebutVM);
            free(ptr);
            sem_post(&temp);

        }
        else if (queue==ptr->suivant) // suppression de l'element de queue
        {
            sem_wait(&semNbVM);
            nbVM--;
            sem_post(&semNbVM);

            sem_wait(&ptr->suivant->semN);
            sem_t temp = ptr->suivant->semN;
            sem_post(&semH);
            queue=ptr;
            sem_post(&semQ);

            free(ptr->suivant->VM.ptrDebutVM);
            free(ptr->suivant);
            ptr->suivant=NULL;
            sem_post(&temp);
            sem_post(&ptr->semN);
            return NULL;
        }
        else // suppression d'un element dans la liste
        {
            sem_wait(&semNbVM);
            nbVM--;
            sem_post(&semNbVM);

            sem_wait(&ptr->suivant->semN);
            sem_t temp = ptr->suivant->semN;
            optr = ptr->suivant;
            sem_wait(&ptr->suivant->suivant->semN);
            ptr->suivant = ptr->suivant->suivant;
            tptr = ptr->suivant;
            free(optr->VM.ptrDebutVM);
            free(optr);
            sem_post(&temp);
            sem_post(&ptr->semN);
        }

        sem_post(&semH);
        sem_post(&semQ);

        while (tptr!=NULL){ // ajustement des numeros de VM
            //Est-ce le prédécesseur de l'item recherché?
            tptr->VM.noVM--;

            if(tptr->suivant == NULL) break;
            //Déplacement du pointeur de navigation
            struct noeudVM* temp = tptr;
            sem_wait(&tptr->suivant->semN);
            tptr=tptr->suivant;
            sem_post(&temp->semN);
        }
        sem_post(&tptr->semN);
    } else {
        sem_post(&semH);
        sem_post(&semQ);
    }
    return NULL;
}

//#######################################
//#
//# Affiche les items dont le numéro séquentiel est compris dans une plage
//#
void* listItems(void* args){
    //Désérialisation des arguments
    struct paramL* param = args;
    struct noeudVM * ptr = NULL;
    int start = param->nStart;
    int end = param->nEnd;
    int client_socket_fd = param->client_socket_fd;

    free(param);

    struct rusage  usage;

    //Affichage des entêtes de colonnes
    sendOutput(client_socket_fd, "noVM  Busy?   tid     TempsCPU(us)        DimRam  DimRamUsed  Adr. Debut VM\n");
    sendOutput(client_socket_fd, "===========================================================================\n");

    sem_wait(&semH);
    if(head == NULL) {
        sem_post(&semH);
    }else {
        sem_wait(&head->semN);
        ptr = head;
        sem_post(&semH);

        while (ptr!=NULL){
            if (ptr->VM.noVM>end) break; //L'ensemble des vm de l'intervale sont passée
            //L'item a un numéro séquentiel dans l'interval défini
            if (ptr->VM.noVM>=start){
                if(ptr->VM.busy){
                    getrusage(RUSAGE_SELF,&usage);
                    char *msg = malloc(150*sizeof(char));
                    sprintf(msg, "%d \t %d \t %ld   \t %ld \t %d \t %d \t %p\n",
                        ptr->VM.noVM,ptr->VM.busy, ptr->VM.tid, usage.ru_utime.tv_sec,
                        ptr->VM.DimRam, ptr->VM.DimRamUsed, ptr->VM.ptrDebutVM);

                    sendOutput(client_socket_fd, msg);
                    free(msg);
                }
                else // not busy
                {
                    char *msg = malloc(150*sizeof(char));
                    sprintf(msg, "%d \t %d \t %s    \t %d   \t %d \t %d \t %p\n",
                        ptr->VM.noVM,0, "##############", 0, 65536, 0, ptr->VM.ptrDebutVM);

                    sendOutput(client_socket_fd, msg);
                    free(msg);
                }
            }

            if(ptr->suivant==NULL) break;
            //On garde une copie de ptr pour pouvoir libéré son sémaphore
            struct noeudVM* optr = ptr;
            sem_wait(&(ptr->suivant->semN));
            ptr = ptr->suivant;
            sem_post(&(optr->semN));
        }
        sem_post(&ptr->semN);
    }
    //Affichage des pieds de colonnes
    sendOutput(client_socket_fd, "===========================================================================\n");
    pthread_exit(0);
}

//#######################################
//#
//# Envoyer une commande à la console (commande B et P)
void* sendCommand(void* args) {
    char command[100];
    struct paramCommand* param = args;
    int client_socket_fd = param->client_socket_fd;
    strcpy(command, param->command);

    free(param);

    const int sfd = dup(1);

    sem_wait(&semC);
    if(dup2(client_socket_fd, 1) == -1) {
        sem_post(&semC);
        pthread_exit(0);
    }

    system(command);
    if(dup2(sfd, 1) != -1)
        close(sfd);
    sem_post(&semC);

    pthread_exit(0);
}

//#######################################
//#
//# Kill un process en execution threadee sur une VM (commande K)
//#
void* killThread(void* args){
    struct paramK* ptr = args;
    pthread_cancel(ptr->tid);
    free(ptr);
    pthread_exit(0);
}

/* Fonction : Envoyer le socket client et le message à la console en exclusion mutuelle
 * Entrée : socket client et le message à afficher
 * Sortie : rien
*/
void sendOutput(int client_socket_fd, char const * msg) {
    sem_wait(&semC); // Exclusion de la console
    const int sfd = dup(1);

    if(dup2(client_socket_fd, 1) == -1) { // Si dup2 est valable ferme la FIFO client et sort
        pthread_exit(0);
    }

    printf("%s", msg); // sinon affiche le msg
    if(dup2(sfd, 1) != -1) // si sfd est ouvert
        close(sfd);

    sem_post(&semC);// Libère la console
}



#pragma once
#include <semaphore.h>
#include <pthread.h>

#include <stdint.h>
#include <sys/stat.h>
#include <signal.h>
/* unix */
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <errno.h>

struct infoVM{                      
    int noVM;
    unsigned char busy;
    pthread_t tid;
    int DimRam;
    int DimRamUsed;
    uint16_t* ptrDebutVM;
    uint16_t offsetDebutCode; // region memoire ReadOnly
    uint16_t offsetFinCode;
};

struct noeudVM{         
    struct infoVM VM;
    struct noeudVM *suivant;
    sem_t semN;
};

struct paramE {
    int noVM;
    int client_socket_fd;
};

struct paramReadTrans {
    int client_socket_fd;
};

struct paramX
{
    int boolPrint;
    char fileName[100];
    int client_socket_fd;
};

struct paramL{
    int nStart;
    int nEnd;
    int client_socket_fd;
};

struct paramCommand {
    char command[100];
    int client_socket_fd;
};

struct paramK {
    pthread_t tid;
};

struct Info_FIFO_Transaction {
    int client_pid;
    char transaction[200];
};

struct client_FIFO_Info {
    char path[100];
};
    
void cls(void);
void error(const int exitcode, const char * message);

struct noeudVM * findItem(const int no);
struct noeudVM * findFreeVM();
struct noeudVM * findPrev(const int no);

struct noeudVM * addItem();
//void* listOlcFile(void* args); // fichier binaire
void* removeItem(void* noVM);
void* listItems(void* args);
void saveItems(const char* sourcefname);
void* executeFile(void* args);
void* killThread(void* args);
void* sendCommand(void* args);

void* readTrans(void* args);
void* receiveTrans();


/* Pour le cours : SIF1015
 * Travail : TP3
 * Session : AUT 2024
 * Étudiants:   Yoan Tremblay
 *              Nathan Pinard
 *              Derek Stevens
 *              Malyk Ratelle
 *              Yannick Lemire
 *
 *  gestionVMS.c : Gestionnaire de statut et de transaction sur le serveur
 *
 */
#include "gestionListeChaineeVMS.h"
#include "gestionVMS.h"

//Pointeur de tête de liste
extern struct noeud* head;
//Pointeur de queue de liste pour ajout rapide
extern struct noeud* queue;

// nombre de VM actives
extern int admin_pid;
extern int nbVM;
extern sem_t semC;

int adminCheck(int* current_admin_pid, int client_pid); // Fonction Vérification du statut
int strToPositiveInt(const char* input);

/* Fonction : fonction utilisée pour la lecture du socket de transaction
 * Entrée : rien
 * Sortie : un pointeur null
*/

void* receiveTrans() {
    /* 
     * Étape : Déclaration des variables pour la gestion des threads clients
     * - `pthread_t clients_tid[1000];` : Tableau pour stocker les identifiants des threads clients.
     * - `int thread_count = 0;` : Compteur pour suivre le nombre de threads créés.
     */
    pthread_t clients_tid[1000];
    int thread_count = 0;

    /* 
     * Étape : Déclaration des structures d'adresses pour le serveur et les clients
     * - `struct sockaddr_in server_address;` : Structure pour stocker l'adresse du serveur.
     * - `struct sockaddr_in client_address;` : Structure pour stocker l'adresse des clients.
     */
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    /* 
     * Étape : Création du socket serveur
     * - `int socket_fd = socket(AF_INET, SOCK_STREAM, 0);` : Crée un socket de type TCP/IP (AF_INET, SOCK_STREAM).
     */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    /* 
     * Étape : Configuration de l'adresse du serveur
     * - `server_address.sin_family = AF_INET;` : Spécifie la famille de protocole (IPv4).
     * - `server_address.sin_addr.s_addr = inet_addr("127.0.0.1");` : Définit l'adresse IP du serveur (localhost).
     * - `server_address.sin_port = 1236;` : Définit le port utilisé par le serveur.
     */
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = 1236;

    /* 
     * Étape : Liaison du socket à l'adresse du serveur
     * - `int res = bind(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address));` : Lie le socket à l'adresse et au port spécifiés.
     * - Si la liaison échoue (`res == -1`), affiche un message d'erreur et termine le programme.
     */
    int res = bind(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address));
    if(res == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    /* 
     * Étape : Mise en écoute du socket serveur
     * - `listen(socket_fd, 5);` : Met le socket en mode écoute, avec une file d'attente de 5 connexions.
     */
    listen(socket_fd, 5);

    /* 
     * Étape : Boucle principale pour accepter les connexions clients
     * - `while(1) { ... }` : Boucle infinie pour accepter et traiter les connexions clients.
     */
    while(1) {
        /* 
         * Étape : Acceptation d'une connexion client
         * - `int client_socket_fd = accept(socket_fd, (struct sockaddr*)&client_address,
         *  (socklen_t*)&client_address.sin_port);` : Accepte une connexion entrante et crée un socket dédié pour le client.
         */
        int client_socket_fd = accept(socket_fd, (struct sockaddr*)&client_address, (socklen_t*)&client_address.sin_port);

        /* 
         * Étape : Allocation de mémoire pour les paramètres de la transaction
         * - `struct paramReadTrans* ptr = malloc(sizeof(struct paramReadTrans));` : Alloue dynamiquement de la mémoire pour une structure `paramReadTrans`.
         * - `ptr->client_socket_fd = client_socket_fd;` : Stocke le descripteur de fichier du socket client dans la structure.
         */
        struct paramReadTrans* ptr = malloc(sizeof(struct paramReadTrans));
        ptr->client_socket_fd = client_socket_fd;

        /* 
         * Étape : Création d'un thread pour traiter la transaction du client
         * - `pthread_create(&clients_tid[thread_count++], NULL, &readTrans, ptr);` : Crée un nouveau thread pour traiter la transaction
         *  du client en appelant la fonction `readTrans` avec les paramètres stockés dans `ptr`.
         */
        pthread_create(&clients_tid[thread_count++], NULL, &readTrans, ptr);
    }
}


/* Fonction : fonction utilisée pour le traitement  des transactions
 * Entrée : rien
 * Sortie : un pointeur null
*/
void* readTrans(void* args) {
    /* 
     * Étape : Récupération et initialisation des paramètres de la structure `paramReadTrans`
     * 
     * 1. Conversion de l'argument `args` :
     *    - L'argument `args` est un pointeur générique (`void*`) passé au thread.
     *    - Il est converti en un pointeur vers une structure `paramReadTrans`, car c'est ainsi
     *      que les données nécessaires sont transmises à cette fonction.
     *    - Cette structure contient un élément essentiel pour le fonctionnement du thread :
     *      - Un descripteur de socket (`int client_socket_fd`) pour la communication avec le client.
     */
    struct paramReadTrans* ptr = args;
    int client_socket_fd = ptr->client_socket_fd;
    free(ptr);

    /* 
     * Étape : Déclaration des variables pour la gestion des threads de transaction
     * - `pthread_t tid[1000];` : Tableau pour stocker les identifiants des threads de transaction.
     * - `int thread_count = 0;` : Compteur pour suivre le nombre de threads créés.
     */
    pthread_t tid[1000];
    int thread_count = 0;
    char *sp;
    ssize_t read_res;

    /* 
     * Étape : Boucle pour le traitement de la transaction
     * - `do { ... } while(read_res > 0);` : Boucle pour lire et traiter les transactions tant qu'il y a des données à lire.
     */
    do {
        /* 
         * Étape : Allocation de mémoire pour les données de la transaction
         * - `struct Info_FIFO_Transaction* socketData = malloc(sizeof(struct Info_FIFO_Transaction));` : Alloue dynamiquement de la mémoire pour une structure `Info_FIFO_Transaction`.
         */
        struct Info_FIFO_Transaction* socketData = malloc(sizeof(struct Info_FIFO_Transaction));

        /* 
         * Étape : Lecture des données de la transaction depuis le socket
         * - `read_res = read(client_socket_fd, socketData, sizeof(*socketData));` : Lit les données de la transaction depuis le socket client.
         * - Si une erreur de lecture se produit (`read_res == -1`), affiche un message d'erreur et quitte la boucle.
         * - Si la lecture retourne 0 (fin de la connexion), quitte la boucle.
         */
        read_res = read(client_socket_fd, socketData, sizeof(*socketData));
        if(read_res == -1) {
            perror("Error");
            break;
        }
        if(read_res == 0) break;

        /* 
         * Étape : Traitement de la transaction
         * - `char *tok = strtok_r(socketData->transaction, " ", &sp);` : Tokenize la transaction pour extraire la commande.
         * - Si la transaction est vide (`tok == NULL`), libère la mémoire allouée et continue à la prochaine itération.
         */
        char *tok = strtok_r(socketData->transaction, " ", &sp);
        if(tok == NULL) {
            free(socketData);
            continue;
        }
        printf("%s\n", tok);


        switch(tok[0]){
            case 'E':
            case 'e': {
                /* 
                 * Étape : Vérification des privilèges administrateur
                 * - `if(!adminCheck(&admin_pid, socketData->client_pid)) break;`
                 * - Vérifie si le client a les privilèges administrateur en appelant `adminCheck`.
                 * - Si le client n'est pas administrateur, quitte le cas.
                 */
                if(!adminCheck(&admin_pid, socketData->client_pid)) break;

                /* 
                 * Étape : Extraction du paramètre
                 * - `int const noVM = strToPositiveInt(strtok_r(NULL, " ", &sp));`
                 * - Extrait le numéro de la VM à partir de la transaction en utilisant `strtok_r`.
                 * - Convertit le paramètre en entier positif avec `strToPositiveInt`.
                 * - Si la conversion échoue (`noVM == -1`), quitte le cas.
                 */
                int const noVM = strToPositiveInt(strtok_r(NULL, " ", &sp));
                if(noVM == -1) break;

                /* 
                 * Étape : Allocation de mémoire pour les paramètres de la fonction `removeItem`
                 * - `struct paramE* ptr = malloc(sizeof(struct paramE));`
                 * - Alloue dynamiquement de la mémoire pour une structure `paramE`.
                 * - Initialise les champs de la structure avec les valeurs appropriées :
                 *   - `ptr->client_socket_fd = client_socket_fd;` : Stocke le descripteur de fichier du socket client.
                 *   - `ptr->noVM = noVM;` : Stocke le numéro de la VM à supprimer.
                 */
                struct paramE* ptr = malloc(sizeof(struct paramE));
                ptr->client_socket_fd = client_socket_fd;
                ptr->noVM = noVM;

                /* 
                 * Étape : Appel de la fonction associée
                 * - `pthread_create(&tid[thread_count++], NULL, &removeItem, ptr);`
                 * - Crée un nouveau thread pour exécuter la fonction `removeItem` avec les paramètres stockés dans `ptr`.
                 * - Incrémente le compteur de threads `thread_count`.
                 */
                pthread_create(&tid[thread_count++], NULL, &removeItem, ptr);
                break;
            }
            case 'L':
            case 'l': {
                /* 
                 * Étape : Vérification des privilèges administrateur
                 * - `if(!adminCheck(&admin_pid, socketData->client_pid)) break;`
                 * - Vérifie si le client a les privilèges administrateur en appelant `adminCheck`.
                 * - Si le client n'est pas administrateur, quitte le cas.
                 */
                if(!adminCheck(&admin_pid, socketData->client_pid)) break;

                /* 
                 * Étape : Extraction des paramètres / vérification des paramètres valides
                 * - `int const start = strToPositiveInt(strtok_r(NULL, "-", &sp));`
                 * - Extrait le paramètre de début de la plage à partir de la transaction en utilisant `strtok_r`.
                 * - Convertit le paramètre en entier positif avec `strToPositiveInt`.
                 * - Si la conversion échoue (`start == -1`), quitte le cas.
                 */
                int const start = strToPositiveInt(strtok_r(NULL, "-", &sp));
                if(start == -1) break;

                /* 
                 * Étape : Extraction du paramètre de fin de la plage
                 * - `int const end = strToPositiveInt(strtok_r(NULL, " ", &sp));`
                 * - Extrait le paramètre de fin de la plage à partir de la transaction en utilisant `strtok_r`.
                 * - Convertit le paramètre en entier positif avec `strToPositiveInt`.
                 * - Si la conversion échoue (`end == -1`), quitte le cas.
                 */
                int const end = strToPositiveInt(strtok_r(NULL, " ", &sp));
                if(end == -1) break;

                /* 
                 * Étape : Allocation de mémoire pour les paramètres de la fonction `listItems`
                 * - `struct paramL* ptr = malloc(sizeof(struct paramL));`
                 * - Alloue dynamiquement de la mémoire pour une structure `paramL`.
                 * - Initialise les champs de la structure avec les valeurs appropriées :
                 *   - `ptr->nStart = start;` : Stocke le paramètre de début de la plage.
                 *   - `ptr->nEnd = end;` : Stocke le paramètre de fin de la plage.
                 *   - `ptr->client_socket_fd = client_socket_fd;` : Stocke le descripteur de fichier du socket client.
                 */
                struct paramL* ptr = malloc(sizeof(struct paramL));
                ptr->nStart = start;
                ptr->nEnd = end;
                ptr->client_socket_fd = client_socket_fd;

                /* 
                 * Étape : Appel de la fonction associée
                 * - `pthread_create(&tid[thread_count++], NULL, &listItems, ptr);`
                 * - Crée un nouveau thread pour exécuter la fonction `listItems` avec les paramètres stockés dans `ptr`.
                 * - Incrémente le compteur de threads `thread_count`.
                 */
                pthread_create(&tid[thread_count++], NULL, &listItems, ptr);
                break;
            }
            case 'B': // Affiche liste des fichiers .olc
            case 'b': {
                /* 
                 * Étape : Allocation de mémoire pour les paramètres de la fonction `sendCommand`
                 * - `struct paramCommand* ptr = malloc(sizeof(struct paramCommand));`
                 * - Alloue dynamiquement de la mémoire pour une structure `paramCommand`.
                 * - Initialise les champs de la structure avec les valeurs appropriées :
                 *   - `ptr->client_socket_fd = client_socket_fd;` : Stocke le descripteur de fichier du socket client.
                 *   - `strcpy(ptr->command, "ls -l *.olc3\n");` : Copie la commande pour lister les fichiers `.olc3` dans le champ `command`.
                 */
                struct paramCommand* ptr = malloc(sizeof(struct paramCommand));
                ptr->client_socket_fd = client_socket_fd;
                strcpy(ptr->command, "ls -l *.olc3\n");

                /* 
                 * Étape : Appel de la fonction associée
                 * - `pthread_create(&tid[thread_count++], NULL, &sendCommand, ptr);`
                 * - Crée un nouveau thread pour exécuter la fonction `sendCommand` avec les paramètres stockés dans `ptr`.
                 * - Incrémente le compteur de threads `thread_count`.
                 */
                pthread_create(&tid[thread_count++], NULL, &sendCommand, ptr);
                break;
            }
            case 'K':
            case 'k': {
                /* 
                 * Étape : Vérification des privilèges administrateur
                 * - `if(!adminCheck(&admin_pid, socketData->client_pid)) break;`
                 * - Vérifie si le client a les privilèges administrateur en appelant `adminCheck`.
                 * - Si le client n'est pas administrateur, quitte le cas.
                 */
                if(!adminCheck(&admin_pid, socketData->client_pid)) break;

                /* 
                 * Étape : Extraction du paramètre
                 * - `char* endptr = NULL;`
                 * - `const char* thread_entered_ch = strtok_r(NULL, " ", &sp);`
                 * - Extrait le paramètre du thread à partir de la transaction en utilisant `strtok_r`.
                 * - Si le paramètre est NULL (`thread_entered_ch == NULL`), quitte le cas.
                 * - Convertit le paramètre en identifiant de thread (`pthread_t`) avec `strtoul`.
                 * - Si la conversion échoue (`thread_entered_ch == endptr`), quitte le cas.
                 */
                char* endptr = NULL;
                const char* thread_entered_ch = strtok_r(NULL, " ", &sp);
                if(thread_entered_ch == NULL) break;
                pthread_t const threadEntered = strtoul(thread_entered_ch, &endptr, 10);
                if(thread_entered_ch == endptr) break;

                /* 
                 * Étape : Allocation de mémoire pour les paramètres de la fonction `killThread`
                 * - `struct paramK* ptr = malloc(sizeof(struct paramK));`
                 * - Alloue dynamiquement de la mémoire pour une structure `paramK`.
                 * - Initialise le champ de la structure avec l'identifiant du thread à tuer :
                 *   - `ptr->tid = threadEntered;`
                 */
                struct paramK* ptr = malloc(sizeof(struct paramK));
                ptr->tid = threadEntered;

                /* 
                 * Étape : Appel de la fonction associée
                 * - `pthread_create(&tid[thread_count++], NULL, &killThread, ptr);`
                 * - Crée un nouveau thread pour exécuter la fonction `killThread` avec les paramètres stockés dans `ptr`.
                 * - Incrémente le compteur de threads `thread_count`.
                 */
                pthread_create(&tid[thread_count++], NULL, &killThread, ptr);
                break;
            }
            case 'P':
            case 'p': {
                /* 
                 * Étape : Vérification des privilèges administrateur
                 * - `if(!adminCheck(&admin_pid, socketData->client_pid)) break;`
                 * - Vérifie si le client a les privilèges administrateur en appelant `adminCheck`.
                 * - Si le client n'est pas administrateur, quitte le cas.
                 */
                if(!adminCheck(&admin_pid, socketData->client_pid)) break;

                /* 
                 * Étape : Allocation de mémoire pour les paramètres de la fonction `sendCommand`
                 * - `struct paramCommand* ptr = malloc(sizeof(struct paramCommand));`
                 * - Alloue dynamiquement de la mémoire pour une structure `paramCommand`.
                 * - Initialise les champs de la structure avec les valeurs appropriées :
                 *   - `ptr->client_socket_fd = client_socket_fd;` : Stocke le descripteur de fichier du socket client.
                 *   - `strcpy(ptr->command, "ps -aux\n");` : Copie la commande pour afficher les processus actifs dans le champ `command`.
                 */
                struct paramCommand* ptr = malloc(sizeof(struct paramCommand));
                ptr->client_socket_fd = client_socket_fd;
                strcpy(ptr->command, "ps -aux\n");

                /* 
                 * Étape : Appel de la fonction associée
                 * - `pthread_create(&tid[thread_count++], NULL, &sendCommand, ptr);`
                 * - Crée un nouveau thread pour exécuter la fonction `sendCommand` avec les paramètres stockés dans `ptr`.
                 * - Incrémente le compteur de threads `thread_count`.
                 */
                pthread_create(&tid[thread_count++], NULL, &sendCommand, ptr);
                break;
            }
            case 'X':
            case 'x': {
                /* 
                 * Étape : Appel de la fonction associée
                 * - `int const p = strToPositiveInt(strtok_r(NULL, " ", &sp));`
                 * - Extrait le paramètre `p` à partir de la transaction en utilisant `strtok_r`.
                 * - Convertit le paramètre en entier positif avec `strToPositiveInt`.
                 * - Si la conversion échoue (`p == -1`), quitte le cas.
                 */
                int const p = strToPositiveInt(strtok_r(NULL, " ", &sp));
                char *nomfich = strtok_r(NULL, "\n", &sp);

                /* 
                 * Étape : Vérification des paramètres
                 * - Si `p` est invalide (`p == -1`) ou si `nomfich` est NULL, quitte le cas.
                 */
                if(p == -1 || nomfich == NULL) break;

                /* 
                 * Étape : Allocation de mémoire pour les paramètres de la fonction `executeFile`
                 * - `struct paramX* ptr = malloc(sizeof(struct paramX));`
                 * - Alloue dynamiquement de la mémoire pour une structure `paramX`.
                 * - Initialise les champs de la structure avec les valeurs appropriées :
                 *   - `ptr->boolPrint = p;` : Stocke le paramètre `p`.
                 *   - `strcpy(ptr->fileName, nomfich);` : Copie le nom du fichier dans le champ `fileName`.
                 *   - `ptr->client_socket_fd = client_socket_fd;` : Stocke le descripteur de fichier du socket client.
                 */
                struct paramX* ptr = malloc(sizeof(struct paramX));
                ptr->boolPrint = p;
                strcpy(ptr->fileName, nomfich);
                ptr->client_socket_fd = client_socket_fd;

                /* 
                 * Étape : Appel de la fonction associée
                 * - `pthread_create(&tid[thread_count++], NULL, &executeFile, ptr);`
                 * - Crée un nouveau thread pour exécuter la fonction `executeFile` avec les paramètres stockés dans `ptr`.
                 * - Incrémente le compteur de threads `thread_count`.
                 */
                pthread_create(&tid[thread_count++], NULL, &executeFile, ptr);
                break;
            }
            case 'Q':
            case 'q': {
                /* 
                 * Étape : Vérification et réinitialisation du PID administrateur
                 * - `if(admin_pid == socketData->client_pid) { admin_pid = 0; }`
                 * - Vérifie si le PID du client est le même que celui de l'administrateur actuel (`admin_pid`).
                 * - Si c'est le cas, réinitialise `admin_pid` à 0, retirant ainsi les privilèges administrateur du client.
                 */
                if(admin_pid == socketData->client_pid) {
                    admin_pid = 0;
                }
                break;
            }
        }
        free(socketData);
    } while(read_res > 0);

    /* 
     * Étape : Attente de la fin des threads de transaction
     * - `for(int i = 0; i < thread_count; i++) { pthread_join(tid[i], NULL); }` : Attend la fin de chaque thread de transaction pour s'assurer que toutes les opérations sont complètes.
     */
    for(int i = 0; i < thread_count; i++) {
        pthread_join(tid[i], NULL);
    }

    /* 
     * Étape : Fermeture du socket client
     * - `close(client_socket_fd);` : Ferme le socket de communication avec le client.
     */
    close(client_socket_fd);

    /* 
     * Étape : Retour
     * - `return NULL;` : Retourne NULL pour indiquer la fin de la fonction.
     */
    return NULL;
}
/* Fonction : Fonction pour convertir une string en entier positif
 * Entrée : Un pointeur vers la string
 * Sortie : un int positif
*/
int strToPositiveInt(const char* input) {
    char* endptr = NULL;
    if(input == NULL) return -1;
    long const val = strtol(input, &endptr, 10); //conversion string à long int

    if(input == endptr || val > INT32_MAX || val < 0) return -1; // Si valeur null, ou + grand qu'un int de 32bit ou < 0 retourne -1

    return (int)val; // sinon retourne la valeur convertie
}
/* Fonction : Fonction pour donner accès au mode admin
 * Entrée : le PID courant de l'admin et le PID du client
 * Sortie : 1 si le client est devenu admin et le pid sinon
*/
int adminCheck(int* current_admin_pid, const int client_pid) {
    if(*current_admin_pid == 0) { // si le mode admin n'est pas attribué
        *current_admin_pid = client_pid;
        return 1;
    }
    return *current_admin_pid == client_pid;
}

/*
Modifications et fonctionnalités à ajouter :
Ajout d'une commande i (ou I) pour lister les fichiers .c du répertoire courant côté serveur :

Description : Le serveur doit répondre à la commande i d'un client en listant tous les fichiers .c du répertoire où il est exécuté.
Implémentation :
Ajouter une fonction listFiles() côté serveur pour parcourir le répertoire courant (exemple : utilisation de opendir et readdir).
Envoyer les résultats au client via le socket correspondant.
Ajout d'une commande s <nom_fichier> pour obtenir les informations sur un fichier spécifique :

Description : Lorsqu'un client envoie la commande s <nom_fichier>, le serveur doit renvoyer les informations du fichier demandé en utilisant la fonction stat().
Implémentation :
Lire le nom du fichier depuis la requête du client.
Vérifier son existence avec stat() et récupérer les métadonnées (taille, permissions, date de modification, etc.).
Envoyer les informations au client.
Gestion des threads côté serveur pour chaque client connecté :

Description : Chaque client doit être géré dans un thread dédié, garantissant ainsi la gestion concurrente des requêtes.
Implémentation :
Créer un thread pour chaque client après acceptation de la connexion avec accept().
Associer le thread à une fonction qui gère les requêtes du client (ex. handleClient()).
Ajout d'une boucle pour traiter plusieurs requêtes par client :

Description : Un client peut envoyer plusieurs requêtes sans fermer la connexion.
Implémentation :
Ajouter une boucle dans handleClient() pour traiter les commandes successives (i, s, etc.) jusqu’à ce que le client envoie une commande de déconnexion.
Ajout de la déconnexion propre :

Description : Permettre aux clients de se déconnecter proprement en envoyant une commande spécifique (ex. quit).
Implémentation :
Vérifier la commande quit dans la fonction qui gère les requêtes.
Fermer le socket et terminer le thread associé.
*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PORT 1234
#define BUFFER_SIZE 256

void* handleClient(void* client_socket);

void listFiles(int client_socket);
void sendFileInfo(int client_socket, const char* filename);

int main() {
    /*
 * Étape : Déclaration des variables nécessaires au serveur
 *
 * 1. `int server_fd, client_fd` :
 *    - `server_fd` : Descripteur de fichier pour le socket serveur. 
 *      Ce socket sera utilisé pour écouter les connexions entrantes des clients.
 *    - `client_fd` : Descripteur de fichier pour le socket client.
 *      Ce socket sera utilisé pour la communication avec un client spécifique après connexion.
 *
 * 2. `struct sockaddr_in server_addr, client_addr` :
 *    - `server_addr` : Structure contenant les informations d’adresse du serveur (IP et port).
 *    - `client_addr` : Structure pour stocker les informations d’adresse d’un client connecté.
 *
 * 3. `socklen_t addr_len` :
 *    - Variable qui stocke la taille de la structure `client_addr`.
 *    - Utilisée par les appels système comme `accept()` pour recevoir les informations d’un client.
 */
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

/*
 * Étape : Création du socket serveur
 *
 * 1. `socket(AF_INET, SOCK_STREAM, 0)` :
 *    - Crée un socket IPv4 (`AF_INET`) de type TCP (`SOCK_STREAM`) pour la communication réseau.
 *    - Le troisième argument (`0`) indique que le protocole par défaut (TCP dans ce cas) sera utilisé.
 *
 * 2. Vérification du succès de la création du socket :
 *    - Si `socket()` retourne `-1`, cela signifie que la création a échoué.
 *    - Affiche un message d'erreur avec `perror()` pour diagnostiquer la cause de l'échec.
 *    - Termine le programme avec `exit(EXIT_FAILURE)`.
 */
    // Créer le socket serveur
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
/*
 * Étape : Configuration de l'adresse du serveur
 *
 * 1. `server_addr.sin_family = AF_INET` :
 *    - Spécifie que la famille d'adresses utilisée est IPv4 (AF_INET).
 *
 * 2. `server_addr.sin_addr.s_addr = INADDR_ANY` :
 *    - Définit l'adresse IP du serveur sur toutes les interfaces réseau disponibles.
 *    - Cela permet au serveur d'accepter des connexions sur n'importe quelle interface.
 *
 * 3. `server_addr.sin_port = htons(PORT)` :
 *    - Définit le port utilisé par le serveur.
 *    - La fonction `htons()` convertit le numéro de port de l'ordre d'octets hôte (little-endian) à l'ordre réseau (big-endian),
 *      car les réseaux utilisent toujours le big-endian.
 */
    // Configurer l'adresse du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
/*
 * Étape : Association du socket à l'adresse (Bind)
 *
 * 1. `bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))` :
 *    - Associe le descripteur de socket `server_fd` à l'adresse spécifiée dans `server_addr` (IP et port).
 *    - Permet au socket d'écouter sur l'adresse et le port définis.
 *
 * 2. Vérification de l'échec du bind :
 *    - Si `bind()` retourne `-1`, cela signifie que l'association a échoué (ex : port déjà utilisé).
 *    - Affiche un message d'erreur avec `perror()` pour diagnostiquer la cause de l'échec.
 *    - Ferme le socket avec `close(server_fd)` pour libérer les ressources.
 *    - Termine le programme avec `exit(EXIT_FAILURE)`.
 */
    // Associer le socket à l'adresse
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
/*
 * Étape : Mise en mode écoute du serveur
 *
 * 1. `listen(server_fd, 10)` :
 *    - Place le socket en mode écoute pour accepter les connexions entrantes.
 *    - Le deuxième argument (`10`) définit la taille de la file d'attente pour les connexions en attente.
 *
 * 2. Vérification de l'échec de la mise en écoute :
 *    - Si `listen()` retourne `-1`, cela signifie que l'opération a échoué.
 *    - Affiche un message d'erreur avec `perror()` pour diagnostiquer la cause de l'échec.
 *    - Ferme le socket avec `close(server_fd)` pour libérer les ressources.
 *    - Termine le programme avec `exit(EXIT_FAILURE)`.
 */
    // Mettre le serveur en mode écoute
    if (listen(server_fd, 10) == -1) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);
/*
 * Étape : Boucle principale pour accepter les connexions des clients
 *
 * 1. `while (1)` :
 *    - Boucle infinie permettant au serveur d'accepter continuellement les connexions des clients.
 *    - Chaque connexion entrante est gérée dans cette boucle.
 */
    while (1) {
/*
 * Étape : Acceptation des connexions
 *
 * 1. `accept(server_fd, (struct sockaddr*)&client_addr, &addr_len)` :
 *    - Accepte une connexion entrante depuis le socket serveur `server_fd`.
 *    - Remplit la structure `client_addr` avec les informations du client (IP, port).
 *    - La taille de cette structure est passée via `addr_len`.
 *    - Retourne un nouveau descripteur de fichier (`client_fd`) pour le socket client.
 *
 * 2. Gestion des erreurs d'acceptation :
 *    - Si `accept()` retourne `-1`, cela signifie que l'acceptation a échoué.
 *    - Affiche un message d'erreur avec `perror()` pour diagnostiquer la cause de l'échec.
 *    - Continue la boucle pour traiter la prochaine connexion sans terminer le serveur.
 */
        // Accepter les connexions des clients
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd == -1) {
            perror("Accept failed");
            continue;
        }
/*
 * Étape : Informations sur le client connecté
 *
 * 1. `inet_ntoa(client_addr.sin_addr)` :
 *    - Convertit l'adresse IP du client en une chaîne lisible au format IPv4 (ex. "192.168.1.1").
 *    - Affiche l'adresse IP du client connecté dans le terminal du serveur.
 */
        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
/*
 * Étape : Création d'un thread pour gérer le client
 *
 * 1. `pthread_t client_thread` :
 *    - Déclare une variable pour le thread qui sera dédié au client.
 *
 * 2. Allocation mémoire pour le descripteur de socket du client :
 *    - `malloc(sizeof(int))` alloue dynamiquement de la mémoire pour stocker le descripteur du socket client.
 *    - Cette allocation est nécessaire car le thread doit accéder à ce descripteur de manière indépendante.
 *
 * 3. `pthread_create(&client_thread, NULL, handleClient, client_socket)` :
 *    - Crée un thread pour gérer la connexion avec le client.
 *    - La fonction `handleClient` est exécutée dans le thread, et le descripteur de socket client est passé en argument.
 *
 * 4. `pthread_detach(client_thread)` :
 *    - Détache le thread pour permettre sa gestion autonome.
 *    - Le thread s'exécute indépendamment et ses ressources sont libérées automatiquement lorsqu'il se termine.
 */
        // Créer un thread pour gérer le client
        pthread_t client_thread;
        int* client_socket = malloc(sizeof(int));
        *client_socket = client_fd;
        pthread_create(&client_thread, NULL, handleClient, client_socket);
        pthread_detach(client_thread);
    }
/*
 * Étape : Fermeture du socket serveur
 *
 * 1. `close(server_fd)` :
 *    - Ferme le socket serveur lorsque la boucle principale est terminée (non atteinte dans ce cas en raison de la boucle infinie).
 */
    close(server_fd);
    return 0;
}




/*
 * Fonction : handleClient
 * Description : Gère les interactions avec un client connecté, en traitant les commandes reçues
 *               et en envoyant les réponses appropriées.
 * Entrée : 
 *   - `client_socket` : Pointeur vers un entier contenant le descripteur de fichier du socket client.
 * Sortie :
 *   - Retourne NULL après la déconnexion du client.
 */

void* handleClient(void* client_socket) {

    /*
 * Étape : Initialisation du socket client
 *
 * 1. `int sock = *(int*)client_socket` :
 *    - Dé-référence le pointeur pour obtenir le descripteur de fichier du socket client.
 *
 * 2. `free(client_socket)` :
 *    - Libère la mémoire allouée dynamiquement pour le descripteur de fichier.
 *    - Cette mémoire a été allouée dans la boucle principale du serveur.
 */

    int sock = *(int*)client_socket;
    free(client_socket);

    char buffer[BUFFER_SIZE];

    /*
 * Étape : Boucle principale de traitement des commandes
 *
 * 1. `while (1)` :
 *    - Maintient une boucle infinie pour traiter les commandes envoyées par le client.
 *    - La boucle se termine si le client se déconnecte ou envoie une commande "quit".
 */

    while (1) {

/*
 * Étape : Lecture de la commande du client
 *
 * 1. `memset(buffer, 0, BUFFER_SIZE)` :
 *    - Initialise le tampon `buffer` pour s'assurer qu'il est vide avant chaque lecture.
 *
 * 2. `read(sock, buffer, BUFFER_SIZE - 1)` :
 *    - Lit une commande envoyée par le client via le socket.
 *    - Stocke jusqu'à `BUFFER_SIZE - 1` caractères dans le tampon `buffer` pour laisser de la place pour le caractère de fin de chaîne.
 *
 * 3. Gestion des erreurs ou de la déconnexion :
 *    - Si `read()` retourne une valeur ≤ 0, cela signifie que le client s'est déconnecté ou qu'une erreur est survenue.
 *    - Ferme le socket client avec `close(sock)` et retourne NULL pour terminer le thread.
 */

        memset(buffer, 0, BUFFER_SIZE);

        // Lire la commande du client
        int bytes_read = read(sock, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            printf("Client disconnected.\n");
            close(sock);
            return NULL;
        }

        buffer[bytes_read] = '\0';
        printf("Command received: %s\n", buffer);

/*
 * Étape : Traitement des commandes
 *
 * 1. `strncmp(buffer, "i", 1) || strncmp(buffer, "I", 1)` :
 *    - Si la commande est `i` ou `I`, appelle la fonction `listFiles(sock)` pour envoyer la liste des fichiers `.c` au client.
 *
 * 2. `strncmp(buffer, "s ", 2)` :
 *    - Si la commande commence par `s `, extrait le nom du fichier (à partir de `buffer + 2`) et appelle `sendFileInfo(sock, buffer + 2)` pour envoyer les informations sur le fichier demandé.
 *
 * 3. `strncmp(buffer, "quit", 4)` :
 *    - Si la commande est `quit`, affiche un message indiquant que le client a demandé une déconnexion.
 *    - Ferme le socket client avec `close(sock)` et retourne NULL pour terminer le thread.
 *
 * 4. Commande inconnue :
 *    - Si aucune commande valide n'est détectée, envoie au client un message "Unknown command\n".
 */


        // Traiter les commandes
        if (strncmp(buffer, "i", 1) == 0 || strncmp(buffer, "I", 1) == 0) {
            listFiles(sock);
        } else if (strncmp(buffer, "s ", 2) == 0) {
            sendFileInfo(sock, buffer + 2);
        } else if (strncmp(buffer, "quit", 4) == 0) {
            printf("Client requested disconnection.\n");
            close(sock);
            return NULL;
        } else {
            write(sock, "Unknown command\n", 16);
        }
    }

/*
 * Étape : Fin de la fonction
 *
 * 1. La boucle continue jusqu'à ce que le client se déconnecte ou envoie "quit".
 * 2. Ferme le socket client et retourne NULL pour terminer proprement le thread.
 */

}

/*
 * Fonction : listFiles
 * Description : Envoie au client la liste des fichiers `.c` présents dans le répertoire courant du serveur.
 * Entrée : 
 *   - `client_socket` : Descripteur de fichier du socket client pour envoyer les données.
 */

void listFiles(int client_socket) {

/*
 * Étape : Ouverture du répertoire courant
 *
 * 1. `opendir(".")` :
 *    - Ouvre le répertoire courant du serveur pour parcourir son contenu.
 *    - Retourne un pointeur `DIR*` représentant le flux de répertoire.
 *
 * 2. Vérification de l'échec :
 *    - Si `opendir()` retourne NULL, cela signifie que l'ouverture du répertoire a échoué.
 *    - Affiche un message d'erreur avec `perror()` pour diagnostiquer la cause.
 *    - Retourne immédiatement sans envoyer de données au client.
 */

    DIR* dir = opendir(".");
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

/*
 * Étape : Lecture des fichiers du répertoire
 *
 * 1. `readdir(dir)` :
 *    - Lit chaque entrée du répertoire et retourne un pointeur vers une structure `struct dirent`.
 *    - La structure `dirent` contient des informations sur l'entrée, y compris son nom (`d_name`).
 *
 * 2. Parcours des entrées :
 *    - La boucle continue jusqu'à ce que `readdir()` retourne NULL, indiquant la fin du répertoire.
 */

    struct dirent* entry;
    char buffer[BUFFER_SIZE];
    while ((entry = readdir(dir)) != NULL) {

    /*
     * Étape : Filtrage des fichiers `.c`
     *
     * 1. `strstr(entry->d_name, ".c")` :
     *    - Recherche la sous-chaîne ".c" dans le nom de l'entrée.
     *    - Si trouvé, cela signifie que l'entrée est un fichier ayant l'extension `.c`.
     *
     * 2. Envoi du nom de fichier au client :
     *    - Formatte le nom du fichier avec un saut de ligne à l'aide de `snprintf`.
     *    - Écrit le contenu formaté dans le socket du client avec `write(client_socket, ...)`.
     */

        if (strstr(entry->d_name, ".c")) {
            snprintf(buffer, BUFFER_SIZE, "%s\n", entry->d_name);
            write(client_socket, buffer, strlen(buffer));
        }
    }

/*
 * Étape : Fermeture du répertoire
 *
 * 1. `closedir(dir)` :
 *    - Ferme le flux de répertoire ouvert avec `opendir`.
 *    - Libère les ressources associées au flux.
 */

    closedir(dir);
}

/*
 * Fonction : sendFileInfo
 * Description : Récupère et envoie au client les informations d'un fichier donné.
 * Entrée : 
 *   - `client_socket` : Descripteur de fichier du socket client pour envoyer les données.
 *   - `filename` : Nom du fichier pour lequel les informations sont demandées.
 */

void sendFileInfo(int client_socket, const char* filename) {

/*
 * Étape : Récupération des informations sur le fichier
 *
 * 1. `stat(filename, &file_stat)` :
 *    - Appelle la fonction `stat()` pour récupérer les métadonnées du fichier `filename`.
 *    - Remplit la structure `file_stat` avec les informations suivantes :
 *        - Taille du fichier (`st_size`).
 *        - Permissions (`st_mode`).
 *        - Horodatage de la dernière modification (`st_mtime`).
 *
 * 2. Gestion des erreurs :
 *    - Si `stat()` retourne `-1`, cela signifie que le fichier n'existe pas ou qu'il n'est pas accessible.
 *    - Formatte un message d'erreur dans `buffer` indiquant que le fichier n'a pas été trouvé.
 *    - Envoie ce message d'erreur au client via `write(client_socket, ...)`.
 *    - Termine l'exécution de la fonction avec `return`.
 */

    struct stat file_stat;
    char buffer[BUFFER_SIZE];

    if (stat(filename, &file_stat) == -1) {
        snprintf(buffer, BUFFER_SIZE, "File not found: %s\n", filename);
        write(client_socket, buffer, strlen(buffer));
        return;
    }

    /*
 * Étape : Formatage des informations du fichier
 *
 * 1. `snprintf(buffer, BUFFER_SIZE, ...)` :
 *    - Formatte les informations récupérées dans un tampon de taille `BUFFER_SIZE`.
 *    - Les informations incluent :
 *        - `File`: Nom du fichier (`filename`).
 *        - `Size`: Taille du fichier en octets (`file_stat.st_size`).
 *        - `Permissions`: Permissions du fichier sous forme octale (`file_stat.st_mode & 0777`).
 *        - `Last modified`: Timestamp UNIX de la dernière modification (`file_stat.st_mtime`).
 *    - Chaque champ est séparé par un saut de ligne pour faciliter la lecture.
 *
 * Étape : Envoi des informations au client
 *
 * 1. `write(client_socket, buffer, strlen(buffer))` :
 *    - Envoie le contenu formaté de `buffer` au client via le socket.
 */

    snprintf(buffer, BUFFER_SIZE,
             "File: %s\nSize: %ld bytes\nPermissions: %o\nLast modified: %ld\n",
             filename, file_stat.st_size, file_stat.st_mode & 0777, file_stat.st_mtime);
    write(client_socket, buffer, strlen(buffer));
}

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SERVER_PORT 1234
#define BUFFER_SIZE 256

void handleConnection(int server_socket);

/*
 * Fonction : main
 * Description : Point d'entrée du programme client. Initialise une connexion avec le serveur et gère les interactions.
 * Entrée : 
 *   - `argc` : Nombre d'arguments en ligne de commande.
 *   - `argv` : Tableau des arguments. Attendu : le nom du programme et l'adresse IP du serveur.
 * Sortie :
 *   - Retourne `EXIT_SUCCESS` en cas de succès, ou `EXIT_FAILURE` en cas d'échec.
 */

int main(int argc, char* argv[]) {

/*
 * Étape : Vérification des arguments en ligne de commande
 *
 * 1. `argc != 2` :
 *    - Vérifie que le programme reçoit exactement 2 arguments :
 *      - `argv[0]` : Le nom du programme.
 *      - `argv[1]` : L'adresse IP du serveur.
 *
 * 2. Affiche un message d'usage et retourne `EXIT_FAILURE` si le nombre d'arguments est incorrect.
 */

    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return EXIT_FAILURE;
    }

/*
 * Étape : Création du socket client
 *
 * 1. `socket(AF_INET, SOCK_STREAM, 0)` :
 *    - Crée un socket IPv4 (`AF_INET`) de type TCP (`SOCK_STREAM`) pour la communication réseau.
 *    - Retourne un descripteur de fichier (`client_socket`).
 *
 * 2. Vérification de l'échec :
 *    - Si `socket()` retourne `-1`, affiche un message d'erreur avec `perror()`.
 *    - Termine le programme avec `EXIT_FAILURE`.
 */

    int client_socket;
    struct sockaddr_in server_addr;

    // Création du socket client
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

/*
 * Étape : Configuration de l'adresse du serveur
 *
 * 1. `server_addr.sin_family = AF_INET` :
 *    - Spécifie que la famille d'adresses utilisée est IPv4.
 *
 * 2. `server_addr.sin_port = htons(SERVER_PORT)` :
 *    - Convertit le numéro de port en format big-endian (réseau) avec `htons`.
 *
 * 3. `inet_pton(AF_INET, argv[1], &server_addr.sin_addr)` :
 *    - Convertit l'adresse IP fournie (ex. "127.0.0.1") en format binaire.
 *    - Retourne un résultat ≤ 0 en cas d'échec (adresse invalide).
 *
 * 4. Gestion des erreurs :
 *    - Si `inet_pton` échoue, affiche un message d'erreur avec `perror()`.
 *    - Ferme le socket avec `close(client_socket)` et retourne `EXIT_FAILURE`.
 */

    // Configuration de l'adresse du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        close(client_socket);
        return EXIT_FAILURE;
    }

/*
 * Étape : Connexion au serveur
 *
 * 1. `connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))` :
 *    - Établit une connexion avec le serveur à l'adresse spécifiée.
 *    - Retourne `-1` en cas d'échec.
 *
 * 2. Gestion des erreurs :
 *    - Si `connect()` échoue, affiche un message d'erreur avec `perror()`.
 *    - Ferme le socket avec `close(client_socket)` et retourne `EXIT_FAILURE`.
 */

    // Connexion au serveur
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection to server failed");
        close(client_socket);
        return EXIT_FAILURE;
    }

    printf("Connected to server %s on port %d\n", argv[1], SERVER_PORT);

/*
 * Étape : Gestion de la connexion avec le serveur
 *
 * 1. Appelle la fonction `handleConnection(client_socket)` pour gérer les interactions avec le serveur.
 *    - Envoie les commandes saisies par l'utilisateur.
 *    - Affiche les réponses reçues du serveur.
 */

    // Gestion de la connexion avec le serveur
    handleConnection(client_socket);

/*
 * Étape : Fermeture du socket client
 *
 * 1. `close(client_socket)` :
 *    - Ferme la connexion avec le serveur et libère les ressources associées au socket.
 */

    // Fermeture du socket client
    close(client_socket);
    return EXIT_SUCCESS;
}

/*
 * Fonction : handleConnection
 * Description : Gère l'interaction entre le client et le serveur. Permet à l'utilisateur d'envoyer des commandes au serveur 
 *               et d'afficher les réponses reçues.
 * Entrée :
 *   - `server_socket` : Descripteur de fichier du socket connecté au serveur.
 */


void handleConnection(int server_socket) {

/*
 * Étape : Déclaration des buffers pour les commandes et réponses
 *
 * 1. `char command[BUFFER_SIZE]` :
 *    - Tampon utilisé pour stocker les commandes saisies par l'utilisateur.
 *
 * 2. `char response[BUFFER_SIZE]` :
 *    - Tampon utilisé pour stocker les réponses reçues du serveur.
 */

    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

/*
 * Étape : Boucle principale pour envoyer des commandes et lire les réponses
 *
 * 1. `while (1)` :
 *    - Boucle infinie pour permettre à l'utilisateur d'envoyer plusieurs commandes successivement.
 *    - La boucle se termine si l'utilisateur entre "quit" ou en cas d'erreur.
 */

    while (1) {

 /*
     * Étape : Lecture de la commande utilisateur
     *
     * 1. `printf(...)` :
     *    - Affiche un message d'invite demandant une commande.
     *
     * 2. `memset(command, 0, BUFFER_SIZE)` :
     *    - Réinitialise le tampon de commande à chaque itération pour éviter des résidus de données précédentes.
     *
     * 3. `fgets(command, BUFFER_SIZE, stdin)` :
     *    - Lit la commande saisie par l'utilisateur depuis l'entrée standard.
     */

        printf("Enter a command (i to list files, s <filename> for file info, quit to disconnect): ");
        memset(command, 0, BUFFER_SIZE);
        fgets(command, BUFFER_SIZE, stdin);

 /*
     * Étape : Nettoyage de la commande
     *
     * 1. `strlen(command)` :
     *    - Détermine la longueur de la commande saisie.
     *
     * 2. Retirer le saut de ligne :
     *    - Si la commande se termine par un caractère de saut de ligne (`\n`), remplacez-le par le caractère de fin de chaîne (`\0`).
     */

        // Retirer le caractère de saut de ligne
        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

 /*
     * Étape : Envoi de la commande au serveur
     *
     * 1. `write(server_socket, command, strlen(command))` :
     *    - Envoie la commande saisie par l'utilisateur au serveur via le socket.
     *
     * 2. Gestion des erreurs :
     *    - Si `write()` retourne `-1`, affiche un message d'erreur avec `perror()` et termine la boucle.
     */

        // Envoyer la commande au serveur
        if (write(server_socket, command, strlen(command)) == -1) {
            perror("Failed to send command");
            break;
        }

 /*
     * Étape : Déconnexion
     *
     * 1. Si la commande est "quit", afficher un message de déconnexion et sortir de la boucle.
     */

        // Si la commande est "quit", sortir de la boucle
        if (strncmp(command, "quit", 4) == 0) {
            printf("Disconnecting from server...\n");
            break;
        }

    /*
     * Étape : Lecture de la réponse du serveur
     *
     * 1. `printf("Response from server:\n")` :
     *    - Affiche un message pour indiquer que la réponse du serveur va être affichée.
     *
     * 2. Boucle pour lire les données reçues :
     *    - Continue de lire les données du serveur jusqu'à ce que la réponse complète ait été reçue.
     */


        // Lire la réponse du serveur
        printf("Response from server:\n");
        while (1) {

/*
         * a. `memset(response, 0, BUFFER_SIZE)` :
         *    - Réinitialise le tampon de réponse pour éviter les résidus.
         *
         * b. `read(server_socket, response, BUFFER_SIZE - 1)` :
         *    - Lit une portion de la réponse du serveur dans le tampon `response`.
         *    - Limite la lecture à `BUFFER_SIZE - 1` pour laisser de la place pour le caractère de fin de chaîne.
         *
         * c. Gestion des erreurs ou fermeture de connexion :
         *    - Si `read()` retourne une valeur ≤ 0, cela signifie que la connexion a été fermée ou qu'une erreur est survenue.
         *    - Affiche un message et termine la fonction.
         */

            memset(response, 0, BUFFER_SIZE);
            ssize_t bytes_read = read(server_socket, response, BUFFER_SIZE - 1);
            if (bytes_read <= 0) {
                printf("Connection closed by server.\n");
                return;
            }

        /*
         * d. Affichage de la réponse :
         *    - Affiche la réponse partielle reçue sur le terminal client.
         */

            // Affiche la réponse et vérifie s'il reste des données
            printf("%s", response);

        /*
         * e. Vérification de la fin de la réponse :
         *    - Si le nombre d'octets lus est inférieur à la taille maximale du tampon, il n'y a plus de données à lire.
         *    - Sortir de la boucle pour attendre une nouvelle commande.
         */

            // Si la réponse est complète, sortir de la boucle
            if (bytes_read < BUFFER_SIZE - 1) {
                break;
            }
        }
    }
}

/*
Fonctionnalités du programme client :
Connexion au serveur :

L'utilisateur doit fournir l'adresse IP du serveur en argument de la ligne de commande.
Le client se connecte au port 1234 du serveur.
Envoi de commandes au serveur :

i ou I : Liste les fichiers .c dans le répertoire du serveur.
s <nom_fichier> : Demande les informations d'un fichier spécifique.
quit : Déconnecte proprement le client du serveur.
Réception et affichage des réponses :

Le client affiche les réponses du serveur directement sur le terminal.
Gestion propre de la déconnexion :

Si le client entre quit, la connexion est fermée proprement.
*/

/*
$ gcc -o server server.c -lpthread
$ ./server
Server is listening on port 1234...
Client connected: 127.0.0.1
*/

/*
$ gcc -o client client.c
$ ./client 127.0.0.1
Connected to server 127.0.0.1 on port 1234
Enter a command (i to list files, s <filename> for file info, quit to disconnect): i
Response from server:
main.c
server.c
client.c
Enter a command (i to list files, s <filename> for file info, quit to disconnect): s server.c
Response from server:
File: server.c
Size: 2048 bytes
Permissions: 644
Last modified: 1673654822
Enter a command (i to list files, s <filename> for file info, quit to disconnect): quit
Disconnecting from server...
*/

#include <sys/stat.h>
#include <stdio.h>
#include <time.h>

/*
 * Fonction : displayFileInfo
 * Description : Affiche les informations détaillées d'un fichier, telles que sa taille, ses permissions, son type,
 *               et les horodatages de modification, d'accès et de changement.
 * Entrée :
 *   - `filename` : Nom du fichier pour lequel les informations doivent être affichées.
 */

void displayFileInfo(const char* filename) {

/*
 * Étape : Récupération des informations sur le fichier
 *
 * 1. `stat(filename, &file_stat)` :
 *    - Appelle la fonction `stat()` pour récupérer les métadonnées du fichier `filename`.
 *    - Stocke les informations dans la structure `file_stat` si le fichier est accessible.
 *
 * 2. Gestion des erreurs :
 *    - Si `stat()` retourne `-1`, cela signifie que le fichier n'existe pas ou qu'il n'est pas accessible.
 *    - Affiche un message d'erreur avec `perror()` et termine l'exécution de la fonction.
 */

    struct stat file_stat;

    // Appel à stat() pour récupérer les informations sur le fichier
    if (stat(filename, &file_stat) == -1) {
        perror("Failed to get file status");
        return;
    }

/*
 * Étape : Affichage des informations générales
 *
 * 1. Nom du fichier :
 *    - Affiche le nom du fichier traité.
 *
 * 2. Ligne de séparation :
 *    - Ajoute une ligne horizontale pour structurer l'affichage.
 */

    // Affichage des informations du fichier
    printf("Informations sur le fichier : %s\n", filename);
    printf("-----------------------------------------\n");

/*
 * Étape : Taille du fichier
 *
 * 1. `file_stat.st_size` :
 *    - Affiche la taille du fichier en octets.
 */

    // Taille du fichier
    printf("Taille : %ld octets\n", file_stat.st_size);

/*
 * Étape : Permissions du fichier
 *
 * 1. `file_stat.st_mode & 0777` :
 *    - Extrait les bits correspondant aux permissions UNIX en mode octal.
 *    - Affiche les permissions sous forme numérique (exemple : `644`).
 */

    // Permissions (mode)
    printf("Permissions : %o\n", file_stat.st_mode & 0777);

/*
 * Étape : Type du fichier
 *
 * 1. `S_ISREG(file_stat.st_mode)` :
 *    - Vérifie si le fichier est un fichier régulier.
 *
 * 2. Autres macros (`S_ISDIR`, `S_ISCHR`, etc.) :
 *    - Vérifient si le fichier est un répertoire, un lien symbolique, un pipe, un socket, etc.
 */

    // Type du fichier
    printf("Type de fichier : ");
    if (S_ISREG(file_stat.st_mode)) printf("Fichier régulier\n");
    else if (S_ISDIR(file_stat.st_mode)) printf("Répertoire\n");
    else if (S_ISCHR(file_stat.st_mode)) printf("Fichier spécial caractère\n");
    else if (S_ISBLK(file_stat.st_mode)) printf("Fichier spécial bloc\n");
    else if (S_ISFIFO(file_stat.st_mode)) printf("FIFO/Pipe\n");
    else if (S_ISLNK(file_stat.st_mode)) printf("Lien symbolique\n");
    else if (S_ISSOCK(file_stat.st_mode)) printf("Socket\n");
    else printf("Inconnu\n");

/*
 * Étape : Identifiants de l'utilisateur et du groupe
 *
 * 1. `file_stat.st_uid` :
 *    - Affiche l'UID (User ID) du propriétaire du fichier.
 *
 * 2. `file_stat.st_gid` :
 *    - Affiche le GID (Group ID) associé au fichier.
 */

    // UID et GID
    printf("UID : %d\n", file_stat.st_uid);
    printf("GID : %d\n", file_stat.st_gid);

/*
 * Étape : Horodatages
 *
 * 1. Conversion des timestamps en chaînes lisibles :
 *    - `file_stat.st_mtime` : Horodatage de la dernière modification.
 *    - `file_stat.st_atime` : Horodatage du dernier accès.
 *    - `file_stat.st_ctime` : Horodatage du dernier changement de métadonnées.
 *
 * 2. `strftime(...)` :
 *    - Convertit chaque horodatage en une chaîne lisible au format `YYYY-MM-DD HH:MM:SS`.
 */

    // Dernière modification
    char mtime[20];
    strftime(mtime, sizeof(mtime), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));
    printf("Dernière modification : %s\n", mtime);

    // Dernier accès
    char atime[20];
    strftime(atime, sizeof(atime), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_atime));
    printf("Dernier accès : %s\n", atime);

    // Dernier changement
    char ctime[20];
    strftime(ctime, sizeof(ctime), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_ctime));
    printf("Dernier changement : %s\n", ctime);

    printf("-----------------------------------------\n");
}

int main() {
    // Exemple de fichier à inspecter
    const char* filename = "exemple.txt";

    // Appel de la fonction pour afficher les informations
    displayFileInfo(filename);

    return 0;
}

/*
Explication des champs affichés :
Taille du fichier :

Affichée avec file_stat.st_size, donne la taille du fichier en octets.
Permissions (mode) :

Les permissions du fichier sont obtenues avec file_stat.st_mode & 0777, qui extrait les bits représentant les permissions pour le propriétaire, le groupe, et les autres utilisateurs.
Type de fichier :

Déterminé à l'aide des macros S_ISREG, S_ISDIR, etc. :
S_ISREG : Fichier régulier.
S_ISDIR : Répertoire.
S_ISCHR : Fichier spécial caractère.
S_ISBLK : Fichier spécial bloc.
S_ISFIFO : FIFO/Pipe.
S_ISLNK : Lien symbolique.
S_ISSOCK : Socket.
UID (User ID) et GID (Group ID) :

Informations sur le propriétaire (file_stat.st_uid) et le groupe (file_stat.st_gid) du fichier.
Dates :

Dernière modification (st_mtime) : Quand le contenu du fichier a été modifié.
Dernier accès (st_atime) : Quand le fichier a été lu pour la dernière fois.
Dernier changement (st_ctime) : Quand les métadonnées (permissions, propriétaire, etc.) ont été modifiées.
*/

/*
Informations sur le fichier : exemple.txt
-----------------------------------------
Taille : 2048 octets
Permissions : 644
Type de fichier : Fichier régulier
UID : 1000
GID : 1000
Dernière modification : 2024-12-16 10:30:45
Dernier accès : 2024-12-16 09:15:20
Dernier changement : 2024-12-16 10:30:50
-----------------------------------------
*/




/*
2)  Pouvoir afficher (côté client) les informations d’un fichier dans une fenêtre ncurses
(ou équivalente) et que cet affichage soit effectué par un thread indépendant de la
lecture sur le socket qui réceptionne les informations sur les fichiers.
Pouvoir  alors modifier le code (langage C) de votre programme client du # 1
pour permettre la communication avec le thread d'affichage.
Pouvoir alors écrire le code du  programme en langage C détaillé du thread d'affichage.
*/

/*
Étape 1 : Configuration du client pour gérer un thread d'affichage
Structure de communication entre threads :

Créer une structure partagée entre le thread de lecture des informations sur le socket et le thread d'affichage. Cette structure servira à transmettre les informations au thread d'affichage.
Thread d'affichage indépendant :

Ce thread utilise ncurses pour afficher les informations du fichier dans une fenêtre dédiée.
Le thread d'affichage attend les nouvelles données en utilisant un mécanisme de synchronisation (comme un sémaphore ou un mutex).
*/

#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVER_PORT 1234
#define BUFFER_SIZE 256

typedef struct {
    char info[BUFFER_SIZE];  // Contient les informations sur le fichier
    pthread_mutex_t mutex;  // Mutex pour synchroniser l'accès aux données
    pthread_cond_t cond;    // Condition pour signaler de nouvelles données
    int ready;              // Indique si de nouvelles données sont prêtes à être affichées
} SharedData;

// Fonction pour le thread de gestion de l'affichage
void* displayThread(void* arg);

// Fonction pour la gestion des commandes et des données reçues
void handleConnection(int server_socket, SharedData* shared);

/*
 * Fonction : main
 * Description : Point d'entrée du programme client. Initialise une connexion avec le serveur,
 *               gère les interactions avec le serveur et les affichages via un thread indépendant.
 * Entrée :
 *   - `argc` : Nombre d'arguments en ligne de commande.
 *   - `argv` : Tableau des arguments. Attendu : <nom_du_programme> <server_ip>.
 * Sortie :
 *   - Retourne `EXIT_SUCCESS` en cas de succès ou `EXIT_FAILURE` en cas d'erreur.
 */

int main(int argc, char* argv[]) {

/*
 * Étape 1 : Vérification des arguments en ligne de commande
 *
 * 1. `argc != 2` :
 *    - Vérifie si l'utilisateur a fourni exactement 2 arguments.
 *    - `argv[0]` : Nom du programme.
 *    - `argv[1]` : Adresse IP du serveur.
 *
 * 2. Affiche un message d'utilisation et termine le programme avec `EXIT_FAILURE` si incorrect.
 */

    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return EXIT_FAILURE;
    }

/*
 * Étape 2 : Création du socket client
 *
 * 1. `socket(AF_INET, SOCK_STREAM, 0)` :
 *    - Crée un socket IPv4 (`AF_INET`) de type TCP (`SOCK_STREAM`) pour la communication réseau.
 *    - Retourne un descripteur de fichier (`client_socket`).
 *
 * 2. Gestion des erreurs :
 *    - Si la création échoue (`socket()` retourne `-1`), affiche un message d'erreur avec `perror()`.
 *    - Termine le programme avec `EXIT_FAILURE`.
 */

    int client_socket;
    struct sockaddr_in server_addr;

    // Création du socket client
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

/*
 * Étape 3 : Configuration de l'adresse du serveur
 *
 * 1. `server_addr.sin_family = AF_INET` :
 *    - Spécifie l'utilisation du protocole IPv4.
 *
 * 2. `server_addr.sin_port = htons(SERVER_PORT)` :
 *    - Convertit le numéro de port `SERVER_PORT` en ordre d'octets réseau (big-endian).
 *
 * 3. `inet_pton(AF_INET, argv[1], &server_addr.sin_addr)` :
 *    - Convertit l'adresse IP fournie en ligne de commande en format binaire utilisable par le socket.
 *
 * 4. Gestion des erreurs :
 *    - Si l'adresse IP est invalide ou inatteignable, `inet_pton` retourne une valeur ≤ 0.
 *    - Affiche un message d'erreur avec `perror()` et ferme le socket.
 */

    // Configuration de l'adresse du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        close(client_socket);
        return EXIT_FAILURE;
    }

/*
 * Étape 4 : Connexion au serveur
 *
 * 1. `connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))` :
 *    - Tente d'établir une connexion avec le serveur spécifié.
 *
 * 2. Gestion des erreurs :
 *    - Si la connexion échoue, `connect()` retourne `-1`.
 *    - Affiche un message d'erreur avec `perror()` et ferme le socket.
 */

    // Connexion au serveur
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection to server failed");
        close(client_socket);
        return EXIT_FAILURE;
    }

    printf("Connected to server %s on port %d\n", argv[1], SERVER_PORT);

/*
 * Étape 5 : Initialisation des données partagées pour le thread d'affichage
 *
 * 1. `SharedData shared` :
 *    - Structure partagée entre le thread principal et le thread d'affichage.
 *    - Contient un tampon pour les informations, un mutex pour la synchronisation,
 *      une condition pour signaler l'arrivée de nouvelles données, et un indicateur `ready`.
 *
 * 2. Initialisation des composants :
 *    - `pthread_mutex_init()` : Initialise le mutex.
 *    - `pthread_cond_init()` : Initialise la condition.
 *    - `shared.ready = 0` : Initialise l'indicateur à "pas prêt".
 */

    // Initialisation des données partagées pour l'affichage
    SharedData shared;
    memset(&shared, 0, sizeof(SharedData));
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_cond_init(&shared.cond, NULL);
    shared.ready = 0;

/*
 * Étape 6 : Création du thread d'affichage
 *
 * 1. `pthread_create()` :
 *    - Crée un thread pour exécuter la fonction `displayThread` en arrière-plan.
 *    - Passe l'adresse de la structure `shared` comme argument pour permettre la communication.
 *
 * 2. Le thread d'affichage attend des données dans `shared` pour les afficher au client.
 */

    // Création du thread d'affichage
    pthread_t display_thread;
    pthread_create(&display_thread, NULL, displayThread, &shared);

/*
 * Étape 7 : Gestion des commandes et des données reçues
 *
 * 1. `handleConnection(client_socket, &shared)` :
 *    - Gère les commandes saisies par l'utilisateur (comme "i", "s <filename>", ou "quit").
 *    - Reçoit les réponses du serveur et transmet les informations à `shared`.
 *    - Le thread d'affichage utilise les données dans `shared` pour les afficher.
 */

    // Gestion des commandes et des données reçues
    handleConnection(client_socket, &shared);

/*
 * Étape 8 : Terminaison et nettoyage
 *
 * 1. Annulation du thread d'affichage :
 *    - `pthread_cancel(display_thread)` : Demande l'annulation du thread d'affichage.
 *    - `pthread_join()` : Attend que le thread se termine proprement.
 *
 * 2. Nettoyage des ressources partagées :
 *    - `pthread_mutex_destroy()` : Détruit le mutex.
 *    - `pthread_cond_destroy()` : Détruit la condition.
 *
 * 3. Fermeture du socket client :
 *    - Libère les ressources réseau en fermant le socket avec `close(client_socket)`.
 *
 * 4. Retour :
 *    - Retourne `EXIT_SUCCESS` pour indiquer la réussite du programme.
 */

    // Attente de la fin du thread d'affichage
    pthread_cancel(display_thread);
    pthread_join(display_thread, NULL);

    // Nettoyage
    pthread_mutex_destroy(&shared.mutex);
    pthread_cond_destroy(&shared.cond);
    close(client_socket);

    return EXIT_SUCCESS;
}

/*
 * Fonction : handleConnection
 * Description : Gère les interactions entre le client et le serveur. Permet à l'utilisateur d'envoyer des commandes
 *               au serveur et transmet les réponses reçues au thread d'affichage via une structure partagée.
 * Entrée :
 *   - `server_socket` : Descripteur de fichier du socket connecté au serveur.
 *   - `shared` : Pointeur vers une structure `SharedData` pour transmettre les données au thread d'affichage.
 */

void handleConnection(int server_socket, SharedData* shared) {

/*
 * Étape 1 : Déclaration des tampons pour les commandes et réponses
 *
 * 1. `char command[BUFFER_SIZE]` :
 *    - Tampon pour stocker les commandes saisies par l'utilisateur.
 *
 * 2. `char response[BUFFER_SIZE]` :
 *    - Tampon pour stocker les réponses reçues du serveur.
 */

    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

/*
 * Étape 2 : Boucle principale d'interaction avec le serveur
 *
 * 1. `while (1)` :
 *    - Maintient une boucle infinie pour permettre à l'utilisateur d'envoyer plusieurs commandes au serveur.
 *    - La boucle se termine si l'utilisateur envoie "quit" ou en cas de problème de connexion.
 */

    while (1) {

    /*
     * Étape 3 : Lecture de la commande utilisateur
     *
     * 1. `printf(...)` :
     *    - Affiche un message invitant l'utilisateur à entrer une commande.
     *
     * 2. `memset(command, 0, BUFFER_SIZE)` :
     *    - Initialise le tampon `command` pour s'assurer qu'il est vide avant chaque saisie.
     *
     * 3. `fgets(command, BUFFER_SIZE, stdin)` :
     *    - Lit une ligne de commande saisie par l'utilisateur depuis l'entrée standard.
     *
     * 4. Retrait du saut de ligne :
     *    - Remplace le caractère `\n` final (s'il existe) par `\0` pour assurer une chaîne propre.
     */

        printf("Enter a command (s <filename> for file info, quit to disconnect): ");
        memset(command, 0, BUFFER_SIZE);
        fgets(command, BUFFER_SIZE, stdin);

        // Retirer le caractère de saut de ligne
        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

    /*
     * Étape 4 : Envoi de la commande au serveur
     *
     * 1. `write(server_socket, command, strlen(command))` :
     *    - Envoie la commande saisie par l'utilisateur au serveur via le socket.
     *
     * 2. Gestion des erreurs :
     *    - Si `write()` retourne `-1`, cela signifie que l'envoi a échoué.
     *    - Affiche un message d'erreur avec `perror()` et termine la boucle.
     */

        // Envoyer la commande au serveur
        if (write(server_socket, command, strlen(command)) == -1) {
            perror("Failed to send command");
            break;
        }

    /*
     * Étape 5 : Gestion de la déconnexion
     *
     * 1. Si la commande est "quit", affiche un message de déconnexion.
     * 2. Sort de la boucle pour terminer proprement l'interaction.
     */

        // Si la commande est "quit", sortir de la boucle
        if (strncmp(command, "quit", 4) == 0) {
            printf("Disconnecting from server...\n");
            break;
        }

    /*
     * Étape 6 : Lecture de la réponse du serveur
     *
     * 1. `memset(response, 0, BUFFER_SIZE)` :
     *    - Initialise le tampon `response` pour s'assurer qu'il est vide avant la réception.
     *
     * 2. `read(server_socket, response, BUFFER_SIZE - 1)` :
     *    - Lit la réponse envoyée par le serveur via le socket.
     *    - Stocke jusqu'à `BUFFER_SIZE - 1` octets pour laisser de la place au caractère de fin de chaîne `\0`.
     *
     * 3. Gestion des erreurs ou de la fermeture de connexion :
     *    - Si `read()` retourne une valeur ≤ 0, cela signifie que la connexion a été fermée par le serveur.
     *    - Affiche un message et termine la boucle.
     */

        // Lire la réponse du serveur
        memset(response, 0, BUFFER_SIZE);
        ssize_t bytes_read = read(server_socket, response, BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            printf("Connection closed by server.\n");
            break;
        }

        response[bytes_read] = '\0';// Assure que la réponse est une chaîne de caractères valide

    /*
     * Étape 7 : Transmission des données au thread d'affichage
     *
     * 1. `pthread_mutex_lock(&shared->mutex)` :
     *    - Verrouille le mutex pour protéger l'accès à la structure partagée `shared`.
     *
     * 2. `strncpy(shared->info, response, BUFFER_SIZE)` :
     *    - Copie la réponse reçue dans le champ `info` de la structure partagée.
     *
     * 3. `shared->ready = 1` :
     *    - Met à jour l'indicateur `ready` pour signaler qu'il y a de nouvelles données à afficher.
     *
     * 4. `pthread_cond_signal(&shared->cond)` :
     *    - Envoie un signal au thread d'affichage pour lui indiquer que de nouvelles données sont prêtes.
     *
     * 5. `pthread_mutex_unlock(&shared->mutex)` :
     *    - Déverrouille le mutex pour permettre au thread d'affichage d'accéder aux données.
     */

        // Transmettre les données au thread d'affichage
        pthread_mutex_lock(&shared->mutex);
        strncpy(shared->info, response, BUFFER_SIZE);
        shared->ready = 1;
        pthread_cond_signal(&shared->cond);  // Signale qu'il y a de nouvelles données
        pthread_mutex_unlock(&shared->mutex);
    }
}


/*
 * Fonction : displayThread
 * Description : Thread indépendant chargé d'afficher les informations des fichiers reçues 
 *               dans une fenêtre `ncurses` lorsqu'elles sont prêtes.
 * Entrée :
 *   - `arg` : Pointeur vers une structure `SharedData` contenant les données partagées
 *             entre le thread principal et ce thread d'affichage.
 * Sortie :
 *   - Retourne NULL lorsque le thread se termine (jamais atteint ici en pratique).
 */

void* displayThread(void* arg) {
    SharedData* shared = (SharedData*)arg;

/*
 * Étape 1 : Initialisation de la bibliothèque ncurses
 *
 * 1. `initscr()` :
 *    - Initialise l'environnement ncurses et prépare l'écran pour le mode d'affichage texte.
 *
 * 2. `noecho()` :
 *    - Empêche l'affichage automatique des caractères tapés par l'utilisateur.
 *
 * 3. `cbreak()` :
 *    - Désactive la mise en tampon de ligne complète pour permettre une lecture instantanée des entrées.
 */

    // Initialisation de ncurses
    initscr();
    noecho();
    cbreak();

/*
 * Étape 2 : Création d'une fenêtre ncurses pour l'affichage
 *
 * 1. `newwin(LINES - 1, COLS, 0, 0)` :
 *    - Crée une nouvelle fenêtre prenant tout l'écran sauf la dernière ligne.
 *    - `LINES - 1` : Hauteur de la fenêtre (toutes les lignes sauf une).
 *    - `COLS` : Largeur de la fenêtre (toutes les colonnes).
 *    - `0, 0` : Position (x, y) en haut à gauche de l'écran.
 *
 * 2. `box(info_window, 0, 0)` :
 *    - Dessine une bordure autour de la fenêtre pour la délimiter visuellement.
 *
 * 3. `mvwprintw(info_window, 0, 1, "File Information")` :
 *    - Affiche le titre "File Information" sur la première ligne de la fenêtre.
 *
 * 4. `wrefresh(info_window)` :
 *    - Met à jour l'affichage de la fenêtre pour montrer les changements.
 */

    // Création d'une fenêtre pour l'affichage
    WINDOW* info_window = newwin(LINES - 1, COLS, 0, 0);
    box(info_window, 0, 0);
    mvwprintw(info_window, 0, 1, "File Information");
    wrefresh(info_window);

/*
 * Étape 3 : Boucle principale d'attente et d'affichage
 *
 * 1. `while (1)` :
 *    - Boucle infinie qui attend les nouvelles données dans la structure partagée `SharedData`
 *      et les affiche dans la fenêtre ncurses.
 */

    while (1) {

/*
     * Étape 4 : Attente des nouvelles données
     *
     * 1. `pthread_mutex_lock(&shared->mutex)` :
     *    - Verrouille le mutex pour accéder à la structure partagée `shared` en toute sécurité.
     *
     * 2. `while (!shared->ready)` :
     *    - Si les données ne sont pas prêtes (`shared->ready == 0`), 
     *      le thread se met en attente via `pthread_cond_wait`.
     *
     * 3. `pthread_cond_wait(&shared->cond, &shared->mutex)` :
     *    - Attend le signal envoyé par le thread principal pour indiquer que les données sont prêtes.
     *    - Relâche temporairement le mutex pendant l'attente.
     */

        // Attente de nouvelles données
        pthread_mutex_lock(&shared->mutex);
        while (!shared->ready) {
            pthread_cond_wait(&shared->cond, &shared->mutex);  // Attend un signal
        }

/*
     * Étape 5 : Affichage des nouvelles données
     *
     * 1. `wclear(info_window)` :
     *    - Efface le contenu précédent de la fenêtre.
     *
     * 2. `box(info_window, 0, 0)` :
     *    - Redessine la bordure de la fenêtre.
     *
     * 3. `mvwprintw(info_window, 0, 1, "File Information")` :
     *    - Réaffiche le titre "File Information" en haut de la fenêtre.
     *
     * 4. `mvwprintw(info_window, 2, 2, "%s", shared->info)` :
     *    - Affiche les informations du fichier reçues dans `shared->info`, en partant de la ligne 2, colonne 2.
     *
     * 5. `wrefresh(info_window)` :
     *    - Met à jour l'affichage de la fenêtre avec les nouvelles informations.
     */

        // Affichage des nouvelles données
        wclear(info_window);
        box(info_window, 0, 0);
        mvwprintw(info_window, 0, 1, "File Information");
        mvwprintw(info_window, 2, 2, "%s", shared->info);
        wrefresh(info_window);

    /*
     * Étape 6 : Réinitialisation de l'état
     *
     * 1. `shared->ready = 0` :
     *    - Réinitialise l'indicateur `ready` pour indiquer que les données ont été affichées.
     *
     * 2. `pthread_mutex_unlock(&shared->mutex)` :
     *    - Déverrouille le mutex pour permettre au thread principal d'écrire de nouvelles données.
     */

        // Réinitialisation de l'état
        shared->ready = 0;
        pthread_mutex_unlock(&shared->mutex);
    }

/*
 * Étape 7 : Nettoyage de ncurses (jamais atteint en pratique)
 *
 * 1. `delwin(info_window)` :
 *    - Détruit la fenêtre créée pour libérer la mémoire associée.
 *
 * 2. `endwin()` :
 *    - Termine l'environnement ncurses et restaure le mode normal du terminal.
 */

    // Nettoyage de ncurses (jamais atteint en pratique)
    delwin(info_window);
    endwin();
    return NULL;
}

/*
Explications des fonctionnalités ajoutées :
Thread d'affichage :

Un thread indépendant est créé pour gérer l'affichage des informations des fichiers dans une fenêtre ncurses.
Ce thread utilise un mécanisme de synchronisation (pthread_cond_t) pour attendre que de nouvelles données soient prêtes à afficher.
Structure partagée (SharedData) :

Contient les informations du fichier (info), un mutex (mutex) pour synchroniser les accès et une condition (cond) pour signaler la disponibilité des nouvelles données.
Gestion des commandes :

La fonction handleConnection() envoie les commandes au serveur et reçoit les réponses. Les données reçues sont transmises au thread d'affichage via la structure partagée.
Fenêtre ncurses :

Une fenêtre est utilisée pour afficher les informations des fichiers. Les nouvelles données remplacent les anciennes dès qu'elles sont disponibles.
*/

/*
Exemple de sortie dans la fenêtre ncurses :
+----------------------------------------------+
| File Information                             |
+----------------------------------------------+
| File: server.c                               |
| Size: 2048 bytes                             |
| Permissions: 644                             |
| Last modified: 2024-12-16 10:30:45           |
+----------------------------------------------+
Ce code permet une communication fluide entre le thread principal 
(lecture depuis le socket) et le thread d'affichage (ncurses).
*/

/*
Étape 1 : Compréhension du problème
Dans le contexte du serveur multi-clients (comme dans le #1 ou le TP3), chaque processus ou thread du programme serveur peut ouvrir un certain nombre de fichiers ou sockets pour gérer les connexions clients.
Linux maintient ces fichiers ouverts dans une table des fichiers ouverts, et cette table peut être inspectée dans le système de fichiers /proc en ligne de commande.

La commande ls -l /proc/<pid>/fd permet d’afficher les descripteurs de fichiers ouverts par un processus ou un thread donné, où :

<pid> est l'ID du processus (ou du thread principal).
/fd représente les descripteurs ouverts associés à ce processus.
*/

/*Étape 2 : Affichage des fichiers ouverts via un script
Pour afficher la liste des fichiers ouverts par un programme (exemple : le programme serveur), vous pouvez exécuter une commande comme suit dans un shell :

ls -l /proc/$(pgrep server_program_name)/fd

Explication :

pgrep server_program_name : Trouve le PID du programme serveur (remplacez server_program_name par le nom de votre programme serveur).
/proc/<pid>/fd : Contient les liens symboliques vers les fichiers, sockets, pipes, etc., ouverts par le processus.
*/

/*
Étape 3 : Intégrer cette commande dans le serveur
Si vous voulez intégrer cette fonctionnalité directement dans le programme serveur pour qu’il puisse afficher sa propre liste de fichiers ouverts à la demande, voici comment faire :

Ajout d'une commande spécifique pour afficher les fichiers ouverts :

Implémentez une commande (ex. lsfd) côté client pour demander au serveur d’afficher sa table de fichiers ouverts.
Utilisation de system() côté serveur :

Le serveur peut exécuter la commande ls -l /proc/<pid>/fd en utilisant la fonction system().
*/

/*
Exemple de Code C pour Intégrer l'Affichage des Fichiers Ouverts dans le Serveur
Voici une implémentation dans le contexte d'un programme serveur comme décrit dans le #1 :
*/

//Fonction pour afficher les fichiers ouverts
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Fonction : listOpenFiles
 * Description : Envoie au client la liste des fichiers ouverts par le processus serveur.
 *               Utilise la commande `ls -l /proc/<pid>/fd` pour récupérer les informations
 *               sur les descripteurs de fichiers ouverts.
 * Entrée :
 *   - `client_socket` : Descripteur de fichier du socket client pour envoyer les résultats.
 */

void listOpenFiles(int client_socket) {

/*
 * Étape 1 : Création de la commande shell pour lister les fichiers ouverts
 *
 * 1. `snprintf(command, sizeof(command), "ls -l /proc/%d/fd", getpid())` :
 *    - Construit une chaîne de commande qui utilise `ls -l` pour lister les fichiers ouverts.
 *    - `/proc/<pid>/fd` est le répertoire contenant les descripteurs de fichiers ouverts pour le processus.
 *    - `getpid()` retourne l'ID du processus actuel.
 *
 *    Exemple de commande générée :
 *    - `ls -l /proc/1234/fd` (où 1234 est l'ID du processus serveur).
 */

    char command[256];
    char buffer[512];
    FILE* fp;

    // Crée la commande pour afficher les fichiers ouverts du processus
    snprintf(command, sizeof(command), "ls -l /proc/%d/fd", getpid());

/*
 * Étape 2 : Exécution de la commande shell
 *
 * 1. `popen(command, "r")` :
 *    - Exécute la commande shell générée et ouvre un flux en lecture (`"r"`) pour capturer sa sortie.
 *    - Retourne un pointeur vers un fichier (`FILE*`).
 *
 * 2. Gestion des erreurs :
 *    - Si `popen()` retourne `NULL`, cela signifie que l'exécution de la commande a échoué.
 *    - Affiche un message d'erreur avec `perror()` et envoie un message au client.
 */

    // Exécute la commande et récupère la sortie
    fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen failed");
        write(client_socket, "Error listing open files.\n", 26);
        return;
    }

/*
 * Étape 3 : Lecture de la sortie de la commande
 *
 * 1. `fgets(buffer, sizeof(buffer), fp)` :
 *    - Lit une ligne de la sortie de la commande exécutée.
 *    - Stocke la ligne lue dans le tampon `buffer`.
 *
 * 2. Envoi de la ligne au client :
 *    - `write(client_socket, buffer, strlen(buffer))` :
 *        - Envoie la ligne lue via le socket client.
 *        - `strlen(buffer)` garantit que seules les données valides sont envoyées.
 *
 * 3. Répétition :
 *    - Continue à lire et envoyer les lignes jusqu'à ce que la fin du flux soit atteinte (`fgets()` retourne NULL).
 */

    // Lit la sortie de la commande et l'envoie au client
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        write(client_socket, buffer, strlen(buffer));
    }

/*
 * Étape 4 : Fermeture du flux
 *
 * 1. `pclose(fp)` :
 *    - Ferme le flux ouvert par `popen()` et libère les ressources associées.
 *    - Cela termine également la commande exécutée si elle est encore en cours.
 */

    // Ferme le flux
    pclose(fp);
}

/*
Intégration dans le serveur
Ajoutez cette fonction à votre serveur pour répondre à une commande spécifique (lsfd par exemple) :
*/

/*
 * Fonction : handleClient
 * Description : Fonction exécutée par un thread pour gérer les interactions avec un client connecté.
 *               Elle reçoit les commandes du client, traite les requêtes spécifiques et envoie des réponses.
 * Entrée :
 *   - `client_socket` : Pointeur vers un entier contenant le descripteur de socket du client.
 * Sortie :
 *   - Retourne NULL après la déconnexion ou l'erreur du client.
 */

void* handleClient(void* client_socket) {

/*
 * Étape 1 : Initialisation du socket client
 *
 * 1. `int sock = *(int*)client_socket` :
 *    - Dé-référence le pointeur `client_socket` pour récupérer le descripteur de fichier du socket client.
 *
 * 2. `free(client_socket)` :
 *    - Libère la mémoire allouée dynamiquement pour le socket dans le thread parent.
 */

    int sock = *(int*)client_socket;
    free(client_socket);

/*
 * Étape 2 : Déclaration du tampon pour la lecture des commandes
 *
 * 1. `char buffer[BUFFER_SIZE]` :
 *    - Tampon utilisé pour stocker les commandes reçues du client.
 */

    char buffer[BUFFER_SIZE];

/*
 * Étape 3 : Boucle principale de traitement des commandes
 *
 * 1. `while (1)` :
 *    - Maintient une boucle infinie pour écouter les commandes du client.
 *    - La boucle se termine si le client envoie "quit" ou si la connexion est interrompue.
 */

    while (1) {

    /*
     * Étape 4 : Lecture de la commande du client
     *
     * 1. `memset(buffer, 0, BUFFER_SIZE)` :
     *    - Initialise le tampon à zéro pour éviter les résidus de données précédentes.
     *
     * 2. `read(sock, buffer, BUFFER_SIZE - 1)` :
     *    - Lit les données envoyées par le client depuis le socket.
     *    - Limite la lecture à `BUFFER_SIZE - 1` pour laisser de la place pour le caractère de fin de chaîne.
     *
     * 3. Gestion de la déconnexion :
     *    - Si `read()` retourne une valeur ≤ 0, cela signifie que le client a fermé la connexion ou qu'une erreur est survenue.
     *    - Ferme le socket avec `close(sock)` et termine le thread.
     */

        memset(buffer, 0, BUFFER_SIZE);

        // Lire la commande du client
        int bytes_read = read(sock, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            printf("Client disconnected.\n");
            close(sock);
            return NULL;
        }

        buffer[bytes_read] = '\0';
        printf("Command received: %s\n", buffer);

    /*
     * Étape 5 : Traitement des commandes
     *
     * 1. `strncmp(buffer, "lsfd", 4)` :
     *    - Si la commande est "lsfd", appelle la fonction `listOpenFiles(sock)` pour envoyer au client
     *      la liste des fichiers ouverts par le processus serveur.
     *
     * 2. `strncmp(buffer, "quit", 4)` :
     *    - Si la commande est "quit", affiche un message indiquant que le client a demandé une déconnexion.
     *    - Ferme le socket avec `close(sock)` et termine proprement le thread.
     *
     * 3. Commande inconnue :
     *    - Si la commande n'est ni "lsfd" ni "quit", envoie un message "Unknown command" au client.
     */

        // Traiter les commandes
        if (strncmp(buffer, "lsfd", 4) == 0) {
            listOpenFiles(sock);
        } else if (strncmp(buffer, "quit", 4) == 0) {
            printf("Client requested disconnection.\n");
            close(sock);
            return NULL;
        } else {
            write(sock, "Unknown command\n", 16);
        }
    }
}

/*
Étape 4 : Exemple de sortie de ls -l /proc/<pid>/fd
Voici un exemple de ce que peut afficher la commande ls -l /proc/<pid>/fd pour un processus serveur :
*/
/*
total 0
lrwx------ 1 user user 64 Dec 16 15:45 0 -> /dev/pts/0
lrwx------ 1 user user 64 Dec 16 15:45 1 -> /dev/pts/0
lrwx------ 1 user user 64 Dec 16 15:45 2 -> /dev/pts/0
lrwx------ 1 user user 64 Dec 16 15:45 3 -> socket:[12456]
lrwx------ 1 user user 64 Dec 16 15:45 4 -> socket:[12457]
lrwx------ 1 user user 64 Dec 16 15:45 5 -> anon_inode:[eventpoll]
lr-x------ 1 user user 64 Dec 16 15:45 6 -> /var/log/server.log
*/

/*
Explication des champs :
Descripteurs de fichiers (colonne de gauche) :

0, 1, 2 : Représentent respectivement l'entrée standard (stdin), la sortie standard (stdout), et la sortie d'erreur (stderr).
3, 4, etc. : Sockets ou autres fichiers ouverts par le processus.
Type de fichier (colonne de droite) :

socket:[12456] : Indique qu'il s'agit d'un socket (utilisé pour la communication réseau).
/var/log/server.log : Exemple d'un fichier journal ouvert par le serveur.
*/

/*
Étape 5 : Explication dans un terminal
Lorsque cette commande est exécutée dans un terminal ou envoyée au client, vous pouvez expliquer :

Les sockets : représentent les connexions ouvertes avec les clients.
Les fichiers ouverts : peuvent inclure des fichiers de configuration, journaux, ou ressources spécifiques au serveur.
Les descripteurs 0, 1, 2 : sont standards et correspondent respectivement à stdin, stdout, et stderr.

Cette approche permet d’intégrer l’affichage et l’explication de la table des fichiers ouverts directement dans votre programme serveur. 
*/


/*
4)  Pouvoir comprendre comment est générée la table des fichiers ouverts du processus main
(thread main) du serveur du #1 ou TP3 en ajoutant une commande F ou f à votre VMS
(voir fonction listFileTable()), cette commande F ajoutée dans la boucle de traitement des transactions
(fonction readTrans()) permettra d’appeler un thread qui effectuera l’affichage de la liste
de fichiers ouverts du thread main. L’appel système system() pourrait permettre de faire
l’affichage de cette liste  de fichiers ouverts en passant comme argument la même chaîne
de caractères utilisée pour faire cet affichage en ligne de commande.
L’affichage de cette table de fichiers ouverts est fait dans la fenêtre terminal du client qui a fait la requête F.
*/

/*
Étape 1 : Ajout de la commande F dans la gestion des transactions
Modifiez la fonction readTrans() pour inclure une condition pour la commande F :

Si le client envoie la commande F, le serveur doit exécuter un thread dédié pour afficher la table des fichiers ouverts du processus principal.
Utilisez la fonction system() pour exécuter la commande Linux ls -l /proc/<pid>/fd et rediriger la sortie vers la fenêtre du client.
Créez un thread associé à l'affichage :

Le thread dédié exécute la commande ls -l /proc/<pid>/fd pour le PID du processus principal.
Envoyez la sortie via le socket associé au client.
*/

/*
Étape 2 : Exemple de code modifié
1. Modification dans readTrans()
Ajoutez un cas pour gérer la commande F :
*/

/*
 * Fonction : readTrans
 * Description : Traite les transactions (commandes) reçues par le serveur depuis un client.
 *               Si la commande est "F" ou "f", un thread indépendant est créé pour gérer
 *               l'affichage de la table des fichiers ouverts.
 * Entrée :
 *   - `client_socket` : Descripteur de fichier du socket client.
 *   - `transaction`   : Commande reçue du client sous forme de chaîne de caractères.
 */

void readTrans(int client_socket, const char* transaction) {

/*
 * Étape 1 : Vérification de la commande "F" ou "f"
 *
 * 1. `strncmp(transaction, "F", 1)` ou `strncmp(transaction, "f", 1)` :
 *    - Vérifie si la commande commence par "F" (majuscule) ou "f" (minuscule).
 *    - Si c'est le cas, la commande est interprétée comme une demande pour afficher
 *      la table des fichiers ouverts du processus serveur.
 */

    if (strncmp(transaction, "F", 1) == 0 || strncmp(transaction, "f", 1) == 0) {

    /*
     * Étape 2 : Création d'un thread pour la gestion de la commande "F"
     *
     * 1. `pthread_t list_files_thread` :
     *    - Déclare une variable pour le thread qui sera chargé d'exécuter la fonction `listFileTable`.
     *
     * 2. Allocation mémoire pour le descripteur du socket client :
     *    - `malloc(sizeof(int))` :
     *        - Alloue dynamiquement de la mémoire pour stocker le descripteur du socket client.
     *        - Cette allocation est nécessaire pour transmettre le socket au thread de manière sécurisée.
     *
     * 3. Initialisation de la mémoire :
     *    - `*client_sock_ptr = client_socket` :
     *        - Copie la valeur du descripteur `client_socket` dans la mémoire allouée.
     *
     * 4. Création du thread :
     *    - `pthread_create(&list_files_thread, NULL, listFileTable, client_sock_ptr)` :
     *        - Crée un thread qui exécute la fonction `listFileTable`.
     *        - Passe l'adresse du socket client (`client_sock_ptr`) comme argument au thread.
     *
     * 5. Détachement du thread :
     *    - `pthread_detach(list_files_thread)` :
     *        - Permet au thread de s'exécuter de manière indépendante.
     *        - Les ressources du thread seront automatiquement libérées à la fin de son exécution.
     */

        pthread_t list_files_thread;

        // Créer un thread pour gérer la commande F
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_socket;
        pthread_create(&list_files_thread, NULL, listFileTable, client_sock_ptr);
        pthread_detach(list_files_thread); // Laisser le thread s'exécuter de manière indépendante
    } else {

 /*
     * Étape 3 : Gestion des commandes non reconnues
     *
     * 1. `printf("Unhandled transaction: %s\n", transaction)` :
     *    - Affiche un message indiquant que la commande reçue n'est pas prise en charge.
     */

        // Gérer les autres commandes
        printf("Unhandled transaction: %s\n", transaction);
    }
}

/*
2. Implémentation de la fonction listFileTable()
Cette fonction sera exécutée par le thread pour afficher la table des fichiers ouverts et transmettre les résultats au client.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 * Fonction : listFileTable
 * Description : Fonction exécutée par un thread pour envoyer au client la table des fichiers ouverts
 *               par le processus serveur. Utilise la commande shell `ls -l /proc/<pid>/fd` pour récupérer
 *               ces informations.
 * Entrée :
 *   - `client_socket_ptr` : Pointeur vers un entier contenant le descripteur du socket client.
 * Sortie :
 *   - Retourne NULL après l'envoi complet des informations.
 */

void* listFileTable(void* client_socket_ptr) {

/*
 * Étape 1 : Récupération et libération du descripteur de socket
 *
 * 1. `int client_socket = *(int*)client_socket_ptr` :
 *    - Dé-référence le pointeur pour récupérer le descripteur de socket du client.
 *
 * 2. `free(client_socket_ptr)` :
 *    - Libère la mémoire allouée dynamiquement pour le descripteur de socket.
 *    - Cette allocation a été faite dans la fonction `readTrans` pour passer le socket au thread.
 */

    int client_socket = *(int*)client_socket_ptr;
    free(client_socket_ptr);

/*
 * Étape 2 : Préparation de la commande pour lister les fichiers ouverts
 *
 * 1. `snprintf(command, sizeof(command), "ls -l /proc/%d/fd", getpid())` :
 *    - Construit la commande shell pour afficher les descripteurs de fichiers ouverts du processus.
 *    - `/proc/<pid>/fd` est le répertoire système où sont stockés les descripteurs de fichiers.
 *    - `getpid()` retourne l'ID du processus serveur.
 *
 *    Exemple de commande générée :
 *      ls -l /proc/1234/fd (où 1234 est l'ID du processus).
 */

    char command[256];
    char buffer[512];
    FILE* fp;

    // Générer la commande pour afficher les fichiers ouverts
    snprintf(command, sizeof(command), "ls -l /proc/%d/fd", getpid());

/*
 * Étape 3 : Exécution de la commande shell
 *
 * 1. `popen(command, "r")` :
 *    - Exécute la commande et ouvre un flux en lecture (`"r"`) pour capturer la sortie.
 *    - Retourne un pointeur vers un fichier (`FILE*`) pour lire la sortie ligne par ligne.
 *
 * 2. Gestion des erreurs :
 *    - Si `popen()` retourne NULL, cela signifie que la commande n'a pas pu être exécutée.
 *    - Envoie un message d'erreur au client via `write()`.
 *    - Termine la fonction en retournant NULL.
 */

    // Exécuter la commande et capturer la sortie
    fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen failed");
        write(client_socket, "Error listing file table.\n", 26);
        return NULL;
    }

/*
 * Étape 4 : Lecture et envoi de la sortie au client
 *
 * 1. `fgets(buffer, sizeof(buffer), fp)` :
 *    - Lit la sortie de la commande ligne par ligne et stocke chaque ligne dans le tampon `buffer`.
 *
 * 2. `write(client_socket, buffer, strlen(buffer))` :
 *    - Envoie chaque ligne lue au client via le socket.
 *    - La fonction `strlen()` est utilisée pour s'assurer que seules les données valides sont envoyées.
 *
 * 3. Répète jusqu'à ce que la fin du flux soit atteinte (lorsque `fgets()` retourne NULL).
 */

    // Lire la sortie de la commande et l'envoyer au client
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        write(client_socket, buffer, strlen(buffer));  // Envoi des lignes une par une
    }

/*
 * Étape 5 : Fermeture du flux
 *
 * 1. `pclose(fp)` :
 *    - Ferme le flux ouvert par `popen()` et libère les ressources associées.
 *    - Cela garantit également que le processus enfant exécutant la commande est terminé.
 */

    // Fermer le flux
    pclose(fp);

/*
 * Étape 6 : Fin de la fonction
 *
 * 1. La fonction retourne NULL pour terminer proprement le thread.
 */

    return NULL;
}

/*
Étape 3 : Intégration avec le client
Le client envoie simplement la commande F ou f pour demander l'affichage de la table des fichiers ouverts.

Exemple de code côté client (modification du programme client) :
*/

/*
 * Fonction : handleConnection
 * Description : Gère l'interaction entre le client et le serveur. Permet à l'utilisateur d'envoyer des commandes 
 *               et affiche les réponses du serveur.
 * Entrée :
 *   - `server_socket` : Descripteur de fichier du socket connecté au serveur.
 */

void handleConnection(int server_socket) {

/*
 * Étape 1 : Déclaration des tampons
 *
 * 1. `char command[BUFFER_SIZE]` :
 *    - Tampon pour stocker les commandes saisies par l'utilisateur.
 *
 * 2. `char response[BUFFER_SIZE]` :
 *    - Tampon pour stocker les réponses reçues du serveur.
 */

    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

/*
 * Étape 2 : Boucle principale pour l'interaction avec le serveur
 *
 * 1. `while (1)` :
 *    - Boucle infinie permettant à l'utilisateur d'envoyer plusieurs commandes successivement.
 *    - La boucle se termine si l'utilisateur envoie "quit" ou en cas de problème de connexion.
 */

    while (1) {

    /*
     * Étape 3 : Lecture de la commande utilisateur
     *
     * 1. `printf(...)` :
     *    - Affiche une invite demandant à l'utilisateur d'entrer une commande.
     *
     * 2. `memset(command, 0, BUFFER_SIZE)` :
     *    - Initialise le tampon `command` pour s'assurer qu'il est vide avant la saisie.
     *
     * 3. `fgets(command, BUFFER_SIZE, stdin)` :
     *    - Lit une ligne de commande saisie par l'utilisateur depuis l'entrée standard.
     *
     * 4. Suppression du saut de ligne :
     *    - Si la commande se termine par un caractère `\n`, remplacez-le par `\0` pour une chaîne propre.
     */

        printf("Enter a command (F for file table, quit to disconnect): ");
        memset(command, 0, BUFFER_SIZE);
        fgets(command, BUFFER_SIZE, stdin);

        // Retirer le caractère de saut de ligne
        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

    /*
     * Étape 4 : Envoi de la commande au serveur
     *
     * 1. `write(server_socket, command, strlen(command))` :
     *    - Envoie la commande saisie par l'utilisateur au serveur via le socket.
     *
     * 2. Gestion des erreurs :
     *    - Si `write()` retourne `-1`, affiche un message d'erreur avec `perror()` et termine la boucle.
     */

        // Envoyer la commande au serveur
        if (write(server_socket, command, strlen(command)) == -1) {
            perror("Failed to send command");
            break;
        }

    /*
     * Étape 5 : Déconnexion
     *
     * 1. Si la commande est "quit", afficher un message de déconnexion et sortir de la boucle.
     */

        // Si la commande est "quit", sortir de la boucle
        if (strncmp(command, "quit", 4) == 0) {
            printf("Disconnecting from server...\n");
            break;
        }

    /*
     * Étape 6 : Lecture de la réponse du serveur
     *
     * 1. `printf("Response from server:\n")` :
     *    - Affiche un message pour signaler que la réponse du serveur sera affichée.
     *
     * 2. Boucle pour lire la réponse complète :
     *    - Continue de lire des données du serveur jusqu'à ce qu'il n'y ait plus rien à lire pour cette commande.
     */

        // Lire la réponse du serveur
        printf("Response from server:\n");
        while (1) {

        /*
         * a. `memset(response, 0, BUFFER_SIZE)` :
         *    - Initialise le tampon `response` pour s'assurer qu'il est vide avant la réception.
         *
         * b. `read(server_socket, response, BUFFER_SIZE - 1)` :
         *    - Lit une portion de la réponse envoyée par le serveur.
         *    - Limite la lecture à `BUFFER_SIZE - 1` pour laisser de la place pour le caractère de fin de chaîne `\0`.
         *
         * c. Gestion des erreurs ou de la fermeture de connexion :
         *    - Si `read()` retourne une valeur ≤ 0, cela signifie que la connexion a été fermée par le serveur ou qu'une erreur est survenue.
         *    - Affiche un message et termine la fonction.
         */

            memset(response, 0, BUFFER_SIZE);
            ssize_t bytes_read = read(server_socket, response, BUFFER_SIZE - 1);
            if (bytes_read <= 0) {
                printf("Connection closed by server.\n");
                return;
            }

        /*
         * d. Affichage de la réponse :
         *    - Affiche la réponse reçue depuis le serveur sur le terminal client.
         */

            printf("%s", response);  // Afficher la réponse reçue

        /*
         * e. Fin de la réponse :
         *    - Si le nombre d'octets lus est inférieur à la taille maximale du tampon, cela indique que la réponse est complète.
         *    - Sortir de la boucle de lecture.
         */

            // Si la réponse est complète, sortir de la boucle
            if (bytes_read < BUFFER_SIZE - 1) {
                break;
            }
        }
    }
}

/*
Étape 4 : Exemple d'exécution

Serveur :
$ gcc -o server server.c -lpthread
$ ./server
Server is listening on port 1234...
Client connected: 127.0.0.1

Client :
$ gcc -o client client.c
$ ./client 127.0.0.1
Connected to server 127.0.0.1 on port 1234
Enter a command (F for file table, quit to disconnect): F
Response from server:
total 0
lrwx------ 1 user user 64 Dec 16 15:45 0 -> /dev/pts/0
lrwx------ 1 user user 64 Dec 16 15:45 1 -> /dev/pts/0
lrwx------ 1 user user 64 Dec 16 15:45 2 -> /dev/pts/0
lrwx------ 1 user user 64 Dec 16 15:45 3 -> socket:[12456]
lrwx------ 1 user user 64 Dec 16 15:45 4 -> socket:[12457]
Enter a command (F for file table, quit to disconnect): quit
Disconnecting from server...


Explications :
Serveur :

La commande F déclenche la création d’un thread pour exécuter listFileTable().
Cette fonction utilise system() via popen() pour récupérer la table des fichiers ouverts du processus principal.
Client :

Envoie la commande F au serveur.
Reçoit et affiche les résultats ligne par ligne dans le terminal.
Sortie :

Le client voit la liste des fichiers ouverts (stdin, stdout, stderr, sockets, etc.) du processus serveur.
Cette fonctionnalité ajoute une dimension utile pour déboguer ou surveiller les ressources utilisées par le serveur.
*/




/*
5)  Pouvoir comprendre et expliquer l’affichage des informations sur les zones de mémoire
d’un processus ou d’un thread généré par la commande LINUX : cat /proc/<pid>/maps associé à
la liste des régions de mémoire du serveur (thread MAIN). 
Pouvoir comprendre comment est généré le mapping des régions de mémoire,
comme précédemment en ligne de commande,
du processus main (thread main) du serveur du TP3 MAIS en ajoutant une commande M à votre VMS (voir fonction listMemoryMap()),
cette commande M ajoutée dans la boucle de traitement des transactions (fonction readTrans())
permettra d’appeler un thread qui effectuera l’affichage des informations des régions de mémoire  du thread main.
L’appel système system() pourrait permettre de faire l’affichage de cette liste
d’information des régions de mémoire en passant comme argument la chaîne de caractères utilisée
pour faire cet affichage en ligne de commande. L’affichage de ce mapping mémoire
est fait dans la fenêtre terminal du client qui a fait la requête M.

Étape 1 : Commande M et Fonctionnalité
Commande M dans la boucle de traitement (readTrans) :

Ajoutez un cas pour détecter la commande M.
Créez un thread dédié pour exécuter la commande cat /proc/<pid>/maps.
Fonction pour afficher le mapping mémoire (listMemoryMap) :

Utilisez system() ou popen() pour exécuter la commande cat /proc/<pid>/maps.
Transmettez la sortie au client via le socket associé.
Exécution dans un thread :

Créez un thread pour gérer l'affichage sans bloquer le traitement des autres requêtes.
*/

/*
Étape 2 : Exemple de Code
Modification dans readTrans()
Ajoutez un cas pour gérer la commande M :
*/

/*
 * Fonction : readTrans
 * Description : Traite les transactions (commandes) reçues du client. Si la commande est "M" ou "m",
 *               un thread est créé pour gérer l'affichage du mapping des régions de mémoire du processus serveur.
 * Entrée :
 *   - `client_socket` : Descripteur de fichier du socket client.
 *   - `transaction`   : Commande reçue du client sous forme de chaîne de caractères.
 */

void readTrans(int client_socket, const char* transaction) {

/*
 * Étape 1 : Vérification de la commande "M" ou "m"
 *
 * 1. `strncmp(transaction, "M", 1)` ou `strncmp(transaction, "m", 1)` :
 *    - Vérifie si la commande commence par "M" (majuscule) ou "m" (minuscule).
 *    - Si c'est le cas, la commande est interprétée comme une demande d'affichage
 *      du mapping des régions de mémoire du processus serveur.
 */

    if (strncmp(transaction, "M", 1) == 0 || strncmp(transaction, "m", 1) == 0) {

    /*
     * Étape 2 : Création d'un thread pour la gestion de la commande "M"
     *
     * 1. `pthread_t memory_map_thread` :
     *    - Déclare une variable pour le thread qui sera chargé d'exécuter la fonction `listMemoryMap`.
     *
     * 2. Allocation mémoire pour le descripteur du socket client :
     *    - `malloc(sizeof(int))` :
     *        - Alloue dynamiquement de la mémoire pour stocker le descripteur du socket client.
     *        - Cette allocation est nécessaire pour transmettre le socket au thread de manière sécurisée.
     *
     * 3. Initialisation de la mémoire :
     *    - `*client_sock_ptr = client_socket` :
     *        - Copie la valeur du descripteur `client_socket` dans la mémoire allouée.
     *
     * 4. Création du thread :
     *    - `pthread_create(&memory_map_thread, NULL, listMemoryMap, client_sock_ptr)` :
     *        - Crée un thread qui exécute la fonction `listMemoryMap`.
     *        - Passe l'adresse du socket client (`client_sock_ptr`) comme argument au thread.
     *
     * 5. Détachement du thread :
     *    - `pthread_detach(memory_map_thread)` :
     *        - Permet au thread de s'exécuter de manière indépendante.
     *        - Les ressources du thread seront automatiquement libérées à la fin de son exécution.
     */


        pthread_t memory_map_thread;

        // Créer un thread pour gérer la commande M
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_socket;
        pthread_create(&memory_map_thread, NULL, listMemoryMap, client_sock_ptr);
        pthread_detach(memory_map_thread); // Laisser le thread s'exécuter de manière indépendante
    } else {

    /*
     * Étape 3 : Gestion des commandes non reconnues
     *
     * 1. `printf("Unhandled transaction: %s\n", transaction)` :
     *    - Affiche un message indiquant que la commande reçue n'est pas prise en charge.
     */

        // Gérer les autres commandes
        printf("Unhandled transaction: %s\n", transaction);
    }
}
/*
Implémentation de la Fonction listMemoryMap()
Cette fonction utilise popen() pour exécuter la commande cat /proc/<pid>/maps et transmet les résultats au client.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 * Fonction : listMemoryMap
 * Description : Fonction exécutée par un thread pour envoyer au client le mapping des régions de mémoire
 *               utilisées par le processus serveur. Utilise la commande shell `cat /proc/<pid>/maps`.
 * Entrée :
 *   - `client_socket_ptr` : Pointeur vers un entier contenant le descripteur de socket du client.
 * Sortie :
 *   - Retourne NULL après l'envoi complet des informations.
 */

void* listMemoryMap(void* client_socket_ptr) {

/*
 * Étape 1 : Récupération et libération du descripteur de socket
 *
 * 1. `int client_socket = *(int*)client_socket_ptr` :
 *    - Dé-référence le pointeur pour récupérer le descripteur de socket du client.
 *
 * 2. `free(client_socket_ptr)` :
 *    - Libère la mémoire allouée dynamiquement pour le descripteur de socket.
 *    - Cette allocation a été faite dans la fonction `readTrans` pour passer le socket au thread.
 */

    int client_socket = *(int*)client_socket_ptr;
    free(client_socket_ptr);

/*
 * Étape 2 : Préparation de la commande pour afficher le mapping mémoire
 *
 * 1. `snprintf(command, sizeof(command), "cat /proc/%d/maps", getpid())` :
 *    - Construit la commande shell pour afficher le mapping mémoire du processus.
 *    - `/proc/<pid>/maps` est le fichier système contenant les informations de mapping mémoire.
 *    - `getpid()` retourne l'ID du processus serveur.
 *
 *    Exemple de commande générée :
 *      cat /proc/1234/maps (où 1234 est l'ID du processus).
 */

    char command[256];
    char buffer[512];
    FILE* fp;

/*
 * Étape 3 : Exécution de la commande shell
 *
 * 1. `popen(command, "r")` :
 *    - Exécute la commande et ouvre un flux en lecture (`"r"`) pour capturer la sortie.
 *    - Retourne un pointeur vers un fichier (`FILE*`) pour lire la sortie ligne par ligne.
 *
 * 2. Gestion des erreurs :
 *    - Si `popen()` retourne NULL, cela signifie que la commande n'a pas pu être exécutée.
 *    - Envoie un message d'erreur au client via `write()`.
 *    - Termine la fonction en retournant NULL.
 */

    // Générer la commande pour afficher le mapping mémoire
    snprintf(command, sizeof(command), "cat /proc/%d/maps", getpid());

    // Exécuter la commande et capturer la sortie
    fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen failed");
        write(client_socket, "Error listing memory map.\n", 26);
        return NULL;
    }

/*
 * Étape 4 : Lecture et envoi de la sortie au client
 *
 * 1. `fgets(buffer, sizeof(buffer), fp)` :
 *    - Lit la sortie de la commande ligne par ligne et stocke chaque ligne dans le tampon `buffer`.
 *
 * 2. `write(client_socket, buffer, strlen(buffer))` :
 *    - Envoie chaque ligne lue via le socket client.
 *    - La fonction `strlen()` est utilisée pour s'assurer que seules les données valides sont envoyées.
 *
 * 3. Répète jusqu'à ce que la fin du flux soit atteinte (lorsque `fgets()` retourne NULL).
 */

    // Lire la sortie de la commande et l'envoyer au client
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        write(client_socket, buffer, strlen(buffer));  // Envoi des lignes une par une
    }

/*
 * Étape 5 : Fermeture du flux
 *
 * 1. `pclose(fp)` :
 *    - Ferme le flux ouvert par `popen()` et libère les ressources associées.
 *    - Cela garantit également que le processus enfant exécutant la commande est terminé.
 */

    // Fermer le flux
    pclose(fp);

/*
 * Étape 6 : Fin de la fonction
 *
 * 1. La fonction retourne NULL pour terminer proprement le thread.
 */

    return NULL;
}

/*
Étape 3 : Intégration dans le Client
Le client doit envoyer la commande M pour demander l’affichage des informations des régions de mémoire.

Exemple de code pour gérer cela côté client :
*/

/*
 * Fonction : handleConnection
 * Description : Gère l'interaction entre le client et le serveur. Permet à l'utilisateur d'envoyer des commandes
 *               au serveur et affiche les réponses reçues.
 * Entrée :
 *   - `server_socket` : Descripteur de fichier du socket connecté au serveur.
 */

void handleConnection(int server_socket) {

/*
 * Étape 1 : Déclaration des tampons
 *
 * 1. `char command[BUFFER_SIZE]` :
 *    - Tampon pour stocker les commandes saisies par l'utilisateur.
 *
 * 2. `char response[BUFFER_SIZE]` :
 *    - Tampon pour stocker les réponses reçues du serveur.
 */

    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

/*
 * Étape 2 : Boucle principale pour l'interaction avec le serveur
 *
 * 1. `while (1)` :
 *    - Maintient une boucle infinie permettant à l'utilisateur d'envoyer plusieurs commandes au serveur.
 *    - La boucle se termine si l'utilisateur envoie "quit" ou si la connexion est interrompue.
 */

    while (1) {

    /*
     * Étape 3 : Lecture de la commande utilisateur
     *
     * 1. `printf(...)` :
     *    - Affiche une invite demandant à l'utilisateur d'entrer une commande.
     *
     * 2. `memset(command, 0, BUFFER_SIZE)` :
     *    - Initialise le tampon `command` pour s'assurer qu'il est vide avant la saisie.
     *
     * 3. `fgets(command, BUFFER_SIZE, stdin)` :
     *    - Lit une ligne de commande saisie par l'utilisateur depuis l'entrée standard.
     *
     * 4. Suppression du saut de ligne :
     *    - Si la commande se termine par un caractère `\n`, remplacez-le par `\0` pour une chaîne propre.
     */

        printf("Enter a command (M for memory map, quit to disconnect): ");
        memset(command, 0, BUFFER_SIZE);
        fgets(command, BUFFER_SIZE, stdin);

        // Retirer le caractère de saut de ligne
        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

    /*
     * Étape 4 : Envoi de la commande au serveur
     *
     * 1. `write(server_socket, command, strlen(command))` :
     *    - Envoie la commande saisie par l'utilisateur au serveur via le socket.
     *
     * 2. Gestion des erreurs :
     *    - Si `write()` retourne `-1`, cela signifie que l'envoi a échoué.
     *    - Affiche un message d'erreur avec `perror()` et termine la boucle.
     */

        // Envoyer la commande au serveur
        if (write(server_socket, command, strlen(command)) == -1) {
            perror("Failed to send command");
            break;
        }

    /*
     * Étape 5 : Déconnexion
     *
     * 1. Si la commande est "quit", afficher un message de déconnexion et sortir de la boucle.
     */

        // Si la commande est "quit", sortir de la boucle
        if (strncmp(command, "quit", 4) == 0) {
            printf("Disconnecting from server...\n");
            break;
        }

    /*
     * Étape 6 : Lecture de la réponse du serveur
     *
     * 1. `printf("Response from server:\n")` :
     *    - Affiche un message indiquant que la réponse du serveur va être affichée.
     *
     * 2. Boucle pour lire la réponse complète :
     *    - Continue de lire des données du serveur jusqu'à ce qu'il n'y ait plus rien à lire pour cette commande.
     */

        // Lire la réponse du serveur
        printf("Response from server:\n");
        while (1) {

        /*
         * a. `memset(response, 0, BUFFER_SIZE)` :
         *    - Initialise le tampon `response` pour s'assurer qu'il est vide avant la réception.
         *
         * b. `read(server_socket, response, BUFFER_SIZE - 1)` :
         *    - Lit une portion de la réponse envoyée par le serveur.
         *    - Limite la lecture à `BUFFER_SIZE - 1` pour laisser de la place pour le caractère de fin de chaîne `\0`.
         *
         * c. Gestion des erreurs ou de la fermeture de connexion :
         *    - Si `read()` retourne une valeur ≤ 0, cela signifie que la connexion a été fermée par le serveur ou qu'une erreur est survenue.
         *    - Affiche un message et termine la fonction.
         */

            memset(response, 0, BUFFER_SIZE);
            ssize_t bytes_read = read(server_socket, response, BUFFER_SIZE - 1);
            if (bytes_read <= 0) {
                printf("Connection closed by server.\n");
                return;
            }

        /*
         * d. Affichage de la réponse :
         *    - Affiche la réponse reçue depuis le serveur sur le terminal client.
         */

            printf("%s", response);  // Afficher la réponse reçue

        /*
         * e. Fin de la réponse :
         *    - Si le nombre d'octets lus est inférieur à la taille maximale du tampon, cela indique que la réponse est complète.
         *    - Sortir de la boucle de lecture.
         */

            // Si la réponse est complète, sortir de la boucle
            if (bytes_read < BUFFER_SIZE - 1) {
                break;
            }
        }
    }
}

/*
Étape 4 : Exemple d’Exécution
Serveur :

$ gcc -o server server.c -lpthread
$ ./server
Server is listening on port 1234...
Client connected: 127.0.0.1

Client :
$ gcc -o client client.c
$ ./client 127.0.0.1
Connected to server
*/