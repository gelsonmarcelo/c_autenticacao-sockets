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

void pare(){
    setbuf(stdin, NULL);
    getchar();
    setbuf(stdin, NULL);
}

int main(void){
    //Declaração das variáveis
    int socket_descritor, conexao, socket_size;
    struct sockaddr_in servidor, cliente;
    char *msg_servidor;
    char msg_cliente[MAX_MSG];
    int tamanho, count;

    // para pegar o IP e porta do cliente
    char *cliente_ip;
    int cliente_port;

    /*********************************************************/
    //Cria um socket
    socket_descritor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_descritor == -1){
        printf("Erro ao criar o socket\n");
        return -1;
    }

    //Prepara a struct do socket
    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY; // Obtem IP do Sistema Operacional
    servidor.sin_port = htons(3344);

    //Associa o socket a porta e endereco
    if (bind(socket_descritor, (struct sockaddr *)&servidor, sizeof(servidor)) < 0){
        perror("Erro ao fazer bind\n");
        return -1;
    }
    puts("Bind efetuado com sucesso\n");
    pare();

    // Ouve por conexoes
    listen(socket_descritor, 3);

    //Aceita e trata conexoes
    puts("Aguardando por conexoes...");
    socket_size = sizeof(struct sockaddr_in);
    conexao = accept(socket_descritor, (struct sockaddr *)&cliente, (socklen_t *)&socket_size);
    if (conexao < 0){
        perror("Erro ao receber conexao\n");
        return -1;
    }

    /*********comunicao entre cliente/servidor****************/

    // obtem IP e porta do cliente
    cliente_ip = inet_ntoa(cliente.sin_addr);
    cliente_port = ntohs(cliente.sin_port);
    printf("cliente conectou\nIP:PORTA -> %s:%d\n", cliente_ip, cliente_port);
    pare();

    // le dados enviados pelo cliente
    if ((tamanho = read(conexao, msg_cliente, MAX_MSG)) < 0){
        perror("Erro ao receber dados do cliente: ");
        return -1;
    }
    puts("Leu o socket que o cliente escreveu");
    
    // Coloca terminador de string
    msg_cliente[tamanho] = '\0';
    
    if(!strcmp(msg_cliente, "senha123")){
        printf("Senha aprovada> O cliente enviou: %s\n", msg_cliente);
        msg_servidor = "Senha aprovada";
    }else{
        printf("Senha negada> O cliente enviou: %s\n", msg_cliente);
        msg_servidor = "Senha negada";
    }

    pare();

    // Envia resposta para o cliente
    puts("Enviei resposta para o cliente");
    write(conexao, msg_servidor, strlen(msg_servidor));
    pare();
    /*********************************************************/

    shutdown(socket_descritor, 2);
    // shutdown(socket_descritor, SHUT_RDWR);
    close(socket_descritor);    

    printf("Servidor finalizado.\n");
    return 0;
}