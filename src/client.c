/* ===============================================
 * Arquivo: client.c
 * Descrição: Implementação do cliente (sensor) RSSF
 * =============================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <math.h>
#include <time.h>

#define BUFFER_SIZE 256

/* Estrutura da mensagem */
typedef struct {
    char type[12];
    int coords[2];
    float measurement;
} sensor_message;

/* Funções auxiliares */
void print_usage() {
    printf("Usage: ./client <server_ip> <port> -type <temperature|humidity|air_quality> -coords <x> <y>\n");
}

float generate_random_measurement(const char* type) {
    if (strcmp(type, "temperature") == 0) {
        return 20.0 + (rand() % 2000) / 100.0;
    } else if (strcmp(type, "humidity") == 0) {
        return 10.0 + (rand() % 8000) / 100.0;
    } else if (strcmp(type, "air_quality") == 0) {
        return 15.0 + (rand() % 1500) / 100.0;
    }
    return 0.0;
}

/* Função principal do cliente */
int main(int argc, char* argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        print_usage();
        return EXIT_FAILURE;
    }

    const char* server_ip = argv[1];
    int port = atoi(argv[2]);
    const char* type = NULL;
    int coords[2] = { -1, -1 };

    // Valida os argumentos
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-type") == 0 && i + 1 < argc) {
            type = argv[++i];
        } else if (strcmp(argv[i], "-coords") == 0 && i + 2 < argc) {
            coords[0] = atoi(argv[++i]);
            coords[1] = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Error: Invalid argument '%s'\n", argv[i]);
            print_usage();
            return EXIT_FAILURE;
        }
    }

    if (!type || (strcmp(type, "temperature") != 0 && strcmp(type, "humidity") != 0 && strcmp(type, "air_quality") != 0)) {
        fprintf(stderr, "Error: Invalid sensor type\n");
        print_usage();
        return EXIT_FAILURE;
    }

    if (coords[0] < 0 || coords[0] > 9 || coords[1] < 0 || coords[1] > 9) {
        fprintf(stderr, "Error: Coordinates must be in the range 0-9\n");
        print_usage();
        return EXIT_FAILURE;
    }

    // Configura conexão com o servidor
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Erro ao criar socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Erro ao configurar endereço do servidor");
        close(client_socket);
        return EXIT_FAILURE;
    }

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao conectar ao servidor");
        close(client_socket);
        return EXIT_FAILURE;
    }

    printf("Cliente conectado ao servidor %s:%d\n", server_ip, port);

    srand(time(NULL));
    sensor_message msg;
    strcpy(msg.type, type);
    msg.coords[0] = coords[0];
    msg.coords[1] = coords[1];
    msg.measurement = generate_random_measurement(type);

    while (1) {
        // Envia medição ao servidor
        if (send(client_socket, &msg, sizeof(msg), 0) < 0) {
            perror("Erro ao enviar mensagem");
            break;
        }

        printf("log:\n%s sensor in (%d,%d)\nmeasurement: %.4f\n\n", msg.type, msg.coords[0], msg.coords[1], msg.measurement);

        // Recebe mensagens do servidor
        sensor_message incoming_msg;
        while (recv(client_socket, &incoming_msg, sizeof(incoming_msg), 0) > 0) {
            const char* action;

            // Verifica a ação a ser tomada
            if (incoming_msg.coords[0] == coords[0] && incoming_msg.coords[1] == coords[1]) {
                action = "same location";
            } else {
                float distance = sqrt(pow(incoming_msg.coords[0] - coords[0], 2) + pow(incoming_msg.coords[1] - coords[1], 2));

                if (distance > 3) {
                    action = "not neighbor";
                } else {
                    float correction = 0.1 * (1 / (distance + 1)) * (incoming_msg.measurement - msg.measurement);
                    msg.measurement += correction;
                    action = "correction applied";
                }
            }

            printf("log:\n%s sensor in (%d,%d)\nmeasurement: %.4f\naction: %s\n\n",
                   incoming_msg.type, incoming_msg.coords[0], incoming_msg.coords[1], incoming_msg.measurement, action);
        }

        // Atualiza medição (aleatória para demonstração)
        msg.measurement = generate_random_measurement(type);
        sleep(5); // Intervalo entre medições (ajustável por tipo)
    }

    close(client_socket);
    return EXIT_SUCCESS;
}
