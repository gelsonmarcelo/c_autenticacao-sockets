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
//Tamanho da mensagem trocada entre cliente/servidor
#define MAX_MSG 1024
//Porta de conexão onde o servidor está ouvindo
#define PORTA 3344
//Endereço IP que se encontra o servidor
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
void pausarPrograma();
void finalizarPrograma();
void fecharArquivo();
void enviarDadosServidor(char *idSincronia, char *dadosCliente);
char *receberDadosServidor();
void finalizarConexao();
void areaLogada();

/**
 * Estrutura para organização dos dados do usuário
 */
struct Usuario
{
    //### Ver quais não são usados para eliminar
    int codigo;                            //Mantém o código (ID do arquivo de dados) do usuário
    char nome[MAX_DADOS];                  //Guarda o nome do usuário (uma palavra)
    char sobrenome[MAX_DADOS];             //Guarda o sobrenome do usuário (uma palavra)
    char email[MAX_DADOS];                 //Guarda o e-mail do usuário
    char identificador[MAX_IDENTIFICADOR]; //Guarda o identificador/login do usuário
    char salt[SALT_SIZE + 1];              //Guarda o salt aleatório gerado
    char senha[MAX_SENHA];                 //Guarda a senha do usuário
    char *senhaCriptografada;              //Ponteiro para guardar o valor da senha criptografada, precisa ser ponteiro para receber o retorno da função crypt
    char linhaUsuario[MAX];                //Essa string é utilizada para encontrar a linha do usuário no arquivo
};

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
        finalizarPrograma();
    }
    printf("> Socket do cliente criado com descritor: %d\n", socketDescritor);

    //Atribuindo informações das configurações do servidor, na sua estrutura
    servidor.sin_addr.s_addr = inet_addr(ENDERECO_SERVIDOR); //Endereço IP
    servidor.sin_family = AF_INET;                           //Para comunicação IPV4
    servidor.sin_port = htons(PORTA);                        //Porta de conexão do servidor

    //Conecta no servidor
    if (connect(socketDescritor, (struct sockaddr *)&servidor, sizeof(servidor)) == -1)
    {
        perror("# ERRO - Não foi possível conectar no servidor");
        finalizarPrograma();
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
        printf("\n[3] Ver política de identificadores e senhas");
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
        enviarDadosServidor("op", operacaoString);

        system("cls || clear");

        if (operacao == 0)
        {
            finalizarConexao();
            finalizarPrograma();
        }

        //Escolhe a operação conforme o valor que o usuário digitou
        switch (operacao)
        {
        case 1: //Login
            imprimirDecoracao();
            printf("\n\t\t>> AUTENTICAÇÃO <<\n");
            imprimirDecoracao();
            if (autenticar())
            {
                areaLogada();
                limparEstruturaUsuario();
            }
            break;
        case 2: //Cadastro
            imprimirDecoracao();
            printf("\n\t\t>> CADASTRO <<\n");
            imprimirDecoracao();
            cadastrarUsuario();
            break;
        case 3: //Ver política
            imprimirDecoracao();
            printf("\n\t>> POLÍTICA DE IDENTIFICADORES E SENHAS <<\n");
            imprimirDecoracao();
            puts(receberDadosServidor("pt"));
            break;
        default:
            printf("\n# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida, tente novamente!\n");
            break;
        }
    } while (1);

    return EXIT_SUCCESS;
}

/**
 * Concatena o id de sincronização à mensagem e escreve no socket a ser lido pelo servidor
 * @param idSincronia é o identificador que será verificado no servidor para saber se a mensagem recebida é um dado esperado
 * @param dadosCliente é a string com dados que o cliente precisa enviar
 */
void enviarDadosServidor(char *idSincronia, char *dadosCliente)
{
    char *msgCliente = malloc(MAX_MSG);                      //Armazena a mensagem a ser enviada contendo id e dados
    sprintf(msgCliente, "%s/%s", idSincronia, dadosCliente); //Insere o id de sincronização e dados em formato específico na mensagem

    //Verificar o tamanho da string a ser enviada
    if (strlen(msgCliente) > MAX_MSG)
    {
        printf("# ERRO FATAL - Os dados que pretende enviar ao servidor excederam o limite de caracteres, envio cancelado.\n");
        //### - Ao invéz de finalizar o programa, solicitar que o usuário informe novamente o dado e validar o tamanho.
        finalizarConexao();
        finalizarPrograma();
    }

    //Escreve no socket e valida se não houveram erros
    if (write(socketDescritor, msgCliente, strlen(msgCliente)) < 0)
    {
        printf("# ERRO - Falha ao enviar dados para o servidor\n");
        return;
    }
    // printf("\n§ msgEnviada: %s\n", msgCliente);
    free(msgCliente); //Libera a memória alocada
}

/**
 * Lê dados escritos pelo servidor no socket de comunicação
 * @param idSincronia é o identificador que será comparado com a mensagem recebida do cliente, para saber se a mensagem é um dado esperado
 * @return a mensagem recebida do servidor
 */
char *receberDadosServidor(char *idSincronia)
{
    int tamanho; //Guarda o tamanho da mensagem recebida pelo servidor
    char *msgServidorLocal = malloc(MAX_MSG);

    //Lê resposta do servidor e valida se não houveram erros
    if ((tamanho = read(socketDescritor, msgServidorLocal, MAX_MSG)) < 0)
    {
        perror("# ERRO - Falha ao receber resposta");
        finalizarConexao();
        finalizarPrograma();
    }
    msgServidorLocal[tamanho] = '\0'; //Adiciona NULL no final da string

    // printf("\n§ msgCliente Recebida: %s\n", msgServidorLocal);

    //Separa o identificador de sincronização e compara se era esse identificador que o programa estava esperando
    if (strcmp(strsep(&msgServidorLocal, "/"), idSincronia))
    {
        //Se não for, houve algum problema na sincronização cliente/servidor e eles não estão comunicando corretamente
        puts("\n# ERRO FATAL - Servidor e cliente não estão sincronizados...");
        finalizarConexao();
        finalizarPrograma();
    }
    return msgServidorLocal; //Retorna somente a parte da mensagem recebida, após o /
}

/**
 * Encerra a comunicação fechando o socket
 */
void finalizarConexao()
{
    imprimirDecoracao();

    //Fecha o socket
    if (close(socketDescritor))
    {
        perror("# ERRO - Não foi possível fechar a comunicação");
        return;
    }
    puts("> INFO - Você foi desconectado.");
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
        enviarDadosServidor("rp", entradaTemp);
        strcpy(msgServidor, receberDadosServidor("rp"));
    } while (msgServidor[0] == '#');
    //Se sair do loop é porque a string é válida e pode ser copiada para a variável correta que irá guardar
    // strcpy(u.nome, entradaTemp);
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
        enviarDadosServidor("ru", entradaTemp);
        strcpy(msgServidor, receberDadosServidor("ru"));
    } while (msgServidor[0] == '#');
    // strcpy(u.sobrenome, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Solicita o e-mail do usuário
    memset(&msgServidor[0], 0, sizeof(msgServidor));
    printf("\n> Informe seu e-mail: ");
    do
    {
        puts(msgServidor);
        setbuf(stdin, NULL);
        scanf("%[^\n]", entradaTemp);
        enviarDadosServidor("re", entradaTemp);
        strcpy(msgServidor, receberDadosServidor("re"));
    } while (msgServidor[0] == '#');
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Solicita que o usuário crie um identificador
    memset(&msgServidor[0], 0, sizeof(msgServidor));
    printf("\n> Crie seu login: ");
    do
    {
        puts(msgServidor);
        setbuf(stdin, NULL);
        scanf("%[^\n]", entradaTemp);
        enviarDadosServidor("ri", entradaTemp);
        strcpy(msgServidor, receberDadosServidor("ri"));
    } while (msgServidor[0] == '#');
    // strcpy(u.identificador, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Solicita o usuário crie uma senha
    memset(&msgServidor[0], 0, sizeof(msgServidor));
    do
    {
        puts(msgServidor);
        setbuf(stdin, NULL);
        //getpass() é usado para desativar o ECHO do console e não exibir a senha sendo digitada pelo usuário, retorna um ponteiro apontando para um buffer contendo a senha, terminada em '\0' (NULL)
        strcpy(entradaTemp, getpass("> Crie uma senha: "));
        enviarDadosServidor("rs", entradaTemp);
        strcpy(msgServidor, receberDadosServidor("rs"));
        //Confirmação de senha
        if (msgServidor[0] == '*')
        {
            strcpy(entradaTemp, getpass("\n> Confirme a senha: "));
            enviarDadosServidor("rs", entradaTemp);
            strcpy(msgServidor, receberDadosServidor("rs"));
        }
    } while (msgServidor[0] == '#');
    // strcpy(u.senha, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    limparEstruturaUsuario();
    puts(receberDadosServidor("rf"));
}

/**
 * Realizar a autenticação do usuário
 * @param ehLogin deve ser passado como 1 caso a chamada da função está sendo realizada para login e não somente autenticação, 
 * quando for login a função vai definir os dados do usuário, trazidos do arquivo, na estrutura
 * @return 1 em caso de sucesso e; 0 em outros casos
 */
short int autenticar()
{
    int repetir = 0;
    char *msgServidor = malloc(MAX_MSG);
    do
    {
        /*Coleta do login e senha para autenticação*/
        printf("\n> Informe suas credenciais:\n# LOGIN: ");
        setbuf(stdin, NULL);              //Limpa o buffer de entrada par evitar lixo
        scanf("%[^\n]", u.identificador); //Realiza a leitura até o usuário pressionar ENTER
        enviarDadosServidor("li", u.identificador);
        strcpy(u.senha, getpass("# SENHA: ")); //Lê a senha com ECHO do console desativado e copia o valor lido para a variável u.senha, do usuário
        enviarDadosServidor("ls", u.senha);
        imprimirDecoracao();

        strcpy(msgServidor, receberDadosServidor("au"));
        if (msgServidor[0] == '#')
        {
            puts(msgServidor);
            puts("\n<?> Tentar novamente? [1]Sim / [0]Não:");
            while (scanf("%d", &repetir) != 1)
            {
                puts("# Digite [1] para autenticar novamente e [0] para voltar para o menu inicial: ");
                setbuf(stdin, NULL);
            }
            if (repetir == 1)
            {
                enviarDadosServidor("cm", "1");
            }
            else
            {
                enviarDadosServidor("cm", "0");
                return 0;
            }
        }
        else if (msgServidor[0] == '>')
        {
            puts("\n> SUCESSO - Login realizado!");
            if (sscanf(msgServidor, ">%d|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^\n]", &u.codigo, u.identificador, u.salt, &u.senhaCriptografada, u.nome, u.sobrenome, u.email) != 7)
            {
                printf("\n# FALHA: Houve um problema ao receber os dados do servidor\n");
                return 0;
            }
            strcpy(u.linhaUsuario, msgServidor);
            // printf("§ %s", u.linhaUsuario);
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
 * Opções para o usuário autenticado
 */
void areaLogada()
{
    int operacao = 0; //Recebe o valor da entrada do usuário para escolher a operação
    char operacaoString[3];
    imprimirDecoracao();
    printf("\n\t\tBEM-VINDO %s!\n", u.nome);

    //Loop do menu de opções do usuário logado
    do
    {
        pausarPrograma();
        operacao = '\0'; //Limpar a variável para evitar lixo de memória nas repetições

        system("cls || clear");
        imprimirDecoracao();
        printf("\t\t> MENU DE OPERAÇÕES <\n\n*Autenticado como %s", u.nome);
        imprimirDecoracao();
        printf("\n> Informe um número para escolher uma opção e pressione ENTER:");
        printf("\n___________________________________");
        printf("\n[0] ENCERRAR PROGRAMA");
        printf("\n[1] LOGOUT");
        printf("\n[2] EXCLUIR MEU CADASTRO");
        printf("\n___________________________________");
        printf("\n[3] Ver meus dados");
        printf("\n[4] Ver política de identificadores e senhas");
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
        enviarDadosServidor("ap", operacaoString);

        system("cls || clear");

        if (operacao == 0)
        {
            limparEstruturaUsuario();
            finalizarConexao();
            finalizarPrograma();
        }

        //Escolhe a operação conforme o valor que o usuário digitou
        switch (operacao)
        {
        case 1: //Logout
            system("clear || cls");
            printf("\n# LOGOUT - Você saiu\n");
            imprimirDecoracao();
            return;
        case 2: //Excluir Cadastro
            imprimirDecoracao();
            printf("\n\t\t>> EXCLUIR CADASTRO <<\n");
            imprimirDecoracao();
            printf("\n# AVISO - Tem certeza que deseja excluir seu cadastro? Essa ação não pode ser desfeita! [s/n]\n> ");
            setbuf(stdin, NULL);
            if (getchar() != 's')
            {
                setbuf(stdin, NULL);
                getchar();
                enviarDadosServidor("cd", "0");
                puts("\n# OPERAÇÃO CANCELADA\n");
                break;
            }
            setbuf(stdin, NULL);
            getchar();
            enviarDadosServidor("cd", "1");
            puts(receberDadosServidor("dl"));
            limparEstruturaUsuario();
            return;
        case 3: //Ver dados do usuário
            imprimirDecoracao();
            printf("\n\t\t>> MEUS DADOS <<\n");
            puts(receberDadosServidor("du"));
            imprimirDecoracao();
            break;
        case 4: //Cadastro
            imprimirDecoracao();
            printf("\n\t>> VER POLÍTICA DE IDENTIFICADORES E SENHAS <<\n");
            imprimirDecoracao();
            puts(receberDadosServidor("pt"));
            break;
        default:
            puts("\n# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida, tente novamente!\n");
            break;
        }
    } while (1);
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
 * ### Zera os dados da estrutura do usuário para reutilização
 */
void limparEstruturaUsuario()
{
    memset(&u.nome[0], 0, sizeof(u.nome));
    memset(&u.sobrenome[0], 0, sizeof(u.sobrenome));
    memset(&u.email[0], 0, sizeof(u.email));
    memset(&u.identificador[0], 0, sizeof(u.identificador));
    memset(&u.salt[0], 0, sizeof(u.salt));
    memset(&u.senha[0], 0, sizeof(u.senha));
    // memset(&u.senhaCriptografada[0], 0, sizeof(u.senhaCriptografada));
    memset(&u.linhaUsuario[0], 0, sizeof(u.linhaUsuario));
    u.codigo = '0';
}

/**
 * Limpa a estrutura com os dados do usuário e encerra o programa
 */
void finalizarPrograma()
{
    imprimirDecoracao();
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    printf("# SISTEMA FINALIZADO.\n");
    exit(0);
}
