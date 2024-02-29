#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#define TAILLE_BUFFER 1024
#define MAX_CLIENTS 10
#define LONGUEUR_NOM_UTILISATEUR 50
// Augmenter la taille pour accueillir la structure du message sans risque de débordement.
#define LONGUEUR_MESSAGE TAILLE_BUFFER + LONGUEUR_NOM_UTILISATEUR + 50 // Augmenté pour la sécurité

int sockets_clients[MAX_CLIENTS] = {0};
char noms_utilisateurs_clients[MAX_CLIENTS][LONGUEUR_NOM_UTILISATEUR] = {0};
pthread_mutex_t mutex_clients = PTHREAD_MUTEX_INITIALIZER;

FILE *fichier_log; // Pointeur vers le fichier de journalisation

void envoyer_liste_utilisateurs(int sock) {
    char liste_utilisateurs[TAILLE_BUFFER] = "Utilisateurs connectés :\n";
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sockets_clients[i] != 0) {
            strcat(liste_utilisateurs, "- ");
            strcat(liste_utilisateurs, noms_utilisateurs_clients[i]);
            strcat(liste_utilisateurs, "\n");
        }
    }
    pthread_mutex_unlock(&mutex_clients);
    send(sock, liste_utilisateurs, strlen(liste_utilisateurs), 0);
}

void diffuser_message(const char *message, int sock_envoyeur) {
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sockets_clients[i] != 0 && sockets_clients[i] != sock_envoyeur) {
            send(sockets_clients[i], message, strlen(message), 0);
        }
    }
    // Enregistrer le message dans le fichier journal avec la date et l'heure
    time_t temps_actuel;
    struct tm *info_temps;
    char horodatage[20];
    time(&temps_actuel);
    info_temps = localtime(&temps_actuel);
    strftime(horodatage, sizeof(horodatage), "%Y-%m-%d %H:%M:%S", info_temps);

    fprintf(fichier_log, "[%s] %s", horodatage, message);
    fflush(fichier_log);

    pthread_mutex_unlock(&mutex_clients);
}

void *gestion_client(void *desc_socket) {
    int sock = *(int *)desc_socket;
    char message_client[TAILLE_BUFFER] = {0};
    char message[LONGUEUR_MESSAGE] = {0};
    char nom_utilisateur[LONGUEUR_NOM_UTILISATEUR] = {0};
    int taille_lue;

    // Recevoir le nom d'utilisateur
    if ((taille_lue = recv(sock, nom_utilisateur, LONGUEUR_NOM_UTILISATEUR - 1, 0)) > 0) {
        nom_utilisateur[taille_lue] = '\0'; // Assurer que la chaîne est terminée par un null
    } else {
        close(sock);
        return NULL;
    }

    // Ajouter le nom d'utilisateur à la liste
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sockets_clients[i] == 0) {
            sockets_clients[i] = sock;
            strncpy(noms_utilisateurs_clients[i], nom_utilisateur, LONGUEUR_NOM_UTILISATEUR);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    // Envoyer un message de bienvenue personnalisé
    char message_bienvenue[TAILLE_BUFFER];
    snprintf(message_bienvenue, sizeof(message_bienvenue), "Bonjour %s\n", nom_utilisateur);
    send(sock, message_bienvenue, strlen(message_bienvenue), 0);

    // Envoyer la liste des utilisateurs connectés
    envoyer_liste_utilisateurs(sock);

    // Gérer les messages entrants
    while ((taille_lue = recv(sock, message_client, TAILLE_BUFFER - 1, 0)) > 0) {
        message_client[taille_lue] = '\0';
        // S'assurer que la taille du message formaté ne dépasse pas LONGUEUR_MESSAGE
        snprintf(message, LONGUEUR_MESSAGE, "\n# %s > %s\n", nom_utilisateur, message_client);

        diffuser_message(message, sock);
        memset(message_client, 0, TAILLE_BUFFER);
    }

    // Supprimer le socket et le nom d'utilisateur à la déconnexion
    if (taille_lue == 0 || taille_lue == -1) {
        pthread_mutex_lock(&mutex_clients);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (sockets_clients[i] == sock) {
                sockets_clients[i] = 0;
                memset(noms_utilisateurs_clients[i], 0, LONGUEUR_NOM_UTILISATEUR);
                break;
            }
        }
        pthread_mutex_unlock(&mutex_clients);
        close(sock);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int socket_serveur, socket_client, *nouveau_socket;
    struct sockaddr_in adresse_serveur;
    socklen_t longueur_client;
    pthread_t thread_id;

    if (argc != 2) {
        fprintf(stderr, "Usage : %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    socket_serveur = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_serveur == -1) {
        perror("Impossible de créer le socket");
        exit(EXIT_FAILURE);
    }

    adresse_serveur.sin_family = AF_INET;
    adresse_serveur.sin_addr.s_addr = INADDR_ANY;
    adresse_serveur.sin_port = htons(atoi(argv[1]));

    if (bind(socket_serveur, (struct sockaddr *)&adresse_serveur, sizeof(adresse_serveur)) < 0) {
        perror("Échec du bind");
        exit(EXIT_FAILURE);
    }

    listen(socket_serveur, 3);
    printf("Serveur démarré sur IP 0.0.0.0 port %s\nEn attente de nouveaux clients...\n", argv[1]);

    fichier_log = fopen("conversations.log", "a"); // Ouvrir le fichier de journalisation en mode ajout

    while ((socket_client = accept(socket_serveur, (struct sockaddr *)&adresse_serveur, &longueur_client))) {
        printf("Connexion acceptée.\n");

        nouveau_socket = malloc(sizeof(int));
        *nouveau_socket = socket_client;

        if (pthread_create(&thread_id, NULL, gestion_client, (void *)nouveau_socket) < 0) {
            perror("Impossible de créer le thread");
            return 1;
        }
    }

    if (socket_client < 0) {
        perror("Échec de l'acceptation");
        return 1;
    }

    fclose(fichier_log); // Fermer le fichier de journalisation lorsque le serveur se termine

    return 0;
}
