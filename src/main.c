#define _POSIX_C_SOURCE 200809L // clock_gettime com -std=c11

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "dns.h"

#define TIMEOUT_SEG 2
#define MAX_TENTATIVAS 3
#define MAX_MX 32

// Tempo restante (em ms) ate o prazo; 0 se ja passou.
static long ms_restantes(const struct timespec *prazo)
{
    struct timespec agora;
    clock_gettime(CLOCK_MONOTONIC, &agora);
    long ms = (prazo->tv_sec - agora.tv_sec) * 1000 +
              (prazo->tv_nsec - agora.tv_nsec) / 1000000;
    return ms > 0 ? ms : 0;
}

// Envia a consulta e espera a resposta por ate TIMEOUT_SEG segundos.
// Pacotes que nao sao a resposta esperada (origem diferente do servidor,
// menor que o header, QR=0 ou ID diferente) sao ignorados, sem reiniciar o prazo.
// Retorna o numero de bytes recebidos, ou -1 em timeout/erro.
static ssize_t enviar_e_receber(int sock, const struct sockaddr_in *servidor,
                                const uint8_t *consulta, size_t consulta_len,
                                uint8_t *resposta, size_t resposta_len)
{
    if (sendto(sock, consulta, consulta_len, 0,
               (const struct sockaddr *)servidor, sizeof(*servidor)) < 0)
    {
        perror("sendto");
        return -1;
    }

    struct timespec prazo;
    clock_gettime(CLOCK_MONOTONIC, &prazo);
    prazo.tv_sec += TIMEOUT_SEG;

    for (;;)
    {
        long ms = ms_restantes(&prazo);
        if (ms == 0)
            return -1;

        // SO_RCVTIMEO faz o recvfrom falhar com EAGAIN/EWOULDBLOCK ao estourar o tempo
        struct timeval tv = {.tv_sec = ms / 1000, .tv_usec = (ms % 1000) * 1000};
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            perror("setsockopt");
            return -1;
        }

        struct sockaddr_in origem;
        socklen_t origem_len = sizeof(origem);
        ssize_t n = recvfrom(sock, resposta, resposta_len, 0,
                             (struct sockaddr *)&origem, &origem_len);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                perror("recvfrom");
            return -1;
        }

        if (origem.sin_addr.s_addr != servidor->sin_addr.s_addr ||
            origem.sin_port != servidor->sin_port)
            continue;
        if (n < DNS_HEADER_LEN)
            continue;
        if (!(resposta[2] & 0x80)) // bit QR: 1 = resposta
            continue;
        if (resposta[0] != consulta[0] || resposta[1] != consulta[1]) // Transaction ID
            continue;

        return n;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Uso: %s <dominio> <ip_servidor_dns>\n", argv[0]);
        return 1;
    }

    const char *dominio = argv[1];
    const char *ip_servidor = argv[2];

    struct sockaddr_in servidor;
    memset(&servidor, 0, sizeof(servidor));
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(53);
    if (inet_pton(AF_INET, ip_servidor, &servidor.sin_addr) != 1)
    {
        fprintf(stderr, "IP de servidor invalido: %s\n", ip_servidor);
        return 1;
    }

    // Transaction ID aleatorio de 16 bits
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    uint16_t id = (uint16_t)(rand() & 0xFFFF);

    uint8_t consulta[DNS_MAX_MSG];
    int consulta_len = dns_build_query(dominio, id, consulta, sizeof(consulta));
    if (consulta_len < 0)
    {
        fprintf(stderr, "Nome de dominio invalido: %s\n", dominio);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    // ate MAX_TENTATIVAS envios, cada um aguardando TIMEOUT_SEG segundos
    uint8_t resposta[DNS_MAX_MSG];
    ssize_t n = -1;
    for (int tentativa = 1; tentativa <= MAX_TENTATIVAS && n < 0; tentativa++)
        n = enviar_e_receber(sock, &servidor, consulta, (size_t)consulta_len,
                             resposta, sizeof(resposta));
    close(sock);

    if (n < 0)
    {
        printf("Nao foi possível coletar entrada MX para %s\n", dominio);
        return 1;
    }

    dns_mx mx[MAX_MX];
    size_t qtd;
    switch (dns_parse_mx(resposta, (size_t)n, mx, MAX_MX, &qtd))
    {
    case DNS_OK:
        for (size_t i = 0; i < qtd; i++)
            printf("%s <> %s\n", dominio, mx[i].servidor);
        return 0;
    case DNS_NXDOMAIN:
        printf("Dominio %s nao encontrado\n", dominio);
        return 1;
    case DNS_SEM_MX:
        printf("Dominio %s nao possui entrada MX\n", dominio);
        return 1;
    default:
        printf("Nao foi possível coletar entrada MX para %s\n", dominio);
        return 1;
    }
}
