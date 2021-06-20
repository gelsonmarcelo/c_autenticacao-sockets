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
	Código do Cliente

	Cliente envia mensagem ao servidor e imprime resposta 
	recebida do Servidor
*/

void pare(){
    setbuf(stdin, NULL);
    getchar();
    setbuf(stdin, NULL);
}

int main(int argc, char *argv[])
{
	// variaveis
	int socket_descritor;
	struct sockaddr_in servidor;
	char *msg_cliente;
	char msg_servidor[MAX_MSG];
	int tamanho;

	/*****************************************/
	/* Cria um socket */
	socket_descritor = socket(AF_INET, SOCK_STREAM, 0);

	/* Informacoes para conectar no servidor */
	servidor.sin_addr.s_addr = inet_addr("127.0.0.1");
	servidor.sin_family = AF_INET;
	servidor.sin_port = htons(3344);

	//Conecta no servidor
	if (connect(socket_descritor, (struct sockaddr *)&servidor, sizeof(servidor)) < 0){
		printf("Nao foi possivel conectar\n");
		return -1;
	}
	printf("Conectado no servidor\n");
	pare();

	/*******COMUNICACAO - TROCA DE MENSAGENS **************/
	// Protocolo dessa aplicacao
	// 1 - Cliente conecta
	// 2 - Cliente envia mensagem
	// 3 - Servidor envia resposta

	//Envia mensagem
	puts("\nDigite a senha para o servidor: ");
	scanf("%s", msg_cliente);
	if (write(socket_descritor, msg_cliente, strlen(msg_cliente)) < 0){
		printf("Erro ao enviar mensagem\n");
		return -1;
	}
	puts("Dados enviados\n");
	pare();

	//Recebe resposta do servidor
	if ((tamanho = read(socket_descritor, msg_servidor, MAX_MSG)) < 0)
	{
		printf("Falha ao receber resposta\n");
		return -1;
	}
	printf("Resposta recebida: ");

	msg_servidor[tamanho] = '\0'; // adiciona fim de linha na string
	puts(msg_servidor);
	pare();

	close(socket_descritor); // fecha o socket
	printf("Cliente finalizado com sucesso!\n");
	return 0;
}