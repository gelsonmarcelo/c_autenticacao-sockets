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

 *      POLÍTICA DE CONTROLE DE ACESSO
- Coordenador:
    - Dados dos Estudantes: ver dados;
    - Disciplina: ver descrição da disciplina; alterar descrição da disciplina; matricular estudante;
    - Notas: ver notas; alterar notas.
- Professor:
    - Dados dos Estudantes: ver dados;
    - Disciplina: ver descrição da disciplina; alterar descrição da disciplina;
    - Notas: ver notas; alterar notas.
- Estudante:
    - Dados dos Estudantes: ver dados*, alterar dados*;
    - Disciplina: ver descrição da disciplina; matricular estudante*;
    - Notas: ver notas*
    *Somente próprias informações
 **/

FILE *ponteiroArquivos = NULL; //Ponteiro para manipular os arquivos
char temp[MAX * 5];            //Variável temporária para gravação de valores que não serão utilizados mas precisam ser colocados em alguma variável

//Strings com nomes dos arquivos utilizados
char arquivoUsuarios[] = "dados.txt";

//Variáveis globais Socket
int conexao;              //Guarda o retorno de accept, um arquivo descritor do socket conectado
char msgCliente[MAX_MSG]; //Guarda a mensagem recebida do cliente
char operacaoCliente[3];

struct Usuario u; //Declaração da estrutura do usuário

/*Declaração das funções*/

int pegarProximoId(char *arquivo);
short int autenticar();
short int validarStringPadrao(char *string);
short int validarIdentificador(char *identificador);
short int validarSenha(char *senha);
short int excluirDados();
short int testarArquivo(char *nomeArquivo);
char *alternarCapitalLetras(char *string, int flag);
void cadastrarUsuario();
void gerarSalt();
void criptografarSenha();
void imprimirDecoracao();
void coletarDados(short int nome, short int sobrenome, short int identificador, short int senha);
void atualizarLinhaArquivo(char *arquivo, char *linhaObsoleta, char *linhaAtualizada);
void pausarPrograma();
void finalizarPrograma();
void fecharArquivo();

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
    short int papel;                       //Guarda id do papel do usuário no sistema
    char linhaUsuario[MAX];                //Essa string é utilizada para encontrar a linha do usuário no arquivo
};

char *receberDadosCliente(char *dadoEsperado)
{
    int tamanho; //Guarda o tamanho da mensagem recebida pelo cliente

    printf("> Aguardando: %s...\n", dadoEsperado);

    //Lê dados enviados pelo cliente
    if ((tamanho = read(conexao, msgCliente, MAX_MSG)) < 0)
    {
        perror("# ERRO - Erro ao receber dados do cliente:");
        return "ERRO";
    }

    msgCliente[tamanho] = '\0'; //Adiciona terminador de string
    return msgCliente;
}

void enviarDadosCliente(char *msg_servidor)
{
    if (write(conexao, msg_servidor, strlen(msg_servidor)) < 0)
    {
        printf("# ERRO - Falha ao enviar mensagem:\n\"%s\"\n", msg_servidor);
    }
    printf("\n> Mensagem enviada para o cliente:\n\"%s\"\n", msg_servidor);
}

void finalizarConexaoCliente()
{
    imprimirDecoracao();
    shutdown(conexao, SHUT_RDWR);
    close(conexao);

    puts("> Conexão com cliente encerrada");
}

/**
 * Função principal
 */
int main()
{
    setlocale(LC_ALL, "Portuguese"); //Permite a utilização de acentos e caracteres do português nas impressões

    //Verifica arquivos necessários para o programa iniciar
    if (testarArquivo(arquivoUsuarios))
    {
        printf("# ERRO FATAL - um arquivo de dados essencial não pode ser aberto, o programa não pode ser iniciado.\n");
        finalizarPrograma();
    }

    /******* CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/
    puts("> INICIANDO SERVIDOR...");
    pausarPrograma();
    system("clear");

    int socketDescritor;                  //Descritor do socket
    int socketSize;                       //Guarda o tamanho da estrutura do socket
    struct sockaddr_in servidor, cliente; //Estruturas de cliente e servidor, para guardar seus dados
    char *clienteIp;                      //Para pegar o IP do cliente
    int clientePorta;                     //Para pegar porta do cliente

    //Cria um socket
    socketDescritor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketDescritor == -1)
    {
        perror("# ERRO - Falha ao criar socket:");
        return EXIT_FAILURE;
    }

    printf("> Socket do servidor criado com descritor: %d\n", socketDescritor);

    //Define as propriedades da struct do socket
    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY; // Obtem IP do Sistema Operacional
    servidor.sin_port = htons(PORTA);

    //Trata o erro de porta já está em uso
    int sim = 1;
    if (setsockopt(socketDescritor, SOL_SOCKET, SO_REUSEADDR, &sim, sizeof(int)) == -1)
    {
        perror("# ERRO - Erro na porta do socket:");
        return EXIT_FAILURE;
    }

    //Associa o socket à porta
    if (bind(socketDescritor, (struct sockaddr *)&servidor, sizeof(servidor)) == -1)
    {
        perror("# ERRO - Erro ao fazer bind:");
        return EXIT_FAILURE;
    }

    // Aguarda conexões de clientes
    if (listen(socketDescritor, 1) == -1)
    {
        perror("# ERRO - Erro ao ouvir conexões:");
        return EXIT_FAILURE;
    }
    printf("> Ouvindo na porta %d\n", PORTA);
    pausarPrograma();

    do
    {
        system("clear");
        //Aceita e trata conexões
        puts("> Aguardando por conexões...");
        socklen_t clienteLenght = sizeof(cliente);
        if ((conexao = accept(socketDescritor, (struct sockaddr *)&cliente, &clienteLenght)) == -1)
        {
            perror("# ERRO - Erro ao aceitar conexão:");
            return EXIT_FAILURE;
        }

        //Obtém IP e porta do cliente
        clienteIp = inet_ntoa(cliente.sin_addr);
        clientePorta = ntohs(cliente.sin_port);
        /******* FIM - CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/

        imprimirDecoracao();
        printf("\t\t>> CLIENTE CONECTADO <<\n");
        printf("> Cliente IP:PORTA = %s:%d", clienteIp, clientePorta);

        int operacao = 0; //Recebe um número que o usuário digitar para escolher a opção do menu

        //Loop do menu de operações
        do
        {
            imprimirDecoracao();
            puts("\t\t> MENU DE OPERAÇÕES <");
            operacao = atoi(receberDadosCliente("operação do menu"));

            if (operacao == 0)
            {
                finalizarConexaoCliente();
                break;
            }

            //Escolhe a operação conforme o valor que o usuário digitou
            switch (operacao)
            {
            case 1: //Login
                imprimirDecoracao();
                printf("\t\t\t>> AUTENTICAÇÃO <<");
                imprimirDecoracao();
                if (autenticar(1))
                {
                    puts("> Cliente está autenticado.");
                    // finalizarConexaoCliente(conexao);
                    // finalizarPrograma();
                }
                break;
            case 2: //Cadastro
                imprimirDecoracao();
                printf("\n\t\t\t>> CADASTRO <<\n\n");
                imprimirDecoracao();
                cadastrarUsuario();
                break;
            default:
                printf("# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida, tente novamente!\n");
                break;
            }
        } while (1);
        imprimirDecoracao();
        puts("> Deseja finalizar o servidor [s/n]?\n");
        if (getchar() == 's')
        {
            getchar();
            setbuf(stdin, NULL);
            break;
        }
        getchar();
        setbuf(stdin, NULL);

    } while (1);
    return EXIT_SUCCESS;
}

/**
 * Busca no arquivo o último ID cadastrado e retorna o próximo ID a ser usado
 * @param arquivo é o char com nome do arquivo que deseja utilizar para verificar o próximo ID
 * @return valor do próximo ID a ser usado e 0 em caso de falha ao abrir o arquivo para leitura, 
 * retorna 1 se não conseguir ler qualquer ID anterior no arquivo
 */
int pegarProximoId(char *arquivo)
{
    //Teste do arquivo
    if (testarArquivo(arquivo))
        return 0;

    int id = 0;

    //Abre o arquivo
    ponteiroArquivos = fopen(arquivo, "r");

    while (!feof(ponteiroArquivos))
    {
        //Lê as linhas até o final do arquivo, atribuindo o id da linha na variável id com formato inteiro
        fscanf(ponteiroArquivos, "%d|%[^\n]", &id, temp);
    }
    fecharArquivo(ponteiroArquivos);

    //O ID lido por último é o último ID cadastrado e será somado mais 1 e retornado para cadastrar o próximo
    return id + 1;
}

/**
 * Função para cadastrar novo usuário, solicita todos os dados ao usuário e insere no arquivo de dados dos usuários
 */
void cadastrarUsuario()
{
    //Validação para caso o arquivo não possa ser aberto parar antes de pedir todos os dados ao usuário
    if (testarArquivo(arquivoUsuarios))
        return;

    printf("\n> Usuário deve fornecer as informações necessárias para efetuar o cadastro:\n");
    coletarDados(1, 1, 1, 1);

    u.codigo = pegarProximoId(arquivoUsuarios); //Define o ID do usuário que está se cadastrando

    //Abrir o arquivo com parâmetro "a" de append, não sobrescreve as informações, apenas adiciona
    ponteiroArquivos = fopen(arquivoUsuarios, "a");

    gerarSalt();
    criptografarSenha(u.senha);

    //Passar dados recebidos para a linha do usuário no formato que irão para o arquivo
    sprintf(u.linhaUsuario, "%d|%s|%s|%s|%s|%s\n", u.codigo, u.identificador, u.salt, u.senhaCriptografada, u.nome, u.sobrenome);

    //Insere a string com todos os dados no arquivo e valida se não ocorreram erros
    if (fputs(u.linhaUsuario, ponteiroArquivos) == EOF)
    {
        enviarDadosCliente("# ERRO - Houve algum problema para inserir os dados no arquivo.\n");
    }
    else
    {
        enviarDadosCliente("> SUCESSO - Cadastro realizado: realize login para acessar o sistema.\n");
    }

    fecharArquivo(ponteiroArquivos); //Fecha o arquivo
    // limparEstruturaUsuario();
}

/**
 * Coletar dados do usuário, chame a função definindo o parâmetro no valor 1 no respectivo dado que deseja solicitar ao usuário
 */
void coletarDados(short int nome, short int sobrenome, short int identificador, short int senha)
{
    char entradaTemp[MAX];                           //Variável para fazer a entrada de valores digitados e realizar a validação antes de atribuir à variável correta
    memset(&entradaTemp[0], 0, sizeof(entradaTemp)); //Limpando a variável para garantir que não entre lixo de memória

    //Solicita o nome do usuário
    if (nome)
    {
        //Loop para validação do nome, enquanto a função que valida a string retornar 0 (falso) o loop vai continuar (há uma negação na frente do retorno da função)
        printf("\n> PRIMEIRO NOME: \n");
        do
        {
            setbuf(stdin, NULL);                                         //Limpa a entrada par evitar lixo
            strcpy(entradaTemp, receberDadosCliente("nome do usuário")); //Leitura do teclado, o %[^\n] vai fazer com que o programa leia tudo que o usuário digitar exceto ENTER (\n), por padrão pararia de ler no caractere espaço
        } while (!validarStringPadrao(entradaTemp));
        //Se sair do loop é porque a string é válida e pode ser copiada para a variável correta que irá guardar
        strcpy(u.nome, entradaTemp);
        //Transformar o nome em maiúsculo para padronização do arquivo
        alternarCapitalLetras(u.nome, 1);
        //Limpar a variável temporária para receber a próxima entrada
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    }

    //Solicita o sobrenome do usuário
    // for (int i = 1; i < 10000000; i++)
    //     ;
    // enviarDadosCliente("\n> Informe seu último sobrenome: ");
    if (sobrenome)
    {
        printf("\n> ÚLTIMO SOBRENOME: \n");
        do
        {
            // printf("\n> Informe seu último sobrenome: ");
            setbuf(stdin, NULL);
            strcpy(entradaTemp, receberDadosCliente("sobrenome do usuário"));
        } while (!validarStringPadrao(entradaTemp));
        strcpy(u.sobrenome, entradaTemp);
        alternarCapitalLetras(u.sobrenome, 1);
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    }

    //Solicita que o usuário crie um identificador
    if (identificador)
    {
        printf("\n> IDENTIFICADOR: \n");
        do
        {
            // printf("\n> Crie seu login: ");
            setbuf(stdin, NULL);
            strcpy(entradaTemp, receberDadosCliente("identificador do usuário"));
        } while (!validarIdentificador(entradaTemp));
        strcpy(u.identificador, entradaTemp);
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    }

    //Solicita o usuário crie uma senha
    if (senha)
    {
        printf("\n> SENHA: \n");
        do
        {
            setbuf(stdin, NULL);
            //getpass() é usado para desativar o ECHO do console e não exibir a senha sendo digitada pelo usuário, retorna um ponteiro apontando para um buffer contendo a senha, terminada em '\0' (NULL)
            // puts("\n> Crie uma senha: ");
            strcpy(entradaTemp, receberDadosCliente("senha do usuário"));
        } while (!validarSenha(entradaTemp));
        strcpy(u.senha, entradaTemp);
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));

        // //Gerar o valor pseudoaleatório do salt desse usuário e jogar na variável salt dele
        // gerarSalt();
        // //Realiza a criptografia da senha com a função 'crypt' e define o valor na variável senhaCriptografada do usuário
        // criptografarSenha();
    }
}

/**
 * Realizar a autenticação do usuário
 * @param ehLogin deve ser passado como 1 caso a chamada da função está sendo realizada para login e não somente autenticação, 
 * quando for login a função vai definir os dados do usuário, trazidos do arquivo, na estrutura
 * @return 1 em caso de sucesso e; 0 em outros casos
 */
short int autenticar()
{
    //Variáveis que guardam os dados lidos nas linhas do arquivo
    char identificadorArquivo[MAX_IDENTIFICADOR], saltArquivo[SALT_SIZE + 1], criptografiaArquivo[120], usuarioArquivo[MAX_DADOS], sobrenomeArquivo[MAX_DADOS];
    int codigoArquivo = 0;
    do
    {
        puts("# LOGIN: ");
        strcpy(u.identificador, receberDadosCliente("identificador"));
        puts("# SENHA: ");
        strcpy(u.senha, receberDadosCliente("senha"));

        //Validação para caso o arquivo não possa ser aberto
        if (testarArquivo(arquivoUsuarios))
            return 0;

        //Abrir o arquivo com parâmetro "r" de read, apenas lê
        ponteiroArquivos = fopen(arquivoUsuarios, "r");

        //Loop que passa por todas as linhas do arquivo até o final
        while (!feof(ponteiroArquivos))
        {
            //Define os dados da linha lida nas variáveis na respectiva ordem do padrão lido
            fscanf(ponteiroArquivos, "%d|%[^|]|%[^|]|%[^|]|%[^|]|%[^\n]", &codigoArquivo, identificadorArquivo, saltArquivo, criptografiaArquivo, usuarioArquivo, sobrenomeArquivo);

            //Copia o salt lido do arquivo nessa linha para a variável u.salt, onde o criptografarSenha() irá usar
            strcpy(u.salt, saltArquivo);
            //Criptografa a senha que o usuário digitou com o salt que foi lido na linha
            criptografarSenha();
            //Se o identificador lido no arquivo e a senha digitada, criptografada com o salt da linha do arquivo forem iguais, autentica o usuário
            if (!strcmp(identificadorArquivo, u.identificador) && !strcmp(criptografiaArquivo, u.senhaCriptografada))
            {
                //Copiando dados para a estrutura
                u.codigo = codigoArquivo;
                strcpy(u.nome, usuarioArquivo);
                strcpy(u.sobrenome, sobrenomeArquivo);
                strcpy(u.identificador, identificadorArquivo);
                strcpy(u.salt, saltArquivo);
                strcpy(u.senhaCriptografada, criptografiaArquivo);
                //Salvar o formato da linha do usuário autenticado
                // sprintf(u.linhaUsuario, "%d|%s|%s|%s|%s|%s\n", u.codigo, u.identificador, u.salt, u.senhaCriptografada, u.nome, u.sobrenome);
                enviarDadosCliente("> SUCESSO - Login realizado!");
                fecharArquivo(ponteiroArquivos); //Fecha o arquivo
                return 1;
            }
        }
        //Se sair do loop é porque não autenticou por erro de login/senha
        fecharArquivo(ponteiroArquivos);
        enviarDadosCliente("# FALHA - Usuário e/ou senha incorretos!");

        if (atoi(receberDadosCliente("confirmação para voltar ao menu")))
        {
            break;
        }
    } while (1);
    return 0;
}

/**
 * Transforma a String inteira para maiúscula ou minúscula e salva na própria variável, facilitando em comparação de dados
 * @param String A string que quer trocar para maiúscula ou minúscula
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
            //Converte o caractere para maiusculo e sobrescreve no mesmo local, substituindo o caractere
            string[i] = toupper(string[i]);
        }
        else
        {
            //Converte o caractere para maiusculo e sobrescreve no mesmo local, substituindo o caractere
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
 * @return 1 caso a string seja válida; 0 caso string inválida
 */
short int validarStringPadrao(char *string)
{
    //Verifica o tamanho da string
    if (strlen(string) > 50 || strlen(string) < 2)
    {
        // enviarDadosCliente("0");
        // for (int i = 1; i < 10000000; i++)
        //     ;
        enviarDadosCliente("# FALHA [QUANTIDADE DE CARACTERES]: esse dado deve conter no mínimo 2 letras e no máximo 50!\n> Tente novamente: ");
        return 0;
    }

    //Loop para passar pelos caracteres da string verificando se são alfabéticos
    for (int i = 0; i < strlen(string); i++)
    {
        //Usando a função isalpha da biblioteca ctype.h, é possível verificar se o caractere é alfabético
        if (!isalpha(string[i]))
        {
            // enviarDadosCliente("0");
            // for (int i = 1; i < 10000000; i++)
            //     ;
            enviarDadosCliente("# FALHA [CARACTERE INVÁLIDO]: insira somente caracteres alfabéticos nesse campo, sem espaços ou pontuações.\n> Tente novamente: ");
            return 0;
        }
    }
    //Se chegou aqui é válido
    enviarDadosCliente("> SUCESSO: palavra aprovada!");
    return 1;
}

/**
 * Função para verificar se o identificador cumpre com a política
 * @param identificador é a string do identificador que deseja verificar se é válida
 * @return 1 em caso de identificador válido; 0 caso inválido
 */
short int validarIdentificador(char *identificador)
{
    char identificadorMaiusculo[MAX_IDENTIFICADOR]; //Variável que irá guardar o identificador convertido para maiúsculo, para simplificar a comparação com o nome e sobrenome
    char identificadorArquivo[MAX_IDENTIFICADOR];   //Variável para guardar o identificador recebido do arquivo

    //Verifica tamanho do identificador
    if (strlen(identificador) < 5 || strlen(identificador) > 15)
    {
        enviarDadosCliente("# FALHA [IDENTIFICADOR INVÁLIDO] - Não contém tamanho permitido (mínimo 5 e máximo 15)\n> Tente novamente: ");
        return 0;
    }

    //Loop para passar pelos caracteres do identificador e verificar se contém caracteres válidos
    for (int i = 0; i < strlen(identificador); i++)
    {
        if (!isalnum(identificador[i]) && identificador[i] != '.')
        {
            enviarDadosCliente("# FALHA [IDENTIFICADOR INVÁLIDO] - Contém caracteres não permitidos\n# O identificador pode conter somente caracteres alfanuméricos e ponto final.\n> Tente novamente: ");
            return 0;
        }
    }

    strcpy(identificadorMaiusculo, identificador);    //Copiando o identificador para transformar em maiúsculo
    alternarCapitalLetras(identificadorMaiusculo, 1); //Tornando maiúsculo
    //Se o identificador for igual ao nome ou sobrenome, é inválido.
    if (!strcmp(identificadorMaiusculo, u.nome) || !strcmp(identificadorMaiusculo, u.sobrenome))
    {
        enviarDadosCliente("# FALHA [IDENTIFICADOR INVÁLIDO] - Identificador não pode ser seu nome ou sobrenome!\n> Tente novamente: ");
        return 0;
    }

    /*Verifica se o identificador já foi utilizado*/
    //Validação para caso o arquivo não possa ser aberto.
    if (testarArquivo(arquivoUsuarios))
        return 0;

    ponteiroArquivos = fopen(arquivoUsuarios, "r"); //Abrir o arquivo com parâmetro "r" de read, apenas lê

    //Passa pelas linhas do arquivo verificando os identificadores já inseridos
    while (!feof(ponteiroArquivos))
    {
        //Lê linha por linha colocando os valores no formato, nas variáveis
        fscanf(ponteiroArquivos, "%[^|]|%[^|]|%s", temp, identificadorArquivo, temp);

        alternarCapitalLetras(identificadorArquivo, 1); //Transformar o identificador recebido do arquivo em maiúsculo também para comparar com o que está tentando cadastrar
        //Realizando a comparação dos identificadores
        if (!strcmp(identificadorArquivo, identificadorMaiusculo))
        {
            //Se entrar aqui o identificador já foi utilizado
            enviarDadosCliente("# FALHA [IDENTIFICADOR INVÁLIDO] - Já está sendo utilizado!\n> Tente novamente: ");
            fecharArquivo(ponteiroArquivos);
            enviarDadosCliente("0");
            return 0;
        }
    }
    fecharArquivo(ponteiroArquivos); //Fecha o arquivo
    //Se chegou até aqui, passou pelas validações, retorna 1 - true
    enviarDadosCliente("> SUCESSO: identificador aprovado!");
    return 1;
}

/**
 * Função para verificar se a senha cumpre com a política de senhas
 * @param senha é a string da senha que quer validar
 * @return valor 1 em caso de senha válida; 0 caso inválida
 */
short int validarSenha(char *senha)
{
    //Contadores dos tipos de caracteres que a senha possui
    int contminusculas = 0, contmaiusculas = 0, contNumeros = 0, contEspeciais = 0;
    char confirmacaoSenha[MAX_SENHA]; //Variável que receberá a confirmação da senha

    //Verifica tamanho da senha
    if (strlen(senha) < 8 || strlen(senha) > 30)
    {
        enviarDadosCliente("# FALHA [SENHA INVÁLIDA] - Não contém tamanho permitido (mínimo 8 e máximo 30)\n> Tente novamente: ");
        return 0;
    }

    //Loop para passar pelos caracteres da senha
    for (int i = 0; i < strlen(senha); i++)
    {
        /*Sequencia de condições para verificar cada caractere da senha*/
        //Usando a função islower da biblioteca ctype.h, é possível verificar se o caractere é alfabético e minúsculo
        if (islower(senha[i]))
        {
            contminusculas++;
        }
        //Usando a função isupper da biblioteca ctype.h, é possível verificar se o caractere é alfabético e maiúsculo
        else if (isupper(senha[i]))
        {
            contmaiusculas++;
        }
        //Usando a função isdigit da biblioteca ctype.h, é possível verificar se o caractere é um dígito
        else if (isdigit(senha[i]))
        {
            contNumeros++;

            //Verifica se a senha contém +2 números ordenados em sequência (ascendente ou descendente)
            if (((senha[i] - '0') + 1 == senha[i + 1] - '0' && (senha[i] - '0') + 2 == senha[i + 2] - '0') || ((senha[i] - '1') == senha[i + 1] - '0' && (senha[i] - '2') == senha[i + 2] - '0'))
            {
                enviarDadosCliente("# FALHA [SENHA INVÁLIDA] - contém números ordenados em sequência\n> Tente novamente: ");
                return 0;
            }

            //Verifica se a senha contém +2 números repetidos em sequência
            if (senha[i] == senha[i + 1] && senha[i] == senha[i + 2])
            {
                enviarDadosCliente("# FALHA [SENHA INVÁLIDA] - contém números repetidos em sequência\n> Tente novamente: ");
                return 0;
            }
        }
        //Verificando se o caractere é especial ou espaço
        else if (ispunct(senha[i]) || isspace(senha[i]))
        {
            contEspeciais++;
        }
        else
        {
            enviarDadosCliente("# FALHA [SENHA INVÁLIDA] - Sua senha contém caracteres que nao são nem alfanuméricos nem especiais ou espaço.\n# Verifique a digitação e tente novamente.\n# Caracteres permitidos:\n#\tEspeciais: ! \" # $ %% & ' ( ) * + , - . / : ; < = > ? @ [ \\ ] ^ _ ` { | } ~ [ESPAÇO]\n#\tNuméricos: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9\n#\tAlfabéticos: a b c d e f g h i j k l m n o p q r s t u v w x y z #\t\t\tA B C D E F G H I J K L M N O P Q R S T U V W X Y Z\n> Tente novamente: ");
            return 0;
        }
    } //Fim do loop que passa pelos caracteres da senha

    //Valida a quantidade de caracteres especiais
    if (contEspeciais < 2)
    {
        enviarDadosCliente("# FALHA [SENHA INVÁLIDA] - Não contém caracteres especiais suficientes\n> Tente novamente: ");
        return 0;
    }
    //Verifica se contém números e letras
    if ((contminusculas + contmaiusculas) == 0 || contNumeros == 0)
    {
        enviarDadosCliente("# FALHA [SENHA INVÁLIDA] - Não contém letras e números\n> Tente novamente: ");
        return 0;
    }
    //Verifica se contém minúsculas
    if (contminusculas == 0)
    {
        enviarDadosCliente("# FALHA [SENHA INVÁLIDA] - Não contém qualquer letra minúscula\n> Tente novamente: ");
        return 0;
    }
    //Verifica se contém maiúsculas
    if (contmaiusculas == 0)
    {
        enviarDadosCliente("# FALHA [SENHA INVÁLIDA] - Não contém qualquer letra maiúscula\n> Tente novamente: ");
        return 0;
    }

    enviarDadosCliente("> SUCESSO - Sua senha está de acordo com a política e foi aprovada!\n");
    return 1;
}

/**
 * Gera o valor de salt e insere ele na variável salt do usuário atual
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
        perror("# ERRO - A geração de caracteres para criação do salt não foi bem sucedida\n");
        return;
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
 *  @param nomeArquivo: nome do arquivo que se deseja testar
 *  @return 1 caso o arquivo não possa ser acessado; 0 caso contrário
 */
short int testarArquivo(char *nomeArquivo)
{
    FILE *arquivo = fopen(nomeArquivo, "a");
    if (arquivo == NULL)
    {
        printf("# ERRO - O arquivo '%s' não pode ser acessado, verifique.\n", nomeArquivo);
        return 1;
    }
    fecharArquivo(arquivo);
    return 0;
}

/**
 * Faz a atualização do arquivo, salvando as linhas que não são a linha obsoleta e quando a encontra, salva a linha atualizada no lugar dela
 * @param arquivo é o arquivo que se deseja utilizar para substituir a linha
 * @param linhaObsoleta é a linha que está com dados desatualizados
 * @param linhaAtualizada é a linha com dados novos que irão no lugar da linha obsoleta
 */
void atualizarLinhaArquivo(char *arquivo, char *linhaObsoleta, char *linhaAtualizada)
{
    //Validação antes de acessar os arquivos
    if (testarArquivo(arquivo) || testarArquivo("transferindo.txt"))
        return;

    FILE *entrada = fopen(arquivo, "r");          //Arquivo de entrada
    FILE *saida = fopen("transferindo.txt", "w"); //Arquivo de saída
    char texto[MAX];                              //Uma string grande para armazenar as linhas lidas

    //Loop pelas linhas do arquivo, procurando a linha obsoleta
    while (fgets(texto, MAX, entrada) != NULL)
    {
        //Se a linha sendo lida no arquivo for diferente da linha obsoleta, ela será apenas copiada para o arquivo de saída
        if (strcmp(linhaObsoleta, texto))
        {
            fputs(texto, saida);
        }
        else
        {
            fputs(linhaAtualizada, saida);
        }
    }

    //Fecha os arquivos
    fecharArquivo(entrada);
    fecharArquivo(saida);
    //Deleta o arquivo com os dados antigos
    if (remove(arquivo))
    {
        system("cls || clear");
        printf("# ERRO - Ocorreu um erro ao deletar um arquivo necessário, o programa foi abortado.\n");
        finalizarPrograma();
    }
    //Renomeia o arquivo onde foram passadas as linhas que não seriam excluidas para o nome do arquivo de dados de entrada
    if (rename("transferindo.txt", arquivo))
    {
        system("cls || clear");
        printf("# ERRO - Ocorreu um erro ao renomear um arquivo necessário, o programa foi abortado.\n");
        finalizarPrograma();
    }
    printf("# SUCESSO - Os dados foram atualizados!\n");
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
    printf("# SISTEMA FINALIZADO.\n");
    exit(0);
}

/**
 * Fecha o arquivo verificando se não houve erro ao fechar, se houver encerra o programa para prevenir problemas posteriores
 */
void fecharArquivo(FILE *arquivo)
{
    if (fclose(arquivo))
    {
        system("cls || clear");
        printf("# ERRO - Ocorreu um erro ao fechar um arquivo necessário, o programa foi abortado para prevenir mais erros.");
        printf("# Contate o desenvolvedor para verificar o problema.");
        finalizarConexaoCliente();
        finalizarPrograma();
    }
}