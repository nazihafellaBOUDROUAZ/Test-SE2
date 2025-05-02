#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

#define NBUS_X 5
#define NBUS_Y 4
#define NTRAJETS 10
#define MAX_TURN 3

// Sémaphores et mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_XY, sem_YX;

// États partagés
int NXY = 0, NYX = 0;
int waiting_XY = 0, waiting_YX = 0;
int turn_XY = 0, turn_YX = 0;

// Fonction pour simuler le trajet
void sleep_random() {
    usleep((rand() % 501 + 1000) * 1000); // 1 à 1.5 secondes
}

void entrer_tunnel(int direction) {
    pthread_mutex_lock(&mutex);

    if (direction == 0) {
        waiting_XY++;

        while (NYX > 0 || turn_XY >= MAX_TURN) {
            pthread_mutex_unlock(&mutex);
            sem_wait(&sem_XY); // Attend que ce soit son tour
            pthread_mutex_lock(&mutex);
        }

        waiting_XY--;
        NXY++;
        turn_XY++;
        turn_YX = 0;

    } else {
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

    pthread_mutex_unlock(&mutex);
}

void sortir_tunnel(int direction) {
    pthread_mutex_lock(&mutex);

    if (direction == 0) {
        NXY--;
        if (NXY == 0) {
            if (waiting_YX > 0 && turn_XY >= MAX_TURN) {
                int to_release = waiting_YX < MAX_TURN ? waiting_YX : MAX_TURN;
                for (int i = 0; i < to_release; i++) sem_post(&sem_YX);
            } else {
                int to_release = waiting_XY < MAX_TURN ? waiting_XY : MAX_TURN;
                for (int i = 0; i < to_release; i++) sem_post(&sem_XY);
            }
        }

    } else {
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
    int id = ((int*)arg)[0];
    int ville_depart = ((int*)arg)[1];
    char* noms_villes[2] = {"X", "Y"};

    for (int i = 1; i <= NTRAJETS; i++) {
        // Aller
        entrer_tunnel(ville_depart);
        printf("🚌 Bus %d de %s : %s -> %s (Trajet %d)\n", id, noms_villes[ville_depart], noms_villes[ville_depart], noms_villes[1 - ville_depart], i);
        sleep_random();
        sortir_tunnel(ville_depart);

        // Retour
        entrer_tunnel(1 - ville_depart);
        printf("🚌 Bus %d de %s : %s -> %s (Retour %d)\n", id, noms_villes[1 - ville_depart], noms_villes[1 - ville_depart], noms_villes[ville_depart], i);
        sleep_random();
        sortir_tunnel(1 - ville_depart);
    }

    return NULL;
}

int main() {
    srand(time(NULL));

    pthread_t threads[NBUS_X + NBUS_Y];
    int infos[NBUS_X + NBUS_Y][2];

    // Init sémaphores
    sem_init(&sem_XY, 0, 0);
    sem_init(&sem_YX, 0, 0);

    // Créer les bus X->Y
    for (int i = 0; i < NBUS_X; i++) {
        infos[i][0] = i + 1;
        infos[i][1] = 0;
        pthread_create(&threads[i], NULL, bus_thread, infos[i]);
    }

    // Créer les bus Y->X
    for (int i = 0; i < NBUS_Y; i++) {
        infos[NBUS_X + i][0] = NBUS_X + i + 1;
        infos[NBUS_X + i][1] = 1;
        pthread_create(&threads[NBUS_X + i], NULL, bus_thread, infos[NBUS_X + i]);
    }

    for (int i = 0; i < NBUS_X + NBUS_Y; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&sem_XY);
    sem_destroy(&sem_YX);

    return 0;
}
