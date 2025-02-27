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
/* Fonction : Fonction principale qui est démarré par l'exécutable
 * Entrée : Paramètres de ligne de commandes optionnels
 * Sortie : Un int non significatif dans la forme présente
*/
int main(int argc, char* argv[]){
    initscr();

    pthread_t read_input_thread; // Création des files
    pthread_t read_output_thread;

    WINDOW* inputWindowPtr = setupTerminalWindow(true, true, "TRANSMISSION", 0);  // Création des fenêtres
    WINDOW* outputWindowPtr = setupTerminalWindow(true, true, "RECEPTION", COLS/2);

    struct sockaddr_in address;
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0); // Création d'un socket IPV4 de type TCP avec le protocol IP

    address.sin_family = AF_INET; // Address family AF_INET= IPv4
    address.sin_addr.s_addr = inet_addr("127.0.0.1"); // Adresse de loopback
    address.sin_port = 1236; // numéro du port utilisé pour la communication

    int result = connect(socket_fd, (struct sockaddr*)&address, sizeof(address)); // On établit la connexion à l'adresse établie

    if(result == -1) { // erreur d'ouverture de socket 
        perror("Error opening file"); 
        pthread_exit(0); 
    }

    struct window_socket* inputArg = malloc(sizeof(struct window_socket)); // Création de structures pour contenir les commandes entrées
    struct window_socket* outputArg = malloc(sizeof(struct window_socket)); // Création de structures pour contenir les données à afficher

    inputArg->fd = socket_fd;  // le socket utilisé en entrée
    inputArg->address = address; // l'adresse du serveur
    inputArg->win = inputWindowPtr; // un lien vers la fenêtre d'entrée 

    outputArg->address = address; // l'adresse du client
    outputArg->fd = socket_fd; // le socket utilisé en sortie
    outputArg->win = outputWindowPtr; // un lien vers la fenêtre de sortie 

    pthread_create(&read_input_thread, NULL, &readInput, inputArg); // création de thread pour exécuter la lecture du fichier d'entrée
    pthread_create(&read_output_thread, NULL, &readOutput, outputArg);// création de thread pour exécuter l'affichage sur la fenêtre de sortie
    
    pthread_join(read_input_thread, NULL);
    pthread_join(read_output_thread, NULL);

    endwin(); 
    return 0;
}
