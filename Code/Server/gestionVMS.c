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
	pthread_t clients_tid[1000];
	int thread_count = 0;
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;

	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	//Entre les information du serveur dans la struct
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_address.sin_port = 1236;
	int res = bind(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address));

	if(res == -1) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	listen(socket_fd, 5); //Se met à l'écoute de tout ce qui passe sur le socket
	while(1) { //On lit dans une boucle sans fin les transactions reçus
		int client_socket_fd = accept(socket_fd, (struct sockaddr*)&client_address, (socklen_t*)&client_address.sin_port); // 
		struct paramReadTrans* ptr = malloc(sizeof(struct paramReadTrans)); // Réserve un espace mémoire pour stocké les paramètres de Read
		ptr->client_socket_fd = client_socket_fd;   // passe le socket
		//Crée un nouveau thread pour chaque nouveau client et appelle la commande readTrans avec la struct de contrôle
		pthread_create(&clients_tid[thread_count++], NULL, &readTrans, ptr);
	}
}

/* Fonction : fonction utilisée pour le traitement  des transactions
 * Entrée : rien
 * Sortie : un pointeur null
*/
void* readTrans(void* args){
	struct paramReadTrans* ptr = args;
	int client_socket_fd = ptr->client_socket_fd;
	free(ptr);

    pthread_t tid[1000];
    int thread_count = 0;
	char *sp;
	ssize_t read_res;
	do { // Boucle pour le traitement de la transaction
		struct Info_FIFO_Transaction* socketData = malloc(sizeof(struct Info_FIFO_Transaction));
		read_res = read(client_socket_fd, socketData, sizeof(*socketData));
		if(read_res == -1) {
			perror("Error");
			break;
		}
		if(read_res == 0) break;
		char *tok = strtok_r(socketData->transaction, " ", &sp);
		if(tok == NULL) { // Vérifie si c'est une ligne vide donc Null
			free(socketData);
			continue;
		}
		printf("%s\n", tok);
		switch(tok[0]){
			case 'E':
			case 'e':{
				if(!adminCheck(&admin_pid, socketData->client_pid)) break;
				//Extraction du paramètre
				int const noVM = strToPositiveInt(strtok_r(NULL, " ", &sp));
				if(noVM == -1) break;

				struct paramE* ptr = malloc(sizeof(struct paramE));

				ptr->client_socket_fd = client_socket_fd;
				ptr->noVM = noVM;

				//Appel de la fonction associée
				pthread_create(&tid[thread_count++], NULL, &removeItem, ptr);
				break;
			}
			case 'L':
			case 'l':{
				if(!adminCheck(&admin_pid, socketData->client_pid)) break;
				//Extraction des paramètres / verification des paramètres valides;
				int const start = strToPositiveInt(strtok_r(NULL, "-", &sp));
				if(start == -1) break;

				int const end = strToPositiveInt(strtok_r(NULL, " ", &sp));
				if(end == -1) break;

				struct paramL* ptr = malloc(sizeof(struct paramL));
				ptr->nStart = start;
				ptr->nEnd = end;
				ptr->client_socket_fd = client_socket_fd;

				pthread_create(&tid[thread_count++], NULL, &listItems, ptr);
				break;
			}
			case 'B': // affiche liste des fichiers .olc
			case 'b':{
				struct paramCommand* ptr = malloc(sizeof(struct paramCommand));
				ptr->client_socket_fd = client_socket_fd;
				strcpy(ptr->command, "ls -l *.olc3\n");

				pthread_create(&tid[thread_count++], NULL, &sendCommand, ptr);
				break;
			}
			case 'K':
			case 'k':{
				if(!adminCheck(&admin_pid, socketData->client_pid)) break;
				//Extraction du paramètre
				char* endptr = NULL;
				const char* thread_entered_ch = strtok_r(NULL, " ", &sp);
				if(thread_entered_ch == NULL) break;
				pthread_t const threadEntered = strtoul(thread_entered_ch, &endptr, 10);
				if(thread_entered_ch == endptr) break;

				struct paramK* ptr = malloc(sizeof(struct paramK));
				ptr->tid = threadEntered;
				//Appel de la fonction associée
				pthread_create(&tid[thread_count++], NULL, &killThread, ptr);
				break;
			}
			case 'P':
			case 'p': {
				if(!adminCheck(&admin_pid, socketData->client_pid)) break;
				struct paramCommand* ptr = malloc(sizeof(struct paramCommand));
				ptr->client_socket_fd = client_socket_fd;
				strcpy(ptr->command, "ps -aux\n");

				pthread_create(&tid[thread_count++], NULL, &sendCommand, ptr);
				break;
			}
			case 'X':
			case 'x':{
				//Appel de la fonction associée
				int const p = strToPositiveInt(strtok_r(NULL, " ", &sp));
				char *nomfich = strtok_r(NULL, "\n", &sp);

				if(p == -1 || nomfich == NULL) break;

				struct paramX* ptr = malloc(sizeof(struct paramX));
				ptr->boolPrint = p;
				strcpy(ptr->fileName, nomfich);
				ptr->client_socket_fd = client_socket_fd;
				pthread_create(&tid[thread_count++], NULL, &executeFile, ptr);
				break;
			}
			case 'Q':
			case 'q': {
				if(admin_pid == socketData->client_pid) {
					admin_pid = 0;
				}
				break;
			}
		}
		free(socketData);
	}while(read_res > 0);
	//Pour chacune des lignes lues
	for(int i = 0; i < thread_count; i++) {
		pthread_join(tid[i], NULL);
	}
	//Fermeture du fifo
	close(client_socket_fd);
	//Retour
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


