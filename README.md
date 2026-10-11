# Trabalho 1 — Cliente DNS

## Fundamentos de Redes de Computadores

### Integrantes

- 
- Daniel Ferreira Nunes - 211061565
- Harleny Angélica Araújo de Sousa — 211061832
- 

---

## 1. Descrição

Este projeto consiste na implementação de um cliente DNS utilizando o protocolo UDP.

O cliente recebe como argumentos o domínio que deverá ser consultado e o endereço IP do servidor DNS. A aplicação realiza uma consulta do tipo **MX (Mail Exchanger)** e apresenta como resultado o nome do servidor responsável pelo recebimento de e-mails do domínio informado.

A requisição DNS é montada manualmente pela aplicação, seguindo o formato definido pelas RFCs 1034 e 1035.

---

## 2. Sistema Operacional

O projeto foi desenvolvido e testado em:

- **Sistema operacional:** Linux
- **Distribuição:** Ubuntu
- **Arquitetura:** x86_64

---

## 3. Ambiente de Desenvolvimento

Foram utilizadas as seguintes ferramentas:

- **Linguagem:** C
- **Compilador:** GCC
- **Padrão utilizado:** C11
- **Editor de código:** Visual Studio Code
- **Sistema de build:** GNU Make
- **Comunicação de rede:** Socket UDP

A compilação é realizada utilizando o `Makefile` fornecido no projeto.

---

## 4. Estrutura do Projeto

```text
.
├── src/
│   ├── main.c
│   ├── dns.c
│   └── dns.h
├── .gitignore
├── Makefile
└── README.md
```

### `main.c`

Responsável pelo fluxo principal da aplicação, incluindo:

- leitura dos argumentos;
- configuração do servidor DNS;
- criação do socket UDP;
- envio da consulta;
- recebimento da resposta;
- controle das tentativas;
- apresentação dos resultados.

### `dns.c`

Responsável pela implementação das funcionalidades relacionadas ao protocolo DNS, incluindo:

- montagem da requisição DNS;
- codificação do nome do domínio;
- interpretação da resposta;
- extração dos registros MX;
- tratamento de compressão de nomes DNS;
- ordenação dos registros MX por preferência.

### `dns.h`

Contém as constantes, estruturas e declarações das funções utilizadas na implementação do cliente DNS.

---

## 5. Como Construir

Na raiz do projeto, execute:

```bash
make
```

O comando irá compilar os arquivos-fonte presentes no diretório `src/` e gerar o executável `meu_cliente`.

Durante a compilação, os arquivos objeto são gerados no diretório `build/`.

Para remover os arquivos gerados pela compilação, execute:

```bash
make clean
```

Esse comando remove o executável `meu_cliente` e o diretório `build/`.

---

## 6. Como Executar

Após a compilação, execute:

```bash
./meu_cliente <dominio> <ip_servidor_dns>
```

Onde:

- `<dominio>` é o domínio que será consultado;
- `<ip_servidor_dns>` é o endereço IPv4 do servidor DNS que receberá a consulta.

### Exemplo

```bash
./meu_cliente unb.br 8.8.8.8
```

Resultado obtido durante os testes:

```text
unb.br <> unb-br.mail.protection.outlook.com
```

---

## 7. Funcionamento

A aplicação realiza exclusivamente consultas do tipo **MX**, utilizando o protocolo **UDP**, através da porta **53**.

Para cada consulta:

1. É gerado um identificador de transação de 16 bits.
2. A requisição DNS é montada manualmente.
3. O domínio informado é convertido para o formato de nomes utilizado pelo protocolo DNS.
4. A consulta é enviada ao servidor DNS.
5. A aplicação aguarda a resposta por até **2 segundos**.
6. Caso não receba uma resposta válida, uma nova tentativa é realizada.
7. São realizadas no máximo **3 tentativas**.
8. Ao receber uma resposta válida, o cliente interpreta os registros retornados e procura pelos registros do tipo MX.

O programa também verifica o identificador da transação e se a mensagem recebida corresponde a uma resposta DNS.

---

## 8. Resultados Possíveis

### Consulta realizada com sucesso

```text
unb.br <> unb-br.mail.protection.outlook.com
```

### Domínio não encontrado

Quando o servidor DNS informa que o domínio não existe:

```text
Dominio <dominio> nao encontrado
```

### Domínio sem entrada MX

Quando o domínio existe, mas não possui registro MX:

```text
Dominio <dominio> nao possui entrada MX
```

### Falha na comunicação

Quando não é possível obter uma resposta válida do servidor DNS após as tentativas:

```text
Nao foi possível coletar entrada MX para <dominio>
```

---

## 9. Instruções de Uso

Para realizar uma consulta, execute o programa informando o domínio e o servidor DNS:

```bash
./meu_cliente unb.br 8.8.8.8
```

O segundo argumento pode ser substituído pelo endereço IPv4 de outro servidor DNS.

A aplicação não possui interface gráfica. Toda a interação ocorre através do terminal, e o resultado é apresentado na saída padrão.

