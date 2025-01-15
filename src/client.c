#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <math.h>
#include <time.h>

#define BUFFER_SIZE 256

/* Estrutura da mensagem */
typedef struct
{
    char type[12];
    int coords[2];
    float measurement;
} sensor_message;

// Funções auxiliares 
float distanciaEuclidiana(int x1, int y1, int x2, int y2)
{
    return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}
void imprimeErroEntrada()
{
    printf("Usage: ./client <server_ip> <port> -type <temperature|humidity|air_quality> -coords <x> <y>\n");
}

float medicaoAleatoria(const char *type)
{
    float result = 0.0;

    if (strcmp(type, "temperature") == 0)
    {
        result = (20.0 + (rand() % 2000) / 100.0);
    }
    else if (strcmp(type, "humidity") == 0)
    {
        result = (10.0 + (rand() % 8000) / 100.0);
    }
    else if (strcmp(type, "air_quality") == 0)
    {
        result = (15.0 + (rand() % 1500) / 100.0);
    }
    return result;
}

// Função principal do cliente
int main(int argc, char *argv[])
{
    if (argc != 8)
    {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        imprimeErroEntrada();
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *type = NULL;
    int coords[2] = {-1, -1};

    // Valida os argumentos
    for (int i = 3; i < argc; i++)
    {
        if (strcmp(argv[i], "-type") == 0 && i + 1 < argc)
        {
            type = argv[++i];
        }
        else if (strcmp(argv[i], "-coords") == 0 && i + 2 < argc)
        {
            coords[0] = atoi(argv[++i]);
            coords[1] = atoi(argv[++i]);
        }
        else
        {
            fprintf(stderr, "Error: Invalid argument '%s'\n", argv[i]);
            imprimeErroEntrada();
            return EXIT_FAILURE;
        }
    }

    if (!type || (strcmp(type, "temperature") != 0 && strcmp(type, "humidity") != 0 && strcmp(type, "air_quality") != 0))
    {
        fprintf(stderr, "Error: Invalid sensor type\n");
        imprimeErroEntrada();
        return EXIT_FAILURE;
    }

    if (coords[0] < 0 || coords[0] > 9 || coords[1] < 0 || coords[1] > 9)
    {
        fprintf(stderr, "Error: Coordinates must be in the range 0-9\n");
        imprimeErroEntrada();
        return EXIT_FAILURE;
    }

    // Conecta com o servidor
    int conexaoCliente = socket(AF_INET, SOCK_STREAM, 0);
    if (conexaoCliente < 0)
    {
        perror("Erro ao criar socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in enderecoServidor;
    enderecoServidor.sin_family = AF_INET;
    enderecoServidor.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &enderecoServidor.sin_addr) <= 0)
    {
        perror("Erro ao configurar endereço do servidor");
        close(conexaoCliente);
        return EXIT_FAILURE;
    }

    if (connect(conexaoCliente, (struct sockaddr *)&enderecoServidor, sizeof(enderecoServidor)) < 0)
    {
        perror("Erro ao conectar ao servidor");
        close(conexaoCliente);
        return EXIT_FAILURE;
    }

    printf("Cliente conectado ao servidor %s:%d\n", server_ip, port);

    srand(time(NULL));
    sensor_message msg;
    strcpy(msg.type, type);
    msg.coords[0] = coords[0];
    msg.coords[1] = coords[1];
    msg.measurement = medicaoAleatoria(type);

    while (1)
    {
        // Envia
        if (send(conexaoCliente, &msg, sizeof(msg), 0) < 0)
        {
            perror("Erro ao enviar mensagem");
            break;
        }

        printf("log:\n%s sensor in (%d,%d)\nmeasurement: %.4f\naction: %s\n\n", msg.type, msg.coords[0], msg.coords[1], msg.measurement, "same location");

        // Recebe
        sensor_message incoming_msg;
        while (recv(conexaoCliente, &incoming_msg, sizeof(incoming_msg), 0) > 0)
        {
            const char *action;

            // Verifica do action
            if (incoming_msg.measurement < 0)
            {
                action = "removed";
            }
            else if (incoming_msg.coords[0] == coords[0] && incoming_msg.coords[1] == coords[1])
            {
                action = "same location";
            }
            else
            {

                float distance = distanciaEuclidiana(incoming_msg.coords[0],incoming_msg.coords[1],coords[0],coords[1]);

                if (distance > 3)
                {
                    action = "not neighbor";
                }
                else
                {
                    float correction = 0.1 * (1 / (distance + 1)) * (incoming_msg.measurement - msg.measurement);
                    msg.measurement += correction;
                    action = "correction applied";
                }
            }

            printf("log:\n%s sensor in (%d,%d)\nmeasurement: %.4f\naction: %s\n\n",incoming_msg.type, incoming_msg.coords[0], incoming_msg.coords[1], incoming_msg.measurement, action);

        }

        // Atualiza medição 
        msg.measurement = medicaoAleatoria(type);
        sleep(5); // Intervalo
    }

    close(conexaoCliente);
    return EXIT_SUCCESS;
}
