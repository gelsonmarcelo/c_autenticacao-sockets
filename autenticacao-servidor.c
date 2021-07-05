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
//Tamanho da mensagem trocada entre cliente/servidor
#define MAX_MSG 1024
//Porta de conexão onde o servidor ouve
#define PORTA 3344

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

FILE *ponteiroArquivos = NULL;        //Ponteiro para manipular o arquivo
char temp[MAX];                       //Variável temporária para gravação de valores que não serão utilizados mas precisam ser colocados em alguma variável
char arquivoUsuarios[] = "dados.txt"; //String com nome do arquivo de dados
int conexao;                          //Guarda o retorno de accept, um arquivo descritor do socket conectado

struct Usuario u; //Declaração da estrutura do usuário

/*Declaração das funções*/

char *receberDadosCliente(char *idSincronia, char *dadoEsperado);
void enviarDadosCliente(char *idSincronia, char *dadosServidor);
short int autenticar();
void cadastrarUsuario();
void coletarDados();
int pegarProximoId(char *arquivo, char *idSincronia);
short int validarStringPadrao(char *string, char *idSincronia);
short int validarStringEmail(char *string, char *idSincronia);
short int validarIdentificador(char *identificador, char *idSincronia);
short int validarSenha(char *senha, char *idSincronia);
char *alternarCapitalLetras(char *string, int flag);
void gerarSalt();
void criptografarSenha();
short int testarArquivo(char *nomeArquivo, char *idSincronia);
void fecharArquivo(FILE *arquivo, char *idSincronia);
short int areaLogada();
void verDadosUsuario();
short int excluirUsuario();
void mostrarPoliticaSenhas();
void imprimirDecoracao();
void pausarPrograma();
void limparEstruturaUsuario();
void finalizar(short int comunicacao, short int programa);

/**
 * Estrutura para organização dos dados do usuário
 */
struct Usuario
{
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
    setlocale(LC_ALL, "Portuguese"); //Permite a utilização de acentos e caracteres do português nas impressões do console

    puts("> INICIANDO SERVIDOR...");
    pausarPrograma();
    system("clear");

    char maximoMsg[sizeof(int) + 1];   //Comprimento máximo da troca de mensagens entre cliente/servidor em formato string para ser enviado para o cliente que se conectar
    sprintf(maximoMsg, "%d", MAX_MSG); //Convertendo valor do comprimeiro máximo das mensagens e adicionando na string
    int operacao;                      //Recebe um número que o cliente enviou para escolher a operação do menu

    //Verifica se é possível manipular o arquivo de dados dos usuários
    if (testarArquivo(arquivoUsuarios, ""))
    {
        printf("# ERRO FATAL - O arquivo de dados não pode ser manipulado, o servidor não pode ser iniciado.\n");
        finalizar(0, 1);
    }

    /******* CONFIGURAÇÕES DO SOCKET *******/
    int socketDescritor;                       //Descritor do socket
    struct sockaddr_in servidor, cliente;      //Estruturas de cliente e servidor, para configuração
    socklen_t clienteLenght = sizeof(cliente); //Salva comprimento da struct do cliente

    //Cria um socket(domínio: IPv4, Tipo: TCP, default protocolo) e valida
    if ((socketDescritor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("# ERRO FATAL - Falha ao criar socket");
        finalizar(0, 1);
    }

    //Define as propriedades da struct do socket
    servidor.sin_family = AF_INET;         //Para comunicação IPV4
    servidor.sin_addr.s_addr = INADDR_ANY; //Obtém IP do Sistema Operacional
    servidor.sin_port = htons(PORTA);      //Porta de conexão do servidor

    //Trata o erro de porta já está em uso
    /* int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen); */
    if (setsockopt(socketDescritor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1)
    {
        perror("# ERRO FATAL - Erro na porta do socket");
        finalizar(0, 1);
    }

    //Associa o socket à porta
    if (bind(socketDescritor, (struct sockaddr *)&servidor, sizeof(servidor)) == -1)
    {
        perror("# ERRO FATAL - Erro ao fazer bind");
        finalizar(0, 1);
    }

    //Aguarda conexões de clientes
    if (listen(socketDescritor, 1) == -1)
    {
        perror("# ERRO FATAL- Erro ao ouvir conexões");
        finalizar(0, 1);
    }
    /******* FIM - CONFIGURAÇÃO DO SOCKET *******/
    //Loop do recebimento e tratativa das conexões
    do
    {
        NOVA_CONEXAO:
        imprimirDecoracao();
        puts("> SERVIDOR ONLINE...");
        printf("> Ouvindo na porta %d.", PORTA);
        puts("\n> Aguardando por conexões...");

        //Aceita a solicitação de conexão do cliente
        if ((conexao = accept(socketDescritor, (struct sockaddr *)&cliente, &clienteLenght)) == -1)
        {
            perror("# ERRO FATAL - Falha ao aceitar conexão");
            finalizar(0, 1);
        }
        imprimirDecoracao();
        printf("\n>> >> CLIENTE CONECTADO >> >>\n");
        printf("> Cliente IP:PORTA = %s:%d\n", inet_ntoa(cliente.sin_addr), ntohs(cliente.sin_port)); //Obtém IP e porta do cliente
        puts("> Enviando comprimento máximo da troca de mensagens do servidor...");
        enviarDadosCliente("mx", maximoMsg); //Envia para o cliente que se conectou, o comprimento máximo de leitura/escrita das mensagens pelo servidor

        operacao = 0;

        //Loop do menu de operações
        do
        {
            imprimirDecoracao();
            puts("\t> MENU DE OPERAÇÕES: NÃO AUTENTICADO <");
            //operacao recebe os dados do cliente, sendo convertidos para int, os dados que chegam aqui já foram validados como inteiros pela aplicação cliente
            operacao = atoi(receberDadosCliente("op", "operação do menu"));

            //Usuário solicitou encerrar o cliente (consequentemente a conexão)
            if (operacao == 0)
            {
                finalizar(1, 0);
                break;
            }

            imprimirDecoracao();
            //Escolhe a operação conforme o valor que o usuário informou
            switch (operacao)
            {
            case 1: //Autenticação
                printf("\t\t>> AUTENTICAÇÃO <<\n");
                if (autenticar())
                {
                    if (areaLogada())
                        goto NOVA_CONEXAO;
                }
                break;
            case 2: //Cadastro
                printf("\t\t>> CADASTRO <<\n");
                cadastrarUsuario();
                break;
            case 3: //Ver política
                printf("\t>> POLÍTICA DE IDENTIFICADORES E SENHAS <<\n");
                mostrarPoliticaSenhas();
                break;
            default:
                puts("\n# INFO - Usuário enviou um comando inválido");
                break;
            }
        } while (1);
        /* ***Código caso deseje parar o servidor a cada conexão encerrada***
        imprimirDecoracao();
        puts("<?> Deseja finalizar o servidor [s/n]?");
        if (getchar() == 's')
        {
            getchar();
            setbuf(stdin, NULL);
            finalizar(0, 1);
        }
        getchar();
        setbuf(stdin, NULL);*/
    } while (1);
    return EXIT_SUCCESS;
}

/**
 * Lê a mensagem escrita pelo cliente no socket de comunicação, separa e verifica o id de sincronização e retorna a mensagem recebida
 * @param idSincronia é um identificador com 2 letras, definido em ambas mensagem de envio (cliente) e recebimento (servidor) para verificar se a mensagem recebida é a 
 * informação que o programa esperava, isso define se a aplicação cliente e servidor estão sincronizadas e comunicando de forma adequada
 * @param dadoEsperado apenas um breve descritivo para que possa ser acompanhado, no console do servidor, qual dado ele está aguardando do cliente
 * @return a mensagem recebida do cliente sem o id de sincronização
 */
char *receberDadosCliente(char *idSincronia, char *dadoEsperado)
{
    int comprimentoMsg;                 //Guarda o comprimeiro da mensagem recebida pelo cliente
    char *msgCliente = malloc(MAX_MSG); //Guarda a mensagem recebida

    printf("> Aguardando: %s...\n", dadoEsperado);

    //Lê dados escritos pelo cliente no socket e valida
    if ((comprimentoMsg = read(conexao, msgCliente, MAX_MSG)) < 0)
    {
        perror("# ERRO FATAL - Falha ao receber dados do cliente");
        finalizar(1, 1);
    }
    msgCliente[comprimentoMsg] = '\0'; //Adiciona NULL no final da string recebida
    
    // printf("\n§ msgCliente recebida: %s\n", msgCliente);

    //Separa o identificador de sincronização e compara se era essa mensagem que o programa esperava receber
    if (strcmp(strsep(&msgCliente, "/"), idSincronia))
    {
        //Se não for, houve algum problema na sincronização cliente/servidor e eles não estão comunicando corretamente
        puts("\n# ERRO FATAL - Servidor e cliente não estão sincronizados...");
        finalizar(1, 1);
    }
    return msgCliente; //Retorna somente a parte da mensagem recebida, após o /
}

/**
 * Escreve uma mensagem no socket a ser lido pelo cliente
 * @param idSincronia é um identificador com 2 letras, definido em ambas mensagem de envio (servidor) e recebimento (cliente) para verificar se a mensagem recebida é a 
 * informação que o programa esperava, isso define se a aplicação cliente e servidor estão sincronizadas e comunicando de forma adequada
 * @param dadosServidor é a string com dados que o servidor precisa enviar ao cliente
 */
void enviarDadosCliente(char *idSincronia, char *dadosServidor)
{
    char *msgServidor = malloc(MAX_MSG); //Armazena a mensagem a ser enviada, contendo id e dados

    //Validar o comprimento da string a ser enviada, a mensagem não pode ser maior do que MAX_MSG menos o comprimento do ID de sincronia
    if (strlen(dadosServidor) > (MAX_MSG - strlen(idSincronia)))
    {
        //Se ultrapassar o comprimento máximo que o cliente pode ler, cancela o envio abortando o programa
        printf("# ERRO FATAL - Os dados que o servidor tentou enviar excederam o limite de caracteres, envio cancelado.\n");
        finalizar(1, 1);
    }

    sprintf(msgServidor, "%s/%s", idSincronia, dadosServidor); //Insere o id de sincronização e dados em formato específico, juntos, na mensagem

    //Escreve no socket de comunicação validando se não houve erro
    if (write(conexao, msgServidor, strlen(msgServidor)) < 0)
    {
        perror("# ERRO - Falha ao enviar mensagem");
    }
    else
    {
        printf("\n> Mensagem enviada para o cliente:\n\"%s\"\n\n", msgServidor);
    }
    free(msgServidor); //Libera a memória alocada
}

/**
 * Encerra o programa e/ou a comunicação com o cliente conectado, dependendo da flag ativada
 * @param conexao defina 1 se a conexão com o cliente atual deve ser encerrada
 * @param programa defina 1 se o programa deve ser finalizado
 */
void finalizar(short int comunicacao, short int programa)
{
    if (comunicacao)
    {
        imprimirDecoracao();
        //Solicita desativação da conexão com o cliente, impedindo recebimento e transmissões posteriores (SHUT_RDWR).
        if (shutdown(conexao, SHUT_RDWR))
        {
            perror("# ERRO - Falha ao desativar comunicação");
        }
        //Fecha o socket
        if (close(conexao))
        {
            perror("# ERRO - Não foi possível fechar a comunicação");
        }
        else
        {
            puts("\n<< << CLIENTE DESCONECTADO << <<");
        }
    }

    if (programa)
    {
        imprimirDecoracao();
        setbuf(stdin, NULL);
        setbuf(stdout, NULL);
        printf("# SISTEMA FINALIZADO.\n");
        exit(0);
    }
}

/**
 * Busca no arquivo o último ID cadastrado e retorna o próximo ID a ser usado
 * @param arquivo é o char com nome do arquivo que deseja utilizar para verificar o próximo ID
 * @param idSincronia deve ser passado o id de sincronização para que seja enviada a mensagem para o cliente em caso de erro
 * @return valor do próximo ID a ser usado; 0 em caso de falha ao abrir o arquivo para leitura, 
 * retorna 1 se não conseguir ler qualquer ID anterior no arquivo
 */
int pegarProximoId(char *arquivo, char *idSincronia)
{
    //Teste do arquivo
    if (testarArquivo(arquivo, idSincronia))
        return 0;

    int id = 0;

    //Abre o arquivo
    ponteiroArquivos = fopen(arquivo, "r");

    while (!feof(ponteiroArquivos))
    {
        //Lê as linhas até o final do arquivo, atribuindo o id da linha na variável id com formato inteiro
        fscanf(ponteiroArquivos, ">%d|%[^\n]\n", &id, temp);
    }
    fecharArquivo(ponteiroArquivos, idSincronia);

    //O ID lido por último é o último ID cadastrado e será somado + 1 e retornado para cadastrar o próximo
    return id + 1;
}

/**
 * Função para cadastrar novo usuário, chama função para solicitar todos os dados ao usuário e insere no arquivo de dados dos usuários
 */
void cadastrarUsuario()
{
    /*Validação para, caso o arquivo não possa ser aberto, interromper antes de pedir todos os dados ao usuário
    e Define o ID do usuário que está se cadastrando, se o ID retornar 0 houve algum problema na função e interrompe o cadastro*/
    if (testarArquivo(arquivoUsuarios, "fi") || !(u.codigo = pegarProximoId(arquivoUsuarios, "fi")))
        return;
    enviarDadosCliente("fi", "> SUCESSO: Teste do arquivo de dados aprovado e ID do novo usuário obtido com sucesso");

    printf("\n> Usuário deve fornecer as informações necessárias para efetuar o cadastro:\n");
    coletarDados();

    //Abrir o arquivo com parâmetro "a" de append, não sobrescreve as informações, apenas adiciona
    ponteiroArquivos = fopen(arquivoUsuarios, "a");

    gerarSalt();
    criptografarSenha(u.senha);

    //Passar dados recebidos para a linha do usuário no formato que irão para o arquivo
    sprintf(u.linhaUsuario, ">%d|%s|%s|%s|%s|%s|%s\n", u.codigo, u.identificador, u.salt, u.senhaCriptografada, u.nome, u.sobrenome, u.email);

    //Insere a string com todos os dados no arquivo e valida se não ocorreram erros
    if (fputs(u.linhaUsuario, ponteiroArquivos) == EOF)
    {
        enviarDadosCliente("rf", "# ERRO - Houve algum problema para inserir os dados no arquivo");
    }
    else
    {
        enviarDadosCliente("rf", "> SUCESSO - Cadastro realizado: faça login para acessar o sistema");
    }

    fecharArquivo(ponteiroArquivos, "rf"); //Fecha o arquivo
}

/**
 * Receber dados de cadastro do usuário
 */
void coletarDados()
{
    char entradaTemp[MAX];                           //Variável para fazer a entrada de valores digitados e realizar a validação antes de atribuir à variável correta
    memset(&entradaTemp[0], 0, sizeof(entradaTemp)); //Limpando a variável para garantir que não entre lixo de memória

    /*Recebe e processa o nome do usuário*/
    //Loop para validação do nome, enquanto a função que valida a string retornar 0 (falso) o loop vai continuar (há uma negação na frente do retorno da função)
    printf("\n> PRIMEIRO NOME: \n");
    do
    {
        strcpy(entradaTemp, receberDadosCliente("rp", "nome do usuário"));
    } while (validarStringPadrao(entradaTemp, "rp"));
    //Se sair do loop é porque a string é válida e pode ser copiada para a variável correta que irá guardar
    strcpy(u.nome, entradaTemp);
    //Transformar o nome em maiúsculo para padronização do arquivo
    alternarCapitalLetras(u.nome, 1);
    //Limpar a variável temporária para receber a próxima entrada
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Recebe e processa o sobrenome
    printf("\n> ÚLTIMO SOBRENOME: \n");
    do
    {
        strcpy(entradaTemp, receberDadosCliente("ru", "sobrenome do usuário"));
    } while (validarStringPadrao(entradaTemp, "ru"));
    strcpy(u.sobrenome, entradaTemp);
    alternarCapitalLetras(u.sobrenome, 1);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Recebe e processa o e-mail
    printf("\n> E-MAIL: \n");
    do
    {
        strcpy(entradaTemp, receberDadosCliente("re", "e-mail do usuário"));
    } while (validarStringEmail(entradaTemp, "re"));
    strcpy(u.email, entradaTemp);
    alternarCapitalLetras(u.email, 1);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Recebe e processa o identificador
    printf("\n> IDENTIFICADOR: \n");
    do
    {
        strcpy(entradaTemp, receberDadosCliente("ri", "identificador do usuário"));
    } while (validarIdentificador(entradaTemp, "ri"));
    strcpy(u.identificador, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Recebe e processa a senha
    printf("\n> SENHA: \n");
    do
    {
        strcpy(entradaTemp, receberDadosCliente("rs", "senha do usuário"));
    } while (validarSenha(entradaTemp, "rs"));
    strcpy(u.senha, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));
}

/**
 * Recebe os dados para autenticar e procura no arquivo um identificador e senha para aprovar a autenticação
 * @return 1 em caso de sucesso; 0 em outros casos
 */
short int autenticar()
{
    //Variáveis que guardam os dados lidos nas linhas do arquivo
    char identificadorArquivo[MAX_IDENTIFICADOR], saltArquivo[SALT_SIZE + 1], criptografiaArquivo[120], usuarioArquivo[MAX_DADOS], sobrenomeArquivo[MAX_DADOS], emailArquivo[MAX_DADOS];
    int codigoArquivo = 0;
    do
    {
        //Recebe os dados de autenticação e copia para as devidas variáveis
        puts("# LOGIN: ");
        strcpy(u.identificador, receberDadosCliente("li", "identificador"));
        puts("# SENHA: ");
        strcpy(u.senha, receberDadosCliente("ls", "senha"));

        //Validação do arquivo
        if (testarArquivo(arquivoUsuarios, "au"))
            return 0;

        //Abrir o arquivo com parâmetro "r" de read, apenas lê
        ponteiroArquivos = fopen(arquivoUsuarios, "r");

        //Loop que passa por todas as linhas até o final do arquivo
        while (!feof(ponteiroArquivos))
        {
            //Define os dados da linha lida nas variáveis na respectiva ordem do padrão lido
            if (fscanf(ponteiroArquivos, ">%d|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^\n]\n", &codigoArquivo, identificadorArquivo, saltArquivo, criptografiaArquivo, usuarioArquivo, sobrenomeArquivo, emailArquivo) != 7)
            {
                printf("\n# ERRO - Não foi possível interpretar completamente a linha do arquivo. %d, %s, %s, %s, %s, %s, %s", codigoArquivo, identificadorArquivo, saltArquivo, criptografiaArquivo, usuarioArquivo, sobrenomeArquivo, emailArquivo);
                enviarDadosCliente("au", "# FALHA - Houve um problema ao ler a linha no arquivo de dados.");
                fecharArquivo(ponteiroArquivos, "au");
                return 0;
            }
            //Se o identificador lido no arquivo for compatível com o enviado pelo usuário vai fazer a criptografia da senha para testar
            if (!strcmp(identificadorArquivo, u.identificador))
            {
                //Copia o salt lido do arquivo nessa linha para a variável u.salt, onde o criptografarSenha() irá processar
                strcpy(u.salt, saltArquivo);
                //Criptografa a senha que o usuário digitou com o salt que foi lido na linha
                criptografarSenha();
                //Se senha enviada, criptografada com o salt da linha do arquivo, forem iguais, autentica o usuário
                if (!strcmp(criptografiaArquivo, u.senhaCriptografada))
                {
                    //Copiando dados para a estrutura
                    u.codigo = codigoArquivo;
                    strcpy(u.nome, usuarioArquivo);
                    strcpy(u.sobrenome, sobrenomeArquivo);
                    strcpy(u.email, emailArquivo);
                    strcpy(u.identificador, identificadorArquivo);
                    strcpy(u.salt, saltArquivo);
                    strcpy(u.senhaCriptografada, criptografiaArquivo);
                    //Salvar o formato da linha do usuário autenticado
                    sprintf(u.linhaUsuario, ">%d|%s|%s|%s|%s|%s|%s\n", u.codigo, u.identificador, u.salt, u.senhaCriptografada, u.nome, u.sobrenome, u.email);
                    enviarDadosCliente("au", u.linhaUsuario); //Envia a linha com os dados para que as variáveis da aplicação cliente sejam preenchidas com os dados do usuário logado
                    fecharArquivo(ponteiroArquivos, "au");    //Fecha o arquivo
                    return 1;
                }
            }
        }
        //Se sair do loop é porque não autenticou por erro de login/senha
        enviarDadosCliente("au", "# FALHA - Usuário e/ou senha incorretos!");
        fecharArquivo(ponteiroArquivos, "au");

        //Cliente enviará 1 se quer repetir a autenticação e 0 caso contrário (negação na frente da conversão)
        if (!atoi(receberDadosCliente("cm", "confirmação para repetir autenticação")))
        {
            //Entra aqui se o cliente enviar 0, pois deseja parar o loop de autenticação
            break;
        }
    } while (1);
    return 0;
}

/**
 * Menu de opções para o usuário autenticado
 * @return 1 caso a aplicação cliente seja finalizada e necessite desconectar; 0 caso foi realizado apenas um logout do usuário atual
 */
short int areaLogada()
{
    int operacao = 0; //Recebe o valor da entrada do usuário para escolher a operação
    imprimirDecoracao();
    printf(">> USUÁRIO '%s' ENTROU... >>", u.nome);

    //Loop do menu de operações do usuário logado
    do
    {
        imprimirDecoracao();
        puts("\t> MENU DE OPERAÇÕES: AUTENTICADO <");
        operacao = atoi(receberDadosCliente("ap", "operação do menu")); //Recebe a operação do cliente e converte para inteiro

        //Operação 0 é encerrar o programa (cliente), cliente vai desconectar
        if (operacao == 0)
        {
            limparEstruturaUsuario();
            finalizar(1, 0);
            break;
        }

        //Escolhe a operação conforme o valor que o usuário enviou
        switch (operacao)
        {
        case 1: //Logout
            imprimirDecoracao();
            limparEstruturaUsuario();
            return 0;
        case 2: //Excluir Cadastro
            imprimirDecoracao();
            printf("\t\t>> EXCLUIR CADASTRO <<\n");
            //Recebe e converte a confirmação para inteiro, o cliente vai enviar 1 caso confirme a exclusão
            if (atoi(receberDadosCliente("cd", "confirmação do usuário para deletar cadastro")))
            {
                //Se o retorno da função excluirUsuario for 1 = sucesso: limpa estrutura e sai da área logada, senão não sai da área logada.
                if (excluirUsuario())
                {
                    limparEstruturaUsuario();
                    return 0;
                }
            }
            puts("# OPERAÇÃO CANCELADA");
            break;
        case 3: //Enviar dados do usuário
            imprimirDecoracao();
            printf("\t\t>> VER DADOS DO USUÁRIO <<\n");
            verDadosUsuario();
            break;
        case 4: //Enviar política de senhas
            imprimirDecoracao();
            printf("\t>> VER POLÍTICA DE IDENTIFICADORES E SENHAS <<\n");
            mostrarPoliticaSenhas();
            break;
        default:
            printf("# INFO - Usuário enviou um comando inválido\n");
            break;
        }
    } while (1);
    //Quando chegar aqui, o cliente encerrou sua aplicação, servidor sairá do loop do cliente conectado (menu não autenticado)
    return 1;
}

/**
 * Excluir os dados do usuário do arquivo de dados
 * @return 1 se o cadastro foi deletado; 0 se a exclusão do cadastro falhou
 */
short int excluirUsuario()
{
    //Validação dos arquivos utilizados
    if (testarArquivo(arquivoUsuarios, "dl") || testarArquivo("transferindo.txt", "dl"))
    {
        return 0;
    }

    ponteiroArquivos = fopen(arquivoUsuarios, "r"); //Arquivo de entrada
    FILE *saida = fopen("transferindo.txt", "w");   //Arquivo de saída
    char texto[MAX];                                //Uma string grande para armazenar as linhas lidas
    short int usuarioExcluido = 0;                  //flag que determina se o usuário de fato foi apagado do arquivo

    //Loop pelas linhas do arquivo para salvar as linhas que não são do usuário em questão e não salvar a linha dele no novo arquivo gerado
    while (fgets(texto, MAX, ponteiroArquivos) != NULL)
    {
        //Se a linha sendo lida no arquivo for diferente da linha do usuário atual, ela será copiada para o arquivo de saída
        if (strcmp(u.linhaUsuario, texto))
        {
            fputs(texto, saida);
        }
        else
        {
            //Ativa a flag para guardar que encontrou a linha do usuário e "excluiu"
            usuarioExcluido = 1;
        }
    }
    //Fecha os arquivos utilizados
    fecharArquivo(ponteiroArquivos, "dl");
    fecharArquivo(saida, "dl");

    //Deleta o arquivo com os dados antigos
    if (remove(arquivoUsuarios))
    {
        printf("\n# ERRO - Ocorreu um problema ao deletar um arquivo necessário, o programa foi abortado.\n");
        enviarDadosCliente("dl", "# ERRO - Ocorreu um problema ao deletar um arquivo necessário, o servidor foi abortado.\n");
        finalizar(1, 1);
    }
    //Renomeia o arquivo onde foram passadas as linhas que não seriam excluídas para o nome do arquivo de dados de entrada
    if (rename("transferindo.txt", arquivoUsuarios))
    {
        printf("\n# ERRO - Ocorreu um problema ao renomear um arquivo necessário, o programa foi abortado.\n");
        enviarDadosCliente("dl", "# ERRO - Ocorreu um problema ao renomear um arquivo necessário, o servidor foi abortado.\n");
        finalizar(1, 1);
    }

    //Verifica se houve sucesso na exclusão, através a flag
    if (usuarioExcluido)
    {
        enviarDadosCliente("dl", "> SUCESSO - Seu cadastro foi excluído.");
        return 1;
    }
    else
    {
        enviarDadosCliente("dl", "# FALHA - A exclusão do cadastro falhou.");
        return 0;
    }
}

/**
 * Envia os dados do usuário atual para o cliente
 */
void verDadosUsuario()
{
    //Alocar o tamanho máximo possível para envio de dados entre servidor/cliente (-4 caracteres que são 2=idSincronização, 1='/' e 1 delimitador de string)
    char *dadosUsuario = malloc(MAX_MSG - 4);
    //Inserir os dados organizados em uma variável para envio
    sprintf(dadosUsuario, "¬ Código: %d\n¬ Nome Completo: %s %s\n¬ E-mail: %s\n¬ Login: %s\n¬ Salt: %s\n¬ Senha Criptografada: %s", u.codigo, u.nome, u.sobrenome, u.email, u.identificador, u.salt, u.senhaCriptografada);
    //Enviar os dados
    enviarDadosCliente("du", dadosUsuario);
}

/**
 * Envia a política de identificadores e senhas
 */
void mostrarPoliticaSenhas()
{
    enviarDadosCliente("pt", "\n\t\tIDENTIFICADOR/LOGIN\n\n-Não pode ser utilizado nome, sobrenome ou email;\n-Pode conter somente caracteres alfanuméricos e ponto final;\n-Deve ter no mínimo 5 caracteres e no máximo 15.\n\n\t\t\tSENHA\n\n-Deve conter no mínimo 8 caracteres e no máximo 30;\n-Deve conter, no mínimo, 2 caracteres especiais;\n-Deve conter números e letras;\n-Deve conter pelo menos uma letra maiúscula e uma minúscula;\n-Não pode conter mais de 2 números ordenados em sequência (Ex.: 123, 987);\n-Não pode conter mais de 2 números repetidos em sequência (Ex.: 555, 999);\n-Não pode conter caracteres que não sejam alfanuméricos (números e letras), especiais ou espaço.\n");
}

/**
 * Transforma a String inteira para maiúscula ou minúscula e salva na própria variável, facilitando em comparação de dados
 * @param string A string que quer trocar para maiúscula ou minúscula
 * @param flag 1 para maiúscula; 0 para minúscula
 * @return Variável string passada como parâmetro, maiúscula ou minúscula, dependendo da flag
 */
char *alternarCapitalLetras(char *string, int flag)
{
    //Declarei o i fora do for para pegar depois a última posição da string e adicionar o NULL
    int i = 0;

    //Passa por todos os caracteres da String
    for (i; string[i] != '\0'; i++)
    {
        if (flag == 1)
        {
            //Converte o caractere para maíusculo e sobrescreve no mesmo local, substituindo
            string[i] = toupper(string[i]);
        }
        else
        {
            //Converte o caractere para minúsculo e sobrescreve no mesmo local, substituindo
            string[i] = tolower(string[i]);
        }
    }
    //Acrescenta o caractere nulo no final da string convertida
    string[i] = '\0';

    //Retorna a mesma palavra, convertida
    return string;
}

/**
 * Verifica se a string passada como parâmetro contém somente caracteres alfabéticos e se o tamanho está entre 2 e 50 letras
 * @param string é a string que deseja validar
 * @param idSincronia é o identificador que está sincronizando a aplicação cliente/servidor e será inserido na função enviarDados() 
 * caso seja necessário enviar alguma mensagem para o cliente
 * @return 0 caso a string seja válida; 1 caso string inválida
 */
short int validarStringPadrao(char *string, char *idSincronia)
{
    //Verifica o tamanho da string
    if (strlen(string) < 2 || strlen(string) > 50)
    {
        enviarDadosCliente(idSincronia, "# FALHA [QUANTIDADE DE CARACTERES]: esse dado deve conter no mínimo 2 letras e no máximo 50!\n> Tente novamente: ");
        return 1;
    }

    //Loop para passar pelos caracteres da string verificando se são alfabéticos
    for (int i = 0; i < strlen(string); i++)
    {
        //Usando a função isalpha da biblioteca ctype.h, é possível verificar se o caractere é alfabético
        if (!isalpha(string[i]))
        {
            enviarDadosCliente(idSincronia, "# FALHA [CARACTERE INVÁLIDO]: insira somente caracteres alfabéticos nesse campo, sem espaços ou pontuações\n> Tente novamente: ");
            return 1;
        }
    }
    //Se chegou aqui é válido
    enviarDadosCliente(idSincronia, "> SUCESSO: palavra aprovada!");
    return 0;
}

/**
 * Verifica se a string passada como parâmetro contém formato permitido para e-mail e tamanho entre 6 e 50 caracteres
 * @param string é a string do e-mail que deseja testar
 * @param idSincronia é o identificador que está sincronizando a aplicação cliente/servidor e será inserido na função enviarDados() 
 * caso seja necessário enviar alguma mensagem para o cliente
 * @return 0 caso a string seja válida; 1 caso string inválida
 */
short int validarStringEmail(char *string, char *idSincronia)
{
    //Variáveis para guardar as partes do e-mail
    char usuario[50], site[50], dominio[50];

    //Valida tamanho da string
    if (strlen(string) < 6 || strlen(string) > 50)
    {
        enviarDadosCliente(idSincronia, "# FALHA [QUANTIDADE DE CARACTERES]: verifique o e-mail digitado, deve conter entre 6 e 50 caracteres\n> Tente novamente: ");
        return 1;
    }

    //sscanf() lê a entrada a partir da string no primeiro parâmetro, atribuindo para as variáveis. Retorna o número de campos convertidos e atribuídos com êxito.
    if (sscanf(string, "%[^@ \t\n]@%[^. \t\n].%3[^ \t\n]", usuario, site, dominio) != 3)
    {
        enviarDadosCliente(idSincronia, "# FALHA [E-MAIL INVÁLIDO]: verifique o e-mail digitado\n> Tente novamente: ");
        return 1;
    }
    //Se chegou aqui é porque a string é válida
    enviarDadosCliente(idSincronia, "> SUCESSO: e-mail aprovado!");
    return 0;
}

/**
 * Função para verificar se o identificador cumpre com a política
 * @param identificador é a string do identificador que deseja verificar se é válida
 * @param idSincronia é o identificador que está sincronizando a aplicação cliente/servidor e será inserido na função enviarDados() 
 * caso seja necessário enviar alguma mensagem para o cliente
 * @return 0 em caso de identificador válido; 1 caso inválido
 */
short int validarIdentificador(char *identificador, char *idSincronia)
{
    char identificadorMaiusculo[MAX_IDENTIFICADOR]; //Variável que irá guardar o identificador convertido para maiúsculo, para simplificar a comparação com o nome e sobrenome
    char identificadorArquivo[MAX_IDENTIFICADOR];   //Variável para guardar o identificador recebido do arquivo

    //Verifica tamanho do identificador
    if (strlen(identificador) < 5 || strlen(identificador) > 15)
    {
        enviarDadosCliente(idSincronia, "# FALHA [IDENTIFICADOR INVÁLIDO] - Não contém tamanho permitido (mínimo 5 e máximo 15)\n> Tente novamente: ");
        return 1;
    }

    //Loop para passar pelos caracteres do identificador e verificar se contém somente caracteres válidos
    for (int i = 0; i < strlen(identificador); i++)
    {
        if (!isalnum(identificador[i]) && identificador[i] != '.')
        {
            enviarDadosCliente(idSincronia, "# FALHA [IDENTIFICADOR INVÁLIDO] - Contém caracteres não permitidos\n# O identificador pode conter somente caracteres alfanuméricos e ponto final.\n> Tente novamente: ");
            return 1;
        }
    }

    strcpy(identificadorMaiusculo, identificador);    //Copiando o identificador para transformar em maiúsculo
    alternarCapitalLetras(identificadorMaiusculo, 1); //Tornando maiúsculo
    //Se o identificador for igual ao nome ou sobrenome, é inválido
    if (!strcmp(identificadorMaiusculo, u.nome) || !strcmp(identificadorMaiusculo, u.sobrenome))
    {
        enviarDadosCliente(idSincronia, "# FALHA [IDENTIFICADOR INVÁLIDO] - Identificador não pode ser seu nome ou sobrenome!\n> Tente novamente: ");
        return 1;
    }

    /*Verifica se o identificador já foi utilizado*/
    //Validação do arquivo necessário
    if (testarArquivo(arquivoUsuarios, idSincronia))
        return 1;

    ponteiroArquivos = fopen(arquivoUsuarios, "r"); //Abrir o arquivo com parâmetro "r" de read, apenas lê

    //Passa pelas linhas do arquivo verificando os identificadores já inseridos
    while (!feof(ponteiroArquivos))
    {
        //Lê linha por linha colocando os valores, conforme o formato, nas variáveis
        fscanf(ponteiroArquivos, ">%[^|]|%[^|]|%[^\n]\n", temp, identificadorArquivo, temp);

        alternarCapitalLetras(identificadorArquivo, 1); //Transformar o identificador recebido do arquivo em maiúsculo também para comparar com o que está tentando cadastrar
        //Realizando a comparação dos identificadores
        if (!strcmp(identificadorArquivo, identificadorMaiusculo))
        {
            //Se entrar aqui, o identificador já foi utilizado
            fecharArquivo(ponteiroArquivos, idSincronia);
            enviarDadosCliente(idSincronia, "# FALHA [IDENTIFICADOR INVÁLIDO] - Já está sendo utilizado!\n> Tente novamente: ");
            return 1;
        }
    }
    fecharArquivo(ponteiroArquivos, idSincronia); //Fecha o arquivo
    //Se chegou até aqui, passou pelas validações, retorna 0
    enviarDadosCliente(idSincronia, "> SUCESSO: identificador aprovado!");
    return 0;
}

/**
 * Função para verificar se a senha cumpre com a política de senhas
 * @param senha é a string da senha que quer validar
 * @param idSincronia é o identificador que está sincronizando a aplicação cliente/servidor e será inserido na função enviarDados() 
 * caso seja necessário enviar alguma mensagem para o cliente
 * @return 0 em caso de senha válida; 1 caso inválida
 */
short int validarSenha(char *senha, char *idSincronia)
{
    //Contadores dos tipos de caracteres que a senha possui
    int contMinusculas = 0, contMaiusculas = 0, contNumeros = 0, contEspeciais = 0;
    char confirmacaoSenha[MAX_SENHA]; //Variável que receberá a confirmação da senha

    //Verifica tamanho da senha
    if (strlen(senha) < 8 || strlen(senha) > 30)
    {
        enviarDadosCliente(idSincronia, "# FALHA [SENHA INVÁLIDA] - Não contém tamanho permitido (mínimo 8 e máximo 30)");
        return 1;
    }

    //Loop para passar pelos caracteres da senha
    for (int i = 0; i < strlen(senha); i++)
    {
        /*Sequência de condições para verificar cada caractere da senha*/
        //Usando a função islower da biblioteca ctype.h, é possível verificar se o caractere é alfabético e minúsculo
        if (islower(senha[i]))
        {
            contMinusculas++;
        }
        //Usando a função isupper da biblioteca ctype.h, é possível verificar se o caractere é alfabético e maiúsculo
        else if (isupper(senha[i]))
        {
            contMaiusculas++;
        }
        //Usando a função isdigit da biblioteca ctype.h, é possível verificar se o caractere é um dígito
        else if (isdigit(senha[i]))
        {
            contNumeros++;

            //Verifica se a senha contém +2 números ordenados em sequência (ascendente ou descendente)
            if (((senha[i] - '0') + 1 == senha[i + 1] - '0' && (senha[i] - '0') + 2 == senha[i + 2] - '0') || ((senha[i] - '1') == senha[i + 1] - '0' && (senha[i] - '2') == senha[i + 2] - '0'))
            {
                enviarDadosCliente(idSincronia, "# FALHA [SENHA INVÁLIDA] - contém números ordenados em sequência");
                return 1;
            }

            //Verifica se a senha contém +2 números repetidos em sequência
            if (senha[i] == senha[i + 1] && senha[i] == senha[i + 2])
            {
                enviarDadosCliente(idSincronia, "# FALHA [SENHA INVÁLIDA] - contém números repetidos em sequência");
                return 1;
            }
        }
        //Verificando se o caractere é especial ou espaço
        else if (ispunct(senha[i]) || isspace(senha[i]))
        {
            contEspeciais++;
        }
        else
        {
            enviarDadosCliente(idSincronia, "# FALHA [SENHA INVÁLIDA] - Sua senha contém caracteres que nao são nem alfanuméricos nem especiais ou espaço.\n# Verifique a digitação e tente novamente.\n# Caracteres permitidos:\n#\tEspeciais: ! \" # $ %% & ' ( ) * + , - . / : ; < = > ? @ [ \\ ] ^ _ ` { | } ~ [ESPAÇO]\n#\tNuméricos: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9\n#\tAlfabéticos: a b c d e f g h i j k l m n o p q r s t u v w x y z #\t\t\tA B C D E F G H I J K L M N O P Q R S T U V W X Y Z");
            return 1;
        }
    } //Fim do loop que passa pelos caracteres da senha

    //Valida a quantidade de caracteres especiais
    if (contEspeciais < 2)
    {
        enviarDadosCliente(idSincronia, "# FALHA [SENHA INVÁLIDA] - Não contém caracteres especiais suficientes");
        return 1;
    }
    //Verifica se contém números e letras
    if ((contMinusculas + contMaiusculas) == 0 || contNumeros == 0)
    {
        enviarDadosCliente(idSincronia, "# FALHA [SENHA INVÁLIDA] - Não contém letras e números");
        return 1;
    }
    //Verifica se contém minúsculas
    if (contMinusculas == 0)
    {
        enviarDadosCliente(idSincronia, "# FALHA [SENHA INVÁLIDA] - Não contém qualquer letra minúscula");
        return 1;
    }
    //Verifica se contém maiúsculas
    if (contMaiusculas == 0)
    {
        enviarDadosCliente(idSincronia, "# FALHA [SENHA INVÁLIDA] - Não contém qualquer letra maiúscula");
        return 1;
    }

    //Solicita a confirmação da senha
    enviarDadosCliente(idSincronia, "* Solicitar a confirmação de senha...");
    //Compara as 2 senhas informadas, se forem diferentes vai retornar != 0, entrando na condição
    if (strcmp(receberDadosCliente(idSincronia, "confirmação da senha"), senha))
    {
        enviarDadosCliente(idSincronia, "# FALHA [SENHAS INCOMPATÍVEIS] - As senhas não coincidem");
        return 1;
    }

    enviarDadosCliente(idSincronia, "> SUCESSO - Sua senha está de acordo com a política e foi aprovada!");
    return 0;
}

/**
 * Gerar o valor de salt e inserir na variável salt, na struct do usuário atual
 */
void gerarSalt()
{
    char *buffer;                                                                                //Ponteiro onde serão armazenados os caracteres gerados aleatoriamente
    char listaCaracteres[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./"; //Lista de caracteres para serem escolhidos para o salt
    int retorno;                                                                                 //Para guardar a quantidade de caracteres gerados na função getrandom

    //Reservar espaço do tamanho do salt + 1 (para o caractere NULL) na memória
    buffer = malloc(SALT_SIZE + 1);

    //Flag 0 na função getrandom(), para que a função utilize /dev/urandom - fonte de aleatoriedade do próprio Kernel
    retorno = getrandom(buffer, SALT_SIZE, 0);

    //Verifica se a função retornou todo os bytes necessários
    if (retorno != SALT_SIZE)
    {
        perror("# ERRO FATAL - A geração de caracteres para criação do salt não foi bem sucedida, aplicação abortada");
        finalizar(1, 1);
    }

    //Loop para montagem da string do salt, escolhendo os caracteres
    for (int i = 0; i < SALT_SIZE; i++)
    {
        /*Seleciona 1 caractere da lista: converte o caractere do buffer para unsigned char (número) e faz
        MOD quantidade de caracteres da lista, o resultado será (resto da divisão) o índice que contém o caractere a ser usado.
        Evitando assim que surjam caracteres que não podem ser interpretados pela codificação do SO ou caracteres não permitidos
        para uso na função crypt, posteriormente.*/
        u.salt[i] = listaCaracteres[((unsigned char)buffer[i]) % (strlen(listaCaracteres))];
    }
    //Adiciona o caractere NULL na última posição da string salt
    u.salt[SALT_SIZE] = '\0';
}

/**
 * Criptografa a senha do usuário e insere o valor da senha criptografada na variável senhaCriptografada do usuário atual
 */
void criptografarSenha()
{
    //Reservar espaço de 120 bytes na memória, que é tamanho suficiente do retorno da função crypt
    u.senhaCriptografada = malloc(120);

    //Variável que armazena o valor do parâmetro (formatado) para função crypt
    char idSaltSenha[strlen(PARAMETRO_CRYPT) + SALT_SIZE];

    //Incluindo o valor do salt gerado na variável (idSaltSenha)
    sprintf(idSaltSenha, PARAMETRO_CRYPT, u.salt);

    //Criptografa e envia o retorno do crypt para senhaCriptografada
    u.senhaCriptografada = crypt(u.senha, idSaltSenha);
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
 *  Verifica se o arquivo passado como parâmetro pode ser criado/utilizado
 *  @param nomeArquivo é o nome do arquivo que se deseja testar
 *  @return 1 caso o arquivo não possa ser acessado; 0 caso contrário
 */
short int testarArquivo(char *nomeArquivo, char *idSincronia)
{
    FILE *arquivo = fopen(nomeArquivo, "a");
    if (arquivo == NULL)
    {
        enviarDadosCliente(idSincronia, "# ERRO - Um arquivo necessário não pode ser acessado, não é possível prosseguir.\n");
        printf("# ERRO - O arquivo '%s' não pode ser acessado, verifique.\n", nomeArquivo);
        return 1;
    }
    fecharArquivo(arquivo, idSincronia);
    return 0;
}

/**
 * Pausa no programa para que mensagens possam ser lidas antes de limpar a tela
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
 * Limpa os dados salvos na estrutura do usuário
 */
void limparEstruturaUsuario()
{
    printf("<< USUÁRIO '%s' SAIU... <<", u.nome);
    memset(&u.nome[0], 0, sizeof(u.nome));
    memset(&u.sobrenome[0], 0, sizeof(u.sobrenome));
    memset(&u.email[0], 0, sizeof(u.email));
    memset(&u.identificador[0], 0, sizeof(u.identificador));
    memset(&u.salt[0], 0, sizeof(u.salt));
    memset(&u.senha[0], 0, sizeof(u.senha));
    memset(&u.linhaUsuario[0], 0, sizeof(u.linhaUsuario));
    u.senhaCriptografada = NULL;
    u.codigo = '0';
}

/**
 * Fecha o arquivo verificando se não houve erro, se houver encerra o programa para prevenir problemas posteriores
 */
void fecharArquivo(FILE *arquivo, char *idSincronia)
{
    if (fclose(arquivo))
    {
        system("clear");
        enviarDadosCliente(idSincronia, "# ERRO - Ocorreu um erro ao fechar um arquivo necessário, o servidor foi abortado.");
        finalizar(1, 1);
    }
}