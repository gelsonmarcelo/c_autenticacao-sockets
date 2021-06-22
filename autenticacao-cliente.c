#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <sys/random.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//Tamanho definido do salt
#define SALT_SIZE 16
//Valor grande para ser utilizado como tamanho em algumas strings
#define MAX 2048
//Tamanho máximo das strings padrão de dados comuns (contando com o último caractere NULL)
#define MAX_DADOS 51
//Tamanho máximo da string do identificador (contando com o último caractere NULL)
#define MAX_IDENTIFICADOR 16
//Tamanho máximo da string da senha (contando com o último caractere NULL)
#define MAX_SENHA 31
//Parâmetro da função crypt
#define PARAMETRO_CRYPT "$6$rounds=20000$%s$"
//###
#define MAX_MSG 1024
#define PORTA 3344
#define ENDERECO_SERVIDOR "127.0.0.1"

/**
 *      POLÍTICA DE IDENTIFICADORES E SENHAS
 - Identificador
 * Para criação do identificador, não pode ser utilizado nome, sobrenome ou email;
 * Pode conter somente caracteres alfanuméricos e ponto final;
 * Deve ter no mínimo 5 caracteres e no máximo 15;
 - Senha
 * Deve conter no mínimo 8 caracteres e no máximo 30;
 * Deve conter, no mínimo, 2 caracteres especiais;
 * Deve conter números e letras;
 * Deve conter pelo menos uma letra maiúscula e uma minúscula;
 * Não pode conter mais de 2 números ordenados em sequência;
 * Não pode conter mais de 2 números repetidos em sequência;
 * Não pode conter caracteres que não sejam alfanuméricos, caracteres especiais ou espaço.
 **/

//Variáveis globais da conexão
int socketDescritor;       //Variável que guarda o descritor do socket
char msgServidor[MAX_MSG]; //Mensagem que será recebida, vinda do servidor

struct Usuario u; //Declaração da estrutura do usuário

/*Declaração das funções*/

short int autenticar();
void cadastrarUsuario();
void limparEstruturaUsuario();
void imprimirDecoracao();
void coletarDados(short int nome, short int sobrenome, short int identificador, short int senha);
void pausarPrograma();
void finalizarPrograma();
void fecharArquivo();

/**
 * Estrutura para organização dos dados do usuário
 */
struct Usuario
{
    char nome[MAX_DADOS];                  //Guarda o nome do usuário (uma palavra)
    char sobrenome[MAX_DADOS];             //Guarda o sobrenome do usuário (uma palavra)
    char identificador[MAX_IDENTIFICADOR]; //Guarda o identificador/login do usuário
    char senha[MAX_SENHA];                 //Guarda a senha do usuário
};

void enviarDadosServidor(char *msgCliente)
{
    if (write(socketDescritor, msgCliente, strlen(msgCliente)) < 0)
    {
        printf("Erro ao enviar dados para o servidor\n");
    }
}

char *receberDadosServidor()
{
    int tamanho;
    //Lê/recebe resposta do servidor
    if ((tamanho = read(socketDescritor, msgServidor, MAX_MSG)) < 0)
    {
        puts("Falha ao receber resposta.");
        return "-1";
    }

    msgServidor[tamanho] = '\0'; //Adiciona fim de linha na string
    // puts(msg_servidor);
    return msgServidor;
}

void finalizarConexao()
{
    close(socketDescritor); //Fecha o socket
    puts("> Cliente desconectado com sucesso.");
}

/**
 * Função principal
 */
int main()
{
    setlocale(LC_ALL, "Portuguese"); //Permite a utilização de acentos e caracteres do português nas impressões

    /******* CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/
    puts("> INICIANDO CLIENTE...");
    pausarPrograma();
    system("clear");

    struct sockaddr_in servidor; //Estrutura que guarda dados do servidor que irá se conectar

    //Cria o socket do cliente
    if ((socketDescritor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("# ERRO - Criação do socket falhou:");
        return EXIT_FAILURE;
    }
    printf("> Socket do cliente criado com descritor: %d\n", socketDescritor);

    //Atribuindo informações das configurações do servidor, na sua estrutura
    servidor.sin_addr.s_addr = inet_addr(ENDERECO_SERVIDOR); //Endereço IP
    servidor.sin_family = AF_INET;                           //Para comunicação IPV4
    servidor.sin_port = htons(PORTA);                        //Porta de conexão do servidor

    //Conecta no servidor
    if (connect(socketDescritor, (struct sockaddr *)&servidor, sizeof(servidor)) == -1)
    {
        perror("# ERRO - Não foi possível conectar no servidor: ");
        return EXIT_FAILURE;
    }
    /******* FIM-CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/

    imprimirDecoracao();
    printf("\n\t\t>> CONECTADO AO SERVIDOR <<\n");
    int operacao = 0; //Recebe um número que o usuário digitar para escolher a opção do menu
    char operacaoString[3];

    //Loop do menu de opções
    do
    {
        pausarPrograma();
        operacao = '\0'; //Limpar a variável para evitar lixo de memória nas repetições

        system("cls || clear");
        imprimirDecoracao();
        printf("\n\t\t>> MENU DE OPERAÇÕES <<\n");
        printf("\n> Informe um número para escolher uma opção e pressione ENTER:");
        printf("\n[1] Login");
        printf("\n[2] Cadastro");
        printf("\n[0] Encerrar programa");
        imprimirDecoracao();
        printf("\n> Informe o número: ");

        setbuf(stdin, NULL);
        //Valida a entrada que o usuário digitou
        while (scanf("%d", &operacao) != 1)
        {
            printf("\n# FALHA - Ocorreu um erro na leitura do valor informado.\n> Tente novamente: ");
            setbuf(stdin, NULL);
        };

        sprintf(operacaoString, "%d", operacao);
        enviarDadosServidor(operacaoString);

        system("cls || clear");

        if (operacao == 0)
        {
            finalizarConexao();
            break;
        }

        //Escolhe a operação conforme o valor que o usuário digitou
        switch (operacao)
        {
        case 1: //Login
            imprimirDecoracao();
            printf("\n\t\t\t>> AUTENTICAÇÃO <<\n\n");
            imprimirDecoracao();
            if (autenticar())
            {
                puts("# SUCESSO - Você entrou e saiu da área logada!");
                // limparEstruturaUsuario();
            }
            break;
        case 2: //Cadastro
            imprimirDecoracao();
            printf("\n\t\t\t>> CADASTRO <<\n\n");
            imprimirDecoracao();
            cadastrarUsuario();
            break;
        default:
            printf("\n# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida, tente novamente!\n");
            break;
        }
    } while (1);

    return EXIT_SUCCESS;
}

/**
 * Função para cadastrar novo usuário, solicita todos os dados ao usuário e insere no arquivo de dados dos usuários
 */
void cadastrarUsuario()
{
    printf("\n> Forneça as informações necessárias para efetuar o cadastro:\n");

    char entradaTemp[MAX];                           //Variável para fazer a entrada de valores digitados e realizar a validação antes de atribuir à variável correta
    memset(&entradaTemp[0], 0, sizeof(entradaTemp)); //Limpando a variável para garantir que não entre lixo de memória

    //Solicita o nome do usuário
    memset(&msgServidor[0], 0, sizeof(msgServidor));
    printf("\n> Informe seu primeiro nome: ");
    //Loop para validação do nome, enquanto a função que valida a string retornar 0 (falso) o loop vai continuar (há uma negação na frente do retorno da função)
    do
    {
        puts(msgServidor);
        setbuf(stdin, NULL);          //Limpa a entrada par evitar lixo
        scanf("%[^\n]", entradaTemp); //Leitura do teclado, o %[^\n] vai fazer com que o programa leia tudo que o usuário digitar exceto ENTER (\n), por padrão pararia de ler no caractere espaço
        enviarDadosServidor(entradaTemp);
        strcpy(msgServidor, receberDadosServidor());
    } while (msgServidor[0] == '#');
    //Se sair do loop é porque a string é válida e pode ser copiada para a variável correta que irá guardar
    strcpy(u.nome, entradaTemp);
    //Limpar a variável temporária para receber a próxima entrada
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Solicita o sobrenome do usuário
    memset(&msgServidor[0], 0, sizeof(msgServidor));
    printf("\n> Informe seu último sobrenome: ");
    do
    {
        puts(msgServidor);
        setbuf(stdin, NULL);
        scanf("%[^\n]", entradaTemp);
        enviarDadosServidor(entradaTemp);
        strcpy(msgServidor, receberDadosServidor());
    } while (msgServidor[0] == '#');
    strcpy(u.sobrenome, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Solicita que o usuário crie um identificador
    memset(&msgServidor[0], 0, sizeof(msgServidor));
    printf("\n> Crie seu login: ");
    do
    {
        puts(msgServidor);
        setbuf(stdin, NULL);
        scanf("%[^\n]", entradaTemp);
        enviarDadosServidor(entradaTemp);
        strcpy(msgServidor, receberDadosServidor());
    } while (msgServidor[0] == '#');
    strcpy(u.identificador, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Solicita o usuário crie uma senha
    memset(&msgServidor[0], 0, sizeof(msgServidor));
    do
    {
        puts(msgServidor);
        setbuf(stdin, NULL);
        //getpass() é usado para desativar o ECHO do console e não exibir a senha sendo digitada pelo usuário, retorna um ponteiro apontando para um buffer contendo a senha, terminada em '\0' (NULL)
        strcpy(entradaTemp, getpass("> Crie uma senha: "));
        enviarDadosServidor(entradaTemp);
        strcpy(msgServidor, receberDadosServidor());
    } while (msgServidor[0] == '#');
    strcpy(u.senha, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    limparEstruturaUsuario();
    puts(receberDadosServidor());
}

/**
 * Realizar a autenticação do usuário
 * @param ehLogin deve ser passado como 1 caso a chamada da função está sendo realizada para login e não somente autenticação, 
 * quando for login a função vai definir os dados do usuário, trazidos do arquivo, na estrutura
 * @return 1 em caso de sucesso e; 0 em outros casos
 */
short int autenticar()
{
    do
    {
        /*Coleta do login e senha para autenticação*/
        printf("\n> Informe suas credenciais:\n# LOGIN: ");
        setbuf(stdin, NULL);              //Limpa o buffer de entrada par evitar lixo
        scanf("%[^\n]", u.identificador); //Realiza a leitura até o usuário pressionar ENTER
        enviarDadosServidor(u.identificador);
        strcpy(u.senha, getpass("# SENHA: ")); //Lê a senha com ECHO do console desativado e copia o valor lido para a variável u.senha, do usuário
        enviarDadosServidor(u.senha);

        strcpy(msgServidor, receberDadosServidor());
        puts(msgServidor);
        if (msgServidor[0] == '#')
        {
            puts("\n<?> Voltar ao menu principal? [s/n]: ");
            setbuf(stdin, NULL);
            if (getchar() == 's')
            {
                enviarDadosServidor("1");
                setbuf(stdin, NULL);
                return 0;
            }
            // setbuf(stdin, NULL);
            getchar();
            // setbuf(stdin, NULL);
            enviarDadosServidor("0");
        }
        else if (msgServidor[0] == '>')
        {
            return 1;
        }
        else
        {
            puts("# ERRO FATAL - O servidor enviou uma mensagem desconhecida.\n");
            finalizarConexao();
            finalizarPrograma();
        }

    } while (1);

    return 0;
}

/**
 * Zera os dados da estrutura do usuário para reutilização
 */
void limparEstruturaUsuario()
{
    memset(&u.nome[0], 0, sizeof(u.nome));
    memset(&u.sobrenome[0], 0, sizeof(u.sobrenome));
    memset(&u.identificador[0], 0, sizeof(u.identificador));
    memset(&u.senha[0], 0, sizeof(u.senha));
}

/**
 * Apenas imprime as linhas de separação
 */
void imprimirDecoracao()
{
    printf("\n_____________________________________________________________________");
    printf("\n*********************************************************************\n");
}

/**
 * Pausa no programa para que o usuário possa ler mensagens antes de limpar a tela
 */
void pausarPrograma()
{
    printf("\nPressione ENTER para continuar...\n");
    //Limpa o buffer do teclado para evitar que lixo de memória (geralmente ENTER) seja lido ao invés da entrada do usuário
    setbuf(stdin, NULL);
    //Apenas uma pausa para que as informações fiquem na tela
    getchar();
    //Limpa o ENTER digitado no getchar()
    setbuf(stdin, NULL);
}

/**
 * Limpa a estrutura com os dados do usuário e encerra o programa
 */
void finalizarPrograma()
{
    setbuf(stdin, NULL);
    limparEstruturaUsuario();
    printf("\n# SISTEMA FINALIZADO.\n");
    exit(0);
}
