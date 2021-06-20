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

void pare()
{
	setbuf(stdin, NULL);
	getchar();
	setbuf(stdin, NULL);
}

int main(int argc, char *argv[])
{
	int socket_descritor;		 //Variável que guarda o descritor do socket
	struct sockaddr_in servidor; //Estrutura que guarda dados do servidor que irá se conectar
	char *msg_cliente;			 //String com mensagem enviada do cliente
	char msg_servidor[MAX_MSG];	 //Mensagem que será recebida, vinda do servidor
	int tamanho;				 //Mantém o tamanho da mensagem recebida, vinda do servidor

	/******* CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/
	//Cria um socket
	socket_descritor = socket(AF_INET, SOCK_STREAM, 0);
	printf("\nSocket_descritor: (int) %d\n", socket_descritor);

	//Atribuindo informações das configurações do servidor, na sua estrutura
	servidor.sin_addr.s_addr = inet_addr("127.0.0.1"); //Endereço IP
	servidor.sin_family = AF_INET;					   //Para comunicação IPV4
	servidor.sin_port = htons(3345);				   //Porta de conexão do servidor

	//Conecta no servidor
	if (connect(socket_descritor, (struct sockaddr *)&servidor, sizeof(servidor)) < 0)
	{
		printf("Nao foi possivel conectar\n");
		return -1;
	}
	printf("Conectado no servidor\n");

	/******* COMUNICACAO - TROCA DE MENSAGENS *******/
	// Protocolo dessa aplicacao
	// 1 - Cliente conecta
	// 2 - Cliente envia mensagem
	// 3 - Servidor envia resposta

	do
	{
		//Escreve/envia mensagem no socket
		puts("\nDigite a senha para o servidor: ");
		scanf("%s", msg_cliente);
		if (write(socket_descritor, msg_cliente, strlen(msg_cliente)) < 0)
		{
			printf("Erro ao enviar mensagem\n");
			return -1;
		}
		puts("Dados enviados\nAguardando resposta do servidor...");

		//Lê/recebe resposta do servidor
		if ((tamanho = read(socket_descritor, msg_servidor, MAX_MSG)) < 0)
		{
			printf("Falha ao receber resposta\n");
			return -1;
		}
		printf("Resposta recebida: ");

		msg_servidor[tamanho] = '\0'; //Adiciona fim de linha na string
		puts(msg_servidor);
	} while (strcmp(msg_servidor, "Senha aprovada"));

	/******* FINALIZA CONEXÃO *******/
	close(socket_descritor); //Fecha o socket
	printf("Cliente finalizado com sucesso!\n");
	pare();
	system("clear");
	return 0;
}