#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TAILLE_BUFFER 1024

// Fonction exécutée par le thread pour recevoir les messages du serveur
void *recevoir_message(void *socket) {
    int sockfd = *(int*)socket;
    char buffer[TAILLE_BUFFER];
    while(1) {
        memset(buffer, 0, TAILLE_BUFFER);
        int reception = recv(sockfd, buffer, TAILLE_BUFFER, 0);
        if(reception > 0) {
            putchar('\n');
            printf("%s", buffer);
            putchar('\n');
            printf("Envoyer un nouveau message : "); // Invitation à saisir un nouveau message après réception
            fflush(stdout); // Assure l'impression immédiate de l'invitation
        } else if (reception == 0) {
            break;
        } else {
            // Gestion des erreurs
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Utilisation : %s <IP serveur> <port> <Pseudo>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erreur lors de la création du socket");
        return EXIT_FAILURE;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("Adresse invalide ou non supportée");
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Echec de la connexion");
        return EXIT_FAILURE;
    }

    // Envoi du pseudo immédiatement après la connexion
    send(sockfd, argv[3], strlen(argv[3]), 0);

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, recevoir_message, (void*)&sockfd) < 0) {
        perror("Impossible de créer le thread");
        return EXIT_FAILURE;
    }
    
    char buffer[TAILLE_BUFFER] = {0};
    while (1) {
        printf("Envoyer un nouveau message : ");
        fgets(buffer, TAILLE_BUFFER, stdin);
        send(sockfd, buffer, strlen(buffer), 0);
    }

    pthread_join(thread_id, NULL);
    close(sockfd);
    return 0;
}
