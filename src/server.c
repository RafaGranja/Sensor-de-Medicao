#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <math.h>

#define MAXIMO_CLIENTES 12
#define BUFFER_SIZE 256
#define MAXIMO_VIZINHOS 3

/* Estrutura da mensagem */
typedef struct
{
    char type[12];
    int coords[2];
    float measurement;
} sensor_message;

/* Cliente conectado */
typedef struct
{
    int socket;
    char type[12];
    int coords[2];
    float measurement;
} informacoes_clinte;

informacoes_clinte arrayClients[MAXIMO_CLIENTES];
int contagemCliente = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Função para enviar mensagem para outros clientes do mesmo tipo, exceto o remetente */
void menssagemBroadCast(sensor_message msg, const char *type, int remetente)
{
    for (int i = 0; i < contagemCliente; i++)
    {
        if (strcmp(arrayClients[i].type, type) == 0 && arrayClients[i].socket != remetente)
        {
            send(arrayClients[i].socket, &msg, sizeof(msg), 0);
        }
    }
}


float distanciaEuclidiana(int x1, int y1, int x2, int y2)
{
    return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}

/* Função para tratar conexões de clientes */
void *clientePrincipal(void *arg)
{
    int conexaoCliente = *(int *)arg;
    free(arg);

    sensor_message msg;
    while (recv(conexaoCliente, &msg, sizeof(msg), 0) > 0)
    {
        pthread_mutex_lock(&client_mutex);

        // Atualiza cliente existente ou adiciona novo
        int encontrei = 0;
        for (int i = 0; i < contagemCliente; i++)
        {
            if (arrayClients[i].socket == conexaoCliente)
            {
                arrayClients[i].measurement = msg.measurement;
                encontrei = 1;
                break;
            }
        }

        if (!encontrei && contagemCliente < MAXIMO_CLIENTES)
        {
            strcpy(arrayClients[contagemCliente].type, msg.type);
            arrayClients[contagemCliente].coords[0] = msg.coords[0];
            arrayClients[contagemCliente].coords[1] = msg.coords[1];
            arrayClients[contagemCliente].measurement = msg.measurement;
            arrayClients[contagemCliente].socket = conexaoCliente;
            contagemCliente++;
        }

        // Log no servidor
        printf("log:\n%s sensor in (%d,%d)\nmeasurement: %.4f\n\n", msg.type, msg.coords[0], msg.coords[1], msg.measurement);

        // Envia mensagem para outros sensores do mesmo tipo, excluindo o remetente
        menssagemBroadCast(msg, msg.type, conexaoCliente);

        pthread_mutex_unlock(&client_mutex);
    }

    // Cliente foi desconectado deve notificar outros sensores
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < contagemCliente; i++)
    {
        if (arrayClients[i].socket == conexaoCliente)
        {
            // Envia mensagem de remoção para os outros sensores do mesmo tipo
            sensor_message removal_msg;
            strcpy(removal_msg.type, arrayClients[i].type);
            removal_msg.coords[0] = arrayClients[i].coords[0];
            removal_msg.coords[1] = arrayClients[i].coords[1];
            removal_msg.measurement = -1.0000;

            menssagemBroadCast(removal_msg, arrayClients[i].type, conexaoCliente);

            printf("log:\n%s sensor in (%d,%d)\nmeasurement: -1.0000\n\n", arrayClients[i].type, arrayClients[i].coords[0], arrayClients[i].coords[1]);

            // Remove cliente
            arrayClients[i] = arrayClients[contagemCliente - 1];
            contagemCliente = contagemCliente - 1;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);

    close(conexaoCliente);
    pthread_exit(NULL);
}

/* Função principal do servidor */
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <v4|v6> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int servidor;
    struct sockaddr_in servidor_v4;
    struct sockaddr_in6 servidor_v6;
    int ipv6 = strcmp(argv[1], "v6") == 0;
    int porta = atoi(argv[2]);

    if (ipv6)
    {
        servidor = socket(AF_INET6, SOCK_STREAM, 0);
        if (servidor < 0)
        {
            perror("Erro ao criar socket IPv6");
            return EXIT_FAILURE;
        }

        memset(&servidor_v6, 0, sizeof(servidor_v6));
        servidor_v6.sin6_family = AF_INET6;
        servidor_v6.sin6_addr = in6addr_any;
        servidor_v6.sin6_port = htons(porta);

        if (bind(servidor, (struct sockaddr *)&servidor_v6, sizeof(servidor_v6)) < 0)
        {
            perror("Erro ao associar socket IPv6");
            return EXIT_FAILURE;
        }
    }
    else
    {
        servidor = socket(AF_INET, SOCK_STREAM, 0);
        if (servidor < 0)
        {
            perror("Erro ao criar socket IPv4");
            return EXIT_FAILURE;
        }

        memset(&servidor_v4, 0, sizeof(servidor_v4));
        servidor_v4.sin_family = AF_INET;
        servidor_v4.sin_addr.s_addr = INADDR_ANY;
        servidor_v4.sin_port = htons(porta);

        if (bind(servidor, (struct sockaddr *)&servidor_v4, sizeof(servidor_v4)) < 0)
        {
            perror("Erro ao associar socket IPv4");
            return EXIT_FAILURE;
        }
    }

    if (listen(servidor, MAXIMO_CLIENTES) < 0)
    {
        perror("Erro ao ouvir no socket");
        return EXIT_FAILURE;
    }

    while (1)
    {
        struct sockaddr_storage clienteEndereco;
        socklen_t clienteTamanho = sizeof(clienteEndereco);
        int conexaoCliente = accept(servidor, (struct sockaddr *)&clienteEndereco, &clienteTamanho);
        if (conexaoCliente < 0)
        {
            perror("Erro ao aceitar conexão");
            continue;
        }

        pthread_t identificador;
        int *new_sock = malloc(sizeof(int));
        *new_sock = conexaoCliente;

        if (pthread_create(&identificador, NULL, clientePrincipal, (void *)new_sock) != 0)
        {
            perror("Erro ao criar thread");
        }

        pthread_detach(identificador);
    }

    close(servidor);
    return 0;
}
