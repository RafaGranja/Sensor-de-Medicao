/* ===============================================
 * Arquivo: server.c
 * Descrição: Implementação do servidor RSSF
 * =============================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <math.h>

#define MAX_CLIENTS 12
#define BUFFER_SIZE 256
#define MAX_NEIGHBORS 3

/* Estrutura da mensagem */
typedef struct {
    char type[12];
    int coords[2];
    float measurement;
} sensor_message;

/* Cliente conectado */
typedef struct {
    int socket;
    char type[12];
    int coords[2];
    float measurement;
} client_info;

client_info clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Funções auxiliares */
void broadcast_message(sensor_message msg, const char* type);
float calculate_distance(int x1, int y1, int x2, int y2);

float calculate_distance(int x1, int y1, int x2, int y2) {
    return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}

/* Função para tratar conexões de clientes */
void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    sensor_message msg;
    while (recv(client_socket, &msg, sizeof(msg), 0) > 0) {
        pthread_mutex_lock(&client_mutex);

        // Atualiza cliente existente ou adiciona novo
        int found = 0;
        for (int i = 0; i < client_count; i++) {
            if (clients[i].socket == client_socket) {
                clients[i].measurement = msg.measurement;
                found = 1;
                break;
            }
        }

        if (!found && client_count < MAX_CLIENTS) {
            strcpy(clients[client_count].type, msg.type);
            clients[client_count].coords[0] = msg.coords[0];
            clients[client_count].coords[1] = msg.coords[1];
            clients[client_count].measurement = msg.measurement;
            clients[client_count].socket = client_socket;
            client_count++;
        }

        // Log no servidor
        printf("log:\n%s sensor in (%d,%d)\nmeasurement: %.4f\n\n", msg.type, msg.coords[0], msg.coords[1], msg.measurement);

        // Envia mensagem para outros sensores do mesmo tipo
        broadcast_message(msg, msg.type);

        pthread_mutex_unlock(&client_mutex);
    }

    // Cliente desconectado
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client_socket) {
            printf("log:\n%s sensor in (%d,%d)\nmeasurement: -1.0000\n\n", clients[i].type, clients[i].coords[0], clients[i].coords[1]);

            // Remove cliente
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);

    close(client_socket);
    pthread_exit(NULL);
}

/* Função para enviar mensagem para outros clientes do mesmo tipo */
void broadcast_message(sensor_message msg, const char* type) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].type, type) == 0 && clients[i].socket != -1) {
            send(clients[i].socket, &msg, sizeof(msg), 0);
        }
    }
}

/* Função principal do servidor */
int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <v4|v6> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int server_socket;
    struct sockaddr_in server_addr_v4;
    struct sockaddr_in6 server_addr_v6;
    int is_ipv6 = strcmp(argv[1], "v6") == 0;
    int port = atoi(argv[2]);

    if (is_ipv6) {
        server_socket = socket(AF_INET6, SOCK_STREAM, 0);
        if (server_socket < 0) {
            perror("Erro ao criar socket IPv6");
            return EXIT_FAILURE;
        }

        memset(&server_addr_v6, 0, sizeof(server_addr_v6));
        server_addr_v6.sin6_family = AF_INET6;
        server_addr_v6.sin6_addr = in6addr_any;
        server_addr_v6.sin6_port = htons(port);

        if (bind(server_socket, (struct sockaddr*)&server_addr_v6, sizeof(server_addr_v6)) < 0) {
            perror("Erro ao associar socket IPv6");
            return EXIT_FAILURE;
        }
    } else {
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            perror("Erro ao criar socket IPv4");
            return EXIT_FAILURE;
        }

        memset(&server_addr_v4, 0, sizeof(server_addr_v4));
        server_addr_v4.sin_family = AF_INET;
        server_addr_v4.sin_addr.s_addr = INADDR_ANY;
        server_addr_v4.sin_port = htons(port);

        if (bind(server_socket, (struct sockaddr*)&server_addr_v4, sizeof(server_addr_v4)) < 0) {
            perror("Erro ao associar socket IPv4");
            return EXIT_FAILURE;
        }
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Erro ao ouvir no socket");
        return EXIT_FAILURE;
    }

    printf("Servidor iniciado na porta %d usando %s\n", port, is_ipv6 ? "IPv6" : "IPv4");

    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Erro ao aceitar conexão");
            continue;
        }

        pthread_t thread_id;
        int* new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void*)new_sock) != 0) {
            perror("Erro ao criar thread");
        }

        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
