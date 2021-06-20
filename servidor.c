#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MAX_MSG 1024

/*
    Código do Servidor

    Servidor aguarda por mensagem do cliente, imprime na tela, 
    envia resposta e finaliza processo
*/

void pare()
{
    setbuf(stdin, NULL);
    getchar();
    setbuf(stdin, NULL);
}

int main(void)
{
    int socket_descritor;                 //Descritor do socket
    int conexao;                          //Guarda o retorno de accept, um arquivo descritor do socket conectado
    int socket_size;                      //Guarda o tamanho da estrutura do socket
    struct sockaddr_in servidor, cliente; //Estruturas de cliente e servidor, para guardar seus dados
    char *msg_servidor;                   //Guarda a mensagem que será enviada pelo servidor
    char msg_cliente[MAX_MSG];            //Guarda a mensagem recebida do cliente
    int tamanho;                          //Guarda o tamanho da mensagem recebida pelo cliente
    // int count;

    char *cliente_ip; //Para pegar o IP do cliente
    int cliente_port; //Para pegar porta do cliente

    /******* CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/
    //Cria um socket
    socket_descritor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_descritor == -1)
    {
        printf("Erro ao criar o socket\n");
        return -1;
    }

    //Prepara a struct do socket
    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY; // Obtem IP do Sistema Operacional
    servidor.sin_port = htons(3345);

    //Associa o socket à porta e endereço
    if (bind(socket_descritor, (struct sockaddr *)&servidor, sizeof(servidor)) < 0)
    {
        perror("Erro ao fazer bind\n");
        return -1;
    }
    puts("Bind efetuado com sucesso\n");

    //Ouve por conexões
    listen(socket_descritor, 3);

    //Aceita e trata conexões
    puts("Aguardando por conexoes...");
    socket_size = sizeof(struct sockaddr_in);
    conexao = accept(socket_descritor, (struct sockaddr *)&cliente, (socklen_t *)&socket_size);
    if (conexao < 0)
    {
        perror("Erro ao receber conexao\n");
        return -1;
    }

    //Obtém IP e porta do cliente
    cliente_ip = inet_ntoa(cliente.sin_addr);
    cliente_port = ntohs(cliente.sin_port);
    printf("Cliente conectou\nIP:PORTA -> %s:%d\n", cliente_ip, cliente_port);

    do
    {
        /******* COMINUCAÇÃO ENTRE CLIENTE/SERVIDOR *******/
        puts("Aguardando resposta do cliente...");
        //Lê dados enviados pelo cliente
        if ((tamanho = read(conexao, msg_cliente, MAX_MSG)) < 0)
        {
            perror("Erro ao receber dados do cliente: ");
            return -1;
        }
        puts("Leu o socket que o cliente escreveu");

        //Adiciona terminador de string
        msg_cliente[tamanho] = '\0';

        if (!strcmp(msg_cliente, "senha123"))
        {
            printf("Senha aprovada> O cliente enviou: %s\n", msg_cliente);
            msg_servidor = "Senha aprovada";
        }
        else
        {
            printf("Senha negada> O cliente enviou: %s\n", msg_cliente);
            msg_servidor = "Senha negada";
        }

        puts("Pressione enter para responder ao cliente...");
        pare();

        //Envia resposta para o cliente
        puts("Enviei resposta para o cliente");
        write(conexao, msg_servidor, strlen(msg_servidor));
        puts("Digite s para finalizar...");
    } while (getchar() != 's');

    /******* FINALIZA CONEXÃO *******/
    shutdown(socket_descritor, 2);
    //shutdown(socket_descritor, SHUT_RDWR);
    close(socket_descritor);

    printf("Servidor finalizado.\n");
    pare();
    system("clear");
    return 0;
}