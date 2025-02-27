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
/* Fonction : Lecture des entrées à partir d'un socket
 * Entrée : void* args ou null
 * Sortie : un pointeur vers une struct nulle ou non
*/
void* readInput(void* args) {
    struct window_socket* ptr = args;
    WINDOW* input_window = ptr->win; // Si la fenêtre existe
    int socket_fd = ptr->fd; 
    struct sockaddr_in address = ptr->address;

    free(args);

    char* input = malloc(200*sizeof(char)); // réserve la mémoire pour le command line

    wprintw(input_window, "> "); // Met un pointeur de ligne
    int key = wgetch(input_window); // attend la touche

    while(key != ERR) { // Tant que l'entrée de la clé n'est pas une erreur
        if(key == 10) { // Enter key
            if(input[0] == 'q' || input[0] == 'Q') break; // est-ce qu'on sort, quit?
            sendInput(input, socket_fd); 

            memset(input, 0, strlen(input) * sizeof(char));  // met en mémoire 
            wprintw(input_window,"\n> "); // simule le retour de ligne à l'écran
        } else { // on gère l'entrée de la console
            const char* check = unctrl(key); // valide la clé

            if(key == 263) { //Backspace
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
        wrefresh(input_window); // affiche le contenu de la fenêtre
        key = wgetch(input_window); //attend le prochain car
    }
    sendInput(input, socket_fd); //envoi de la commande au serveur
    free(input); // libère input
    close(socket_fd); // ferme le socket serveur
    pthread_exit(0); // ferme le thread 
}

/* Fonction : Inscription de la commande dans le socket
 * Entrée : la commande et le socket vers le serveur
 * Sortie : rien
*/
void sendInput(const char *input,const int socket_fd) {
    struct Info_FIFO_Transaction* ptr = malloc(sizeof(struct Info_FIFO_Transaction));
    ptr->pid_client = getpid(); // Écrit le PID pour identifier l'envoyeur dans la struct
    strcpy(ptr->transaction, input); // inscrit la transaction dans la struct
    write(socket_fd, ptr, sizeof(*ptr)); // Écrit la struct dans le socket

    free(ptr); // libère la struct
}

/* Fonction : Lecture de la sortie
 * Entrée : Un pointeur vers la fenêtre active
 * Sortie : un pointeur vers une struct nulle ou non
*/
void* readOutput(void* args) {
    struct window_socket* ptr = args;
    WINDOW* win = ptr->win; // Si la fenêtre existe
    const int socket_fd = ptr->fd;
    struct sockaddr_in address = ptr->address;

    free(args);
    ssize_t read_res;
    do{ // Boucle 
        char output[200] = {0};
        read_res = read(socket_fd, output, sizeof(output));// Lecture du socket pour avoir la réponse du serveur
        if(read_res == -1) {
            perror("Error");
            break;
        }

        wprintw(win,"%s", output);// Sinon écrit dans la fenêtre
        wrefresh(win); // Affiche la fenêtre
    }while(read_res > 0); //  tant que la lecture du fichier retourne qqchose 
    pthread_exit(0); 
}
