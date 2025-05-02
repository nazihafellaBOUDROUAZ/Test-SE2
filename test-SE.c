#include <stdio.h>      // Pour les entrées/sorties (printf)
#include <stdlib.h>     // Pour les fonctions générales (rand, malloc, etc.)
#include <pthread.h>    // Pour la gestion des threads
#include <unistd.h>     // Pour usleep (temporisation)
#include <semaphore.h>  // Pour utiliser les sémaphores
#include <time.h>       // Pour initialiser la graine aléatoire

#define NBUS_X 5        // Nombre de bus partant de la ville X
#define NBUS_Y 4        // Nombre de bus partant de la ville Y
#define NTRAJETS 10     // Nombre de trajets aller-retour par bus
#define MAX_TURN 3      // Nombre max de bus consécutifs dans une même direction

// Initialisation des verrous et sémaphores
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex pour protéger les variables partagées
sem_t sem_XY, sem_YX;  // Sémaphores pour gérer l’attente des bus X->Y et Y->X

// Variables globales partagées entre les threads
int NXY = 0, NYX = 0;                  // Nombre de bus actuellement dans le tunnel (X->Y et Y->X)
int waiting_XY = 0, waiting_YX = 0;   // Nombre de bus en attente de chaque côté
int turn_XY = 0, turn_YX = 0;         // Compteurs pour limiter le nombre de passages consécutifs

// Fonction utilitaire : simule un trajet avec un délai aléatoire entre 1s et 1.5s
void sleep_random() {
    usleep((rand() % 501 + 1000) * 1000);
}
void entrer_tunnel(int direction) {
    pthread_mutex_lock(&mutex); // Entrée section critique

    if (direction == 0) { // Cas des bus X → Y
        waiting_XY++; // Indique qu’un bus attend
        while (NYX > 0 || turn_XY >= MAX_TURN) {
            pthread_mutex_unlock(&mutex); // Libère mutex avant d’attendre
            sem_wait(&sem_XY); // Attend un signal
            pthread_mutex_lock(&mutex); // Ré-acquiert le mutex après le réveil
        }
        waiting_XY--; // Le bus est prêt à entrer
        NXY++; // Incrémente le compteur de bus dans le tunnel
        turn_XY++; // Incrémente le nombre de passages consécutifs X->Y
        turn_YX = 0; // Réinitialise le compteur inverse

    } else { // Cas des bus Y → X
        waiting_YX++;
        while (NXY > 0 || turn_YX >= MAX_TURN) {
            pthread_mutex_unlock(&mutex);
            sem_wait(&sem_YX);
            pthread_mutex_lock(&mutex);
        }
        waiting_YX--;
        NYX++;
        turn_YX++;
        turn_XY = 0;
    }

    pthread_mutex_unlock(&mutex); // Sortie section critique
}
void sortir_tunnel(int direction) {
    pthread_mutex_lock(&mutex);

    if (direction == 0) { // Bus X->Y sort
        NXY--; // Décrémentation du nombre de bus dans le tunnel
        if (NXY == 0) { // Si le tunnel devient vide
            if (waiting_YX > 0 && turn_XY >= MAX_TURN) {
                // Donne la main à l'autre sens
                int to_release = waiting_YX < MAX_TURN ? waiting_YX : MAX_TURN;
                for (int i = 0; i < to_release; i++) sem_post(&sem_YX);
            } else {
                // Continue dans le même sens
                int to_release = waiting_XY < MAX_TURN ? waiting_XY : MAX_TURN;
                for (int i = 0; i < to_release; i++) sem_post(&sem_XY);
            }
        }

    } else { // Bus Y->X sort
        NYX--;
        if (NYX == 0) {
            if (waiting_XY > 0 && turn_YX >= MAX_TURN) {
                int to_release = waiting_XY < MAX_TURN ? waiting_XY : MAX_TURN;
                for (int i = 0; i < to_release; i++) sem_post(&sem_XY);
            } else {
                int to_release = waiting_YX < MAX_TURN ? waiting_YX : MAX_TURN;
                for (int i = 0; i < to_release; i++) sem_post(&sem_YX);
            }
        }
    }

    pthread_mutex_unlock(&mutex);
}
void* bus_thread(void* arg) {
    int id = ((int*)arg)[0];           // Numéro du bus
    int ville_depart = ((int*)arg)[1]; // 0 = ville X, 1 = ville Y
    char* noms_villes[2] = {"X", "Y"};

    for (int i = 1; i <= NTRAJETS; i++) {
        // Trajet aller
        entrer_tunnel(ville_depart);
        printf("🚌 Bus %d de %s : %s -> %s (Trajet %d)\n",
               id, noms_villes[ville_depart],
               noms_villes[ville_depart], noms_villes[1 - ville_depart], i);
        sleep_random(); // Simule le passage dans le tunnel
        sortir_tunnel(ville_depart);

        // Trajet retour
        entrer_tunnel(1 - ville_depart);
        printf("🚌 Bus %d de %s : %s -> %s (Retour %d)\n",
               id, noms_villes[1 - ville_depart],
               noms_villes[1 - ville_depart], noms_villes[ville_depart], i);
        sleep_random();
        sortir_tunnel(1 - ville_depart);
    }

    return NULL;
}
int main() {
    srand(time(NULL)); // Initialise le générateur aléatoire

    pthread_t threads[NBUS_X + NBUS_Y];    // Tableau de threads
    int infos[NBUS_X + NBUS_Y][2];         // Stocke l'id et la ville de départ

    // Initialisation des sémaphores
    sem_init(&sem_XY, 0, 0);
    sem_init(&sem_YX, 0, 0);

    // Création des bus partant de X
    for (int i = 0; i < NBUS_X; i++) {
        infos[i][0] = i + 1;    // ID
        infos[i][1] = 0;        // Ville X
        pthread_create(&threads[i], NULL, bus_thread, infos[i]);
    }

    // Création des bus partant de Y
    for (int i = 0; i < NBUS_Y; i++) {
        infos[NBUS_X + i][0] = NBUS_X + i + 1;
        infos[NBUS_X + i][1] = 1; // Ville Y
        pthread_create(&threads[NBUS_X + i], NULL, bus_thread, infos[NBUS_X + i]);
    }

    // Attente de la fin des threads
    for (int i = 0; i < NBUS_X + NBUS_Y; i++) {
        pthread_join(threads[i], NULL);
    }

    // Nettoyage des sémaphores
    sem_destroy(&sem_XY);
    sem_destroy(&sem_YX);

    return 0;
}
