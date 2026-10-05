#include "dns.h"

#include <string.h>

// Escreve um inteiro de 16 bits em network byte order (big endian).
static void put16(uint8_t *p, uint16_t v)
{
    p[0] = v >> 8;
    p[1] = v & 0xFF;
}

// Converte "unb.br" em labels "\3unb\2br\0". Retorna bytes escritos ou -1.
static int encode_name(const char *nome, uint8_t *out, size_t outlen)
{
    size_t len = strlen(nome);
    size_t pos = 0;

    // aceita ponto final
    if (len > 0 && nome[len - 1] == '.')
        len--;
    if (len == 0 || len > 253)
        return -1;

    size_t i = 0;
    while (i < len)
    {
        size_t fim = i;
        while (fim < len && nome[fim] != '.')
            fim++;
        // Converte o label para o formato DNS
        size_t tam = fim - i;
        if (tam == 0 || tam > 63) // label vazio ("a..b") ou grande demais
            return -1;
        if (pos + 1 + tam + 1 > outlen) // label + byte final 0
            return -1;

        out[pos++] = (uint8_t)tam;
        memcpy(out + pos, nome + i, tam);
        pos += tam;
        i = fim + 1;
    }

    out[pos++] = 0;
    return (int)pos;
}

int dns_build_query(const char *dominio, uint16_t id, uint8_t *buf, size_t buflen)
{
    if (buflen < DNS_HEADER_LEN)
        return -1;

    // header (12 bytes)
    put16(buf + 0, id);     // transaction ID
    put16(buf + 2, 0x0100); // flags
    put16(buf + 4, 1);      // QDCOUNT
    put16(buf + 6, 0);      // ANCOUNT
    put16(buf + 8, 0);      // NSCOUNT
    put16(buf + 10, 0);     // ARCOUNT

    // QNAME + QTYPE + QCLASS
    int n = encode_name(dominio, buf + DNS_HEADER_LEN, buflen - DNS_HEADER_LEN);
    if (n < 0)
        return -1;

    size_t pos = DNS_HEADER_LEN + (size_t)n;
    if (pos + 4 > buflen)
        return -1;

    put16(buf + pos, DNS_TYPE_MX);
    put16(buf + pos + 2, DNS_CLASS_IN);
    return (int)(pos + 4);
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

// Le um nome DNS que comeca em msg[*pos], seguindo ponteiros de compressao
// (0xC0xx). Escreve o nome com pontos em out (sem ponto final). Ao retornar,
// *pos aponta para o byte logo apos o nome no ponto onde ele foi lido
// (depois do primeiro ponteiro, se houver). Retorna 0 ou -1 se invalido.
static int read_name(const uint8_t *msg, size_t len, size_t *pos, char *out, size_t outlen)
{
    size_t p = *pos;
    size_t o = 0;
    int saltos = 0;
    int pulou = 0; // ja seguimos um ponteiro? entao *pos nao avanca mais

    for (;;)
    {
        if (p >= len)
            return -1;
        uint8_t tam = msg[p];

        if ((tam & 0xC0) == 0xC0) // ponteiro: 2 bytes, offset nos 14 bits finais
        {
            if (p + 1 >= len)
                return -1;
            if (!pulou)
                *pos = p + 2;
            pulou = 1;
            p = ((tam & 0x3F) << 8) | msg[p + 1];
            if (++saltos > 64) // protege contra lacos de ponteiros
                return -1;
            continue;
        }
        if (tam & 0xC0) // 0x40 / 0x80 sao reservados
            return -1;

        p++;
        if (tam == 0)
            break;
        if (p + tam > len)
            return -1;
        if (o + tam + 2 > outlen) // label + '.' + '\0'
            return -1;

        if (o > 0)
            out[o++] = '.';
        memcpy(out + o, msg + p, tam);
        o += tam;
        p += tam;
    }

    out[o] = '\0';
    if (!pulou)
        *pos = p;
    return 0;
}

// Pula um nome sem decodifica-lo (so precisamos avancar *pos).
static int skip_name(const uint8_t *msg, size_t len, size_t *pos)
{
    char tmp[DNS_MAX_NAME];
    return read_name(msg, len, pos, tmp, sizeof(tmp));
}

dns_status dns_parse_mx(const uint8_t *msg, size_t len, dns_mx *out, size_t max, size_t *qtd)
{
    *qtd = 0;
    if (len < DNS_HEADER_LEN)
        return DNS_ERRO;

    uint16_t flags = get16(msg + 2);
    uint16_t qdcount = get16(msg + 4);
    uint16_t ancount = get16(msg + 6);

    if (!(flags & 0x8000)) // QR: precisa ser resposta
        return DNS_ERRO;

    switch (flags & 0x000F) // RCODE
    {
    case 0:
        break;
    case 3:
        return DNS_NXDOMAIN;
    default:
        return DNS_ERRO;
    }

    size_t pos = DNS_HEADER_LEN;

    // Question: QNAME + QTYPE + QCLASS
    for (uint16_t i = 0; i < qdcount; i++)
    {
        if (skip_name(msg, len, &pos) < 0 || pos + 4 > len)
            return DNS_ERRO;
        pos += 4;
    }

    // Answer: NAME, TYPE, CLASS, TTL (4), RDLENGTH (2), RDATA
    int malformado = 0;
    for (uint16_t i = 0; i < ancount && *qtd < max; i++)
    {
        if (skip_name(msg, len, &pos) < 0 || pos + 10 > len)
        {
            malformado = 1;
            break;
        }
        uint16_t tipo = get16(msg + pos);
        uint16_t classe = get16(msg + pos + 2);
        uint16_t rdlen = get16(msg + pos + 8);
        pos += 10;
        if (pos + rdlen > len)
        {
            malformado = 1;
            break;
        }

        if (tipo == DNS_TYPE_MX && classe == DNS_CLASS_IN && rdlen >= 3)
        {
            // RDATA do MX: PREFERENCE (2 bytes) + EXCHANGE (nome, possivelmente comprimido)
            size_t rpos = pos + 2;
            dns_mx mx;
            mx.preferencia = get16(msg + pos);
            if (read_name(msg, len, &rpos, mx.servidor, sizeof(mx.servidor)) < 0)
            {
                malformado = 1;
                break;
            }
            out[(*qtd)++] = mx;
        }
        pos += rdlen;
    }

    // se ja achamos algum MX, usamos; sem nenhum, malformado e erro, senao e "sem MX"
    if (*qtd == 0)
        return malformado ? DNS_ERRO : DNS_SEM_MX;

    // ordena por preferencia (insercao; a lista e pequena)
    for (size_t i = 1; i < *qtd; i++)
    {
        dns_mx chave = out[i];
        size_t j = i;
        while (j > 0 && out[j - 1].preferencia > chave.preferencia)
        {
            out[j] = out[j - 1];
            j--;
        }
        out[j] = chave;
    }
    return DNS_OK;
}
