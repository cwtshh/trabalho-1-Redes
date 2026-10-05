#ifndef DNS_H
#define DNS_H

#include <stddef.h>
#include <stdint.h>

#define DNS_HEADER_LEN 12
#define DNS_TYPE_MX 15
#define DNS_CLASS_IN 1
#define DNS_MAX_MSG 512 /* tamanho maximo de mensagem DNS sobre UDP (RFC1035) */

int dns_build_query(const char *dominio, uint16_t id, uint8_t *buf, size_t buflen);

#define DNS_MAX_NAME 256

typedef struct
{
    uint16_t preferencia;
    char servidor[DNS_MAX_NAME]; // nome do servidor de e-mail, sem ponto final
} dns_mx;

typedef enum
{
    DNS_OK,       // ao menos 1 registro MX extraido
    DNS_NXDOMAIN, // RCODE 3: dominio nao existe
    DNS_SEM_MX,   // dominio existe, mas sem registro MX
    DNS_ERRO      // resposta malformada ou RCODE de falha (SERVFAIL, REFUSED...)
} dns_status;

// Interpreta a resposta em msg (len bytes) e preenche out com ate max registros MX,
// ordenados por preferencia (menor primeiro). *qtd recebe quantos foram extraidos.
dns_status dns_parse_mx(const uint8_t *msg, size_t len, dns_mx *out, size_t max, size_t *qtd);

#endif
