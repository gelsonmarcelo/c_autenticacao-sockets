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
int conexao; //Guarda o retorno de accept, um arquivo descritor do socket conectado

struct Usuario u; //Declaração da estrutura do usuário

/*Declaração das funções*/

char *receberDadosCliente(char *idSincronia, char *dadoEsperado);
void enviarDadosCliente(char *idSincronia, char *dadosServidor);
short int autenticar();
void cadastrarUsuario();
void coletarDados();
int pegarProximoId(char *arquivo, char *idSincronia);
short int validarStringPadrao(char *string, char *idSincronia);
short int validarStringEmail(char *string);
short int validarIdentificador(char *identificador);
short int validarSenha(char *senha);
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
void finalizarConexao();
void finalizarPrograma();

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
    setlocale(LC_ALL, "Portuguese"); //Permite a utilização de acentos e caracteres do português nas impressões

    //Verifica arquivos necessários para o programa iniciar
    if (testarArquivo(arquivoUsuarios, ""))
    {
        printf("# ERRO FATAL - O arquivo de dados não pode ser manipulado, o servidor não pode ser iniciado.\n");
        finalizarPrograma();
    }

    /******* CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/
    int socketDescritor;                  //Descritor do socket
    int socketSize;                       //Guarda o tamanho da estrutura do socket
    struct sockaddr_in servidor, cliente; //Estruturas de cliente e servidor, para guardar seus dados
    char *clienteIp;                      //Para pegar o IP do cliente
    int clientePorta;                     //Para pegar porta do cliente

    //Cria um socket
    socketDescritor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketDescritor == -1)
    {
        perror("# ERRO FATAL - Falha ao criar socket");
        finalizarPrograma();
    }

    // printf("§ Socket do servidor criado com descritor: %d\n", socketDescritor);

    //Define as propriedades da struct do socket
    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY; // Obtem IP do Sistema Operacional
    servidor.sin_port = htons(PORTA);

    //Trata o erro de porta já está em uso
    int sim = 1;
    if (setsockopt(socketDescritor, SOL_SOCKET, SO_REUSEADDR, &sim, sizeof(int)) == -1)
    {
        perror("# ERRO FATAL - Erro na porta do socket");
        finalizarPrograma();
    }

    //Associa o socket à porta
    if (bind(socketDescritor, (struct sockaddr *)&servidor, sizeof(servidor)) == -1)
    {
        perror("# ERRO FATAL - Erro ao fazer bind");
        finalizarPrograma();
    }

    puts("> SERVIDOR INICIADO...");
    pausarPrograma();
    system("clear");

    // Aguarda conexões de clientes
    if (listen(socketDescritor, 1) == -1)
    {
        perror("# ERRO FATAL- Erro ao ouvir conexões:");
        finalizarPrograma();
    }
    
    do
    {
        imprimirDecoracao();
        puts("> SERVIDOR ONLINE...");
        printf("> Ouvindo na porta %d", PORTA);
        //Aceita e trata conexões
        puts("\n> Aguardando por conexões...");
        socklen_t clienteLenght = sizeof(cliente);
        if ((conexao = accept(socketDescritor, (struct sockaddr *)&cliente, &clienteLenght)) == -1)
        {
            perror("# ERRO FATAL - Falha ao aceitar conexão");
            finalizarPrograma();
        }

        //Obtém IP e porta do cliente
        clienteIp = inet_ntoa(cliente.sin_addr);
        clientePorta = ntohs(cliente.sin_port);
        /******* FIM - CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/

        imprimirDecoracao();
        printf(">> >> CLIENTE CONECTADO >> >>\n");
        printf("> Cliente IP:PORTA = %s:%d", clienteIp, clientePorta);

        int operacao = 0; //Recebe um número que o usuário digitar para escolher a opção do menu
        short int encerrarConexao = 0;

        //Loop do menu de operações
        do
        {
            imprimirDecoracao();
            puts("\t> MENU DE OPERAÇÕES: NÃO AUTENTICADO <");
            operacao = atoi(receberDadosCliente("op", "operação do menu"));

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
                printf("\t\t>> AUTENTICAÇÃO <<\n");
                if (autenticar())
                {
                    encerrarConexao = areaLogada();
                    limparEstruturaUsuario();
                }
                break;
            case 2: //Cadastro
                imprimirDecoracao();
                printf("\t\t>> CADASTRO <<\n");
                cadastrarUsuario();
                break;
            case 3: //Ver política
                imprimirDecoracao();
                printf("\t>> POLÍTICA DE IDENTIFICADORES E SENHAS <<\n");
                imprimirDecoracao();
                mostrarPoliticaSenhas();
                break;
            default:
                puts("\n# INFO - Usuário enviou um comando inválido");
                break;
            }

            if (encerrarConexao)
                break;
        } while (1);
        /* ***Código caso deseje parar o servidor a cada conexão encerrada***
        imprimirDecoracao();
        puts("<?> Deseja finalizar o servidor [s/n]?");
        if (getchar() == 's')
        {
            getchar();
            setbuf(stdin, NULL);
            finalizarPrograma();
        }
        getchar();
        setbuf(stdin, NULL);*/
    } while (1);
    return EXIT_SUCCESS;
}

/**
 * Lê dados escritos pelo cliente no socket de comunicação, separa e analisa o id de sincronização e retorna a mensagem recebida
 * @param idSincronia é o identificador que será comparado com a mensagem recebida do cliente, para saber se a mensagem é um dado esperado
 * @param dadosEsperado apenas um breve descritivo para que possa ser acompanhado, no console do servidor, qual dado ele está aguardando
 * @return a mensagem recebida do cliente
 */
char *receberDadosCliente(char *idSincronia, char *dadoEsperado)
{
    int tamanho; //Guarda o tamanho da mensagem recebida pelo cliente
    char *msgClienteLocal = malloc(MAX_MSG);

    printf("> Aguardando: %s...\n", dadoEsperado);

    //Lê dados enviados pelo cliente
    if ((tamanho = read(conexao, msgClienteLocal, MAX_MSG)) < 0)
    {
        perror("# ERRO FATAL - Falha ao receber dados do cliente");
        finalizarConexao();
        finalizarPrograma();
    }

    msgClienteLocal[tamanho] = '\0'; //Adiciona NULL no final da string recebida

    // printf("\n§ msgCliente Recebida: %s\n", msgClienteLocal);

    //Separa o identificador de sincronização e compara se era esse identificador que o programa estava esperando
    if (strcmp(strsep(&msgClienteLocal, "/"), idSincronia))
    {
        //Se não for, houve algum problema na sincronização cliente/servidor e eles não estão comunicando corretamente
        puts("\n# ERRO FATAL - Servidor e cliente não estão sincronizados...");
        finalizarConexao();
        finalizarPrograma();
    }
    return msgClienteLocal; //Retorna somente a parte da mensagem recebida, após o /
}

/**
 * Escreve uma mensagem no socket a ser lido pelo cliente
 * @param idSincronia é o identificador que será verificado no cliente para saber se a mensagem recebida é um dado esperado
 * @param dadosServidor é a string com dados que o cliente precisa enviar
 */
void enviarDadosCliente(char *idSincronia, char *dadosServidor)
{
    char *msgServidor = malloc(MAX_MSG);                       //Armazena a mensagem a ser enviada contendo id e dados

    //Verificar o tamanho da string a ser enviada
    if (strlen(msgServidor) > (MAX_MSG-strlen(idSincronia)-1))
    {
        printf("# ERRO FATAL - Os dados que o servidor tentou enviar excederam o limite de caracteres, envio cancelado.\n");
        finalizarConexao();
        finalizarPrograma();
    }

    sprintf(msgServidor, "%s/%s", idSincronia, dadosServidor); //Insere o id de sincronização e dados em formato específico na mensagem

    //Escreve no socket de comunicação e valida se não houve erro
    if (write(conexao, msgServidor, strlen(msgServidor)) < 0)
    {
        printf("# ERRO - Falha ao enviar mensagem:\n\"%s\"\n", msgServidor);
        return;
    }
    printf("\n> Mensagem enviada para o cliente:\n\"%s\"\n\n", msgServidor);
    free(msgServidor); //Libera a memória alocada
}

/**
 * Encerra a comunicação com o cliente atual
 */
void finalizarConexao()
{
    imprimirDecoracao();
    if (shutdown(conexao, SHUT_RDWR))
    {
        perror("# ERRO - Falha ao desativar comunicação");
        return;
    }
    //Fecha o socket
    if (close(conexao))
    {
        perror("# ERRO - Não foi possível fechar a comunicação");
        return;
    }
    puts("\n<< << CLIENTE DESCONECTADO << <<");
}

/**
 * Busca no arquivo o último ID cadastrado e retorna o próximo ID a ser usado
 * @param arquivo é o char com nome do arquivo que deseja utilizar para verificar o próximo ID
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

    //O ID lido por último é o último ID cadastrado e será somado mais 1 e retornado para cadastrar o próximo
    return id + 1;
}

/**
 * Função para cadastrar novo usuário, chama função para solicitar todos os dados ao usuário e insere no arquivo de dados dos usuários
 */
void cadastrarUsuario()
{
    //Validação para caso o arquivo não possa ser aberto parar antes de pedir todos os dados ao usuário
    if (testarArquivo(arquivoUsuarios, "rf"))
        return;

    printf("\n> Usuário deve fornecer as informações necessárias para efetuar o cadastro:\n");
    coletarDados();

    u.codigo = pegarProximoId(arquivoUsuarios, "rf"); //Define o ID do usuário que está se cadastrando

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
 * Coletar dados de cadastro do usuário
 */
void coletarDados()
{
    char entradaTemp[MAX];                           //Variável para fazer a entrada de valores digitados e realizar a validação antes de atribuir à variável correta
    memset(&entradaTemp[0], 0, sizeof(entradaTemp)); //Limpando a variável para garantir que não entre lixo de memória

    /*Solicita o nome do usuário*/
    //Loop para validação do nome, enquanto a função que valida a string retornar 0 (falso) o loop vai continuar (há uma negação na frente do retorno da função)
    printf("\n> PRIMEIRO NOME: \n");
    do
    {
        setbuf(stdin, NULL);                                               //Limpa a entrada par evitar lixo
        strcpy(entradaTemp, receberDadosCliente("rp", "nome do usuário")); //Leitura do teclado, o %[^\n] vai fazer com que o programa leia tudo que o usuário digitar exceto ENTER (\n), por padrão pararia de ler no caractere espaço
    } while (!validarStringPadrao(entradaTemp, "rp"));
    //Se sair do loop é porque a string é válida e pode ser copiada para a variável correta que irá guardar
    strcpy(u.nome, entradaTemp);
    //Transformar o nome em maiúsculo para padronização do arquivo
    alternarCapitalLetras(u.nome, 1);
    //Limpar a variável temporária para receber a próxima entrada
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    printf("\n> ÚLTIMO SOBRENOME: \n");
    do
    {
        // printf("\n> Informe seu último sobrenome: ");
        setbuf(stdin, NULL);
        strcpy(entradaTemp, receberDadosCliente("ru", "sobrenome do usuário"));
    } while (!validarStringPadrao(entradaTemp, "ru"));
    strcpy(u.sobrenome, entradaTemp);
    alternarCapitalLetras(u.sobrenome, 1);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    printf("\n> E-MAIL: \n");
    do
    {
        setbuf(stdin, NULL);
        strcpy(entradaTemp, receberDadosCliente("re", "e-mail do usuário"));
    } while (!validarStringEmail(entradaTemp));
    strcpy(u.email, entradaTemp);
    alternarCapitalLetras(u.email, 1);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Aguarda identificador
    printf("\n> IDENTIFICADOR: \n");
    do
    {
        // printf("\n> Crie seu login: ");
        setbuf(stdin, NULL);
        strcpy(entradaTemp, receberDadosCliente("ri", "identificador do usuário"));
    } while (!validarIdentificador(entradaTemp));
    strcpy(u.identificador, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));

    //Solicita o usuário crie uma senha

    printf("\n> SENHA: \n");
    do
    {
        setbuf(stdin, NULL);
        //getpass() é usado para desativar o ECHO do console e não exibir a senha sendo digitada pelo usuário, retorna um ponteiro apontando para um buffer contendo a senha, terminada em '\0' (NULL)
        // puts("\n> Crie uma senha: ");
        strcpy(entradaTemp, receberDadosCliente("rs", "senha do usuário"));
    } while (!validarSenha(entradaTemp));
    strcpy(u.senha, entradaTemp);
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));
}

/**
 * Realizar a autenticação do usuário
 * @return 1 em caso de sucesso e; 0 em outros casos
 */
short int autenticar()
{
    //Variáveis que guardam os dados lidos nas linhas do arquivo
    char identificadorArquivo[MAX_IDENTIFICADOR], saltArquivo[SALT_SIZE + 1], criptografiaArquivo[120], usuarioArquivo[MAX_DADOS], sobrenomeArquivo[MAX_DADOS], emailArquivo[MAX_DADOS];
    int codigoArquivo = 0;
    do
    {
        puts("# LOGIN: ");
        strcpy(u.identificador, receberDadosCliente("li", "identificador"));
        puts("# SENHA: ");
        strcpy(u.senha, receberDadosCliente("ls", "senha"));

        //Validação para caso o arquivo não possa ser aberto
        if (testarArquivo(arquivoUsuarios, "au"))
            return 0;

        //Abrir o arquivo com parâmetro "r" de read, apenas lê
        ponteiroArquivos = fopen(arquivoUsuarios, "r");
        int i = 0;
        //Loop que passa por todas as linhas do arquivo até o final
        while (!feof(ponteiroArquivos))
        {
            //Define os dados da linha lida nas variáveis na respectiva ordem do padrão lido
            if (i = fscanf(ponteiroArquivos, ">%d|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^\n]\n", &codigoArquivo, identificadorArquivo, saltArquivo, criptografiaArquivo, usuarioArquivo, sobrenomeArquivo, emailArquivo) != 7)
            {
                printf("\n# ERRO - Não foi possível interpretar completamente a linha do arquivo. %d, %s, %s, %s, %s, %s, %s", codigoArquivo, identificadorArquivo, saltArquivo, criptografiaArquivo, usuarioArquivo, sobrenomeArquivo, emailArquivo);
            }

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
                strcpy(u.email, emailArquivo);
                strcpy(u.identificador, identificadorArquivo);
                strcpy(u.salt, saltArquivo);
                strcpy(u.senhaCriptografada, criptografiaArquivo);
                //Salvar o formato da linha do usuário autenticado
                sprintf(u.linhaUsuario, ">%d|%s|%s|%s|%s|%s|%s", u.codigo, u.identificador, u.salt, u.senhaCriptografada, u.nome, u.sobrenome, u.email);
                fecharArquivo(ponteiroArquivos, "au"); //Fecha o arquivo
                enviarDadosCliente("au", u.linhaUsuario);
                return 1;
            }
        }
        //Se sair do loop é porque não autenticou por erro de login/senha
        fecharArquivo(ponteiroArquivos, "au");
        enviarDadosCliente("au", "# FALHA - Usuário e/ou senha incorretos!");

        if (!atoi(receberDadosCliente("cm", "confirmação para repetir autenticação")))
        {
            break;
        }
    } while (1);
    return 0;
}

/**
 * Opções para o usuário autenticado
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
        operacao = atoi(receberDadosCliente("ap", "operação do menu"));

        if (operacao == 0)
        {
            finalizarConexao();
            break;
        }

        //Escolhe a operação conforme o valor que o usuário digitou
        switch (operacao)
        {
        case 1: //Logout
            imprimirDecoracao();
            return 0;
        case 2: //Excluir Cadastro
            imprimirDecoracao();
            printf("\n\t\t>> EXCLUIR CADASTRO <<\n");
            if (atoi(receberDadosCliente("cd", "confirmação do usuário para deletar cadastro")))
                if (excluirUsuario())
                {
                    return 0;
                }
            puts("# OPERAÇÃO CANCELADA");
            break;
        case 3: //Ver dados do usuário
            imprimirDecoracao();
            printf("\n\t\t>> VER DADOS DO USUÁRIO <<\n");
            verDadosUsuario();
            break;
        case 4: //Cadastro
            imprimirDecoracao();
            printf("\n\t>> VER POLÍTICA DE IDENTIFICADORES E SENHAS <<\n");
            mostrarPoliticaSenhas();
            break;
        default:
            printf("# INFO - Usuário enviou um comando inválido\n");
            break;
        }
    } while (1);
    return 1;
}

/**
 * Excluir os dados do usuário do arquivo de dados
 * @return 1 se o cadastro foi deletado; 0 se foi cancelado pelo usuário ou por falha no arquivo
 */
short int excluirUsuario()
{
    //Validação para caso os arquivos não possa ser acessados
    if (testarArquivo(arquivoUsuarios, "dl") || testarArquivo("transferindo.txt", "dl"))
    {
        return 0;
    }

    ponteiroArquivos = fopen(arquivoUsuarios, "r"); //Arquivo de entrada
    FILE *saida = fopen("transferindo.txt", "w");   //Arquivo de saída
    char texto[MAX];                                //Uma string grande para armazenar as linhas lidas
    short int flag = 0;

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
            flag = 1;
        }
    }
    //Fecha os arquivos
    fecharArquivo(ponteiroArquivos, "dl");
    fecharArquivo(saida, "dl");

    //Deleta o arquivo com os dados antigos
    if (remove(arquivoUsuarios))
    {
        printf("\n# ERRO - Ocorreu um problema ao deletar um arquivo necessário, o programa foi abortado.\n");
        enviarDadosCliente("dl", "# ERRO - Ocorreu um problema ao deletar um arquivo necessário, o servidor foi abortado.\n");
        finalizarPrograma();
    }
    //Renomeia o arquivo onde foram passadas as linhas que não seriam excluidas para o nome do arquivo de dados de entrada
    if (rename("transferindo.txt", arquivoUsuarios))
    {
        printf("\n# ERRO - Ocorreu um problema ao renomear um arquivo necessário, o programa foi abortado.\n");
        enviarDadosCliente("dl", "# ERRO - Ocorreu um problema ao renomear um arquivo necessário, o servidor foi abortado.\n");
        finalizarPrograma();
    }

    if (flag)
    {
        limparEstruturaUsuario();
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
 * Imprime os dados do usuário
 * @param idUsuario ID do usuário que se deseja exibir os dados
 */
void verDadosUsuario()
{
    char *dadosUsuario = malloc(MAX_MSG);
    sprintf(dadosUsuario, "¬ Código: %d\n¬ Nome Completo: %s %s\n¬ E-mail: %s\n¬ Login: %s\n¬ Salt: %s\n¬ Senha Criptografada: %s", u.codigo, u.nome, u.sobrenome, u.email, u.identificador, u.salt, u.senhaCriptografada);
    enviarDadosCliente("du", dadosUsuario);
}

void mostrarPoliticaSenhas()
{
    enviarDadosCliente("pt", "\n\t\tIDENTIFICADOR/LOGIN\n\n-Não pode ser utilizado nome, sobrenome ou email;\n-Pode conter somente caracteres alfanuméricos e ponto final;\n-Deve ter no mínimo 5 caracteres e no máximo 15.\n\n\t\t\tSENHA\n\n-Deve conter no mínimo 8 caracteres e no máximo 30;\n-Deve conter, no mínimo, 2 caracteres especiais;\n-Deve conter números e letras;\n-Deve conter pelo menos uma letra maiúscula e uma minúscula;\n-Não pode conter mais de 2 números ordenados em sequência (Ex.: 123, 987);\n-Não pode conter mais de 2 números repetidos em sequência (Ex.: 555, 999);\n-Não pode conter caracteres que não sejam alfanuméricos (números e letras), especiais ou espaço.\n");
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
short int validarStringPadrao(char *string, char *idSincronia)
{
    //Verifica o tamanho da string
    if (strlen(string) > 50 || strlen(string) < 2)
    {
        enviarDadosCliente(idSincronia, "# FALHA [QUANTIDADE DE CARACTERES]: esse dado deve conter no mínimo 2 letras e no máximo 50!\n> Tente novamente: ");
        return 0;
    }

    //Loop para passar pelos caracteres da string verificando se são alfabéticos
    for (int i = 0; i < strlen(string); i++)
    {
        //Usando a função isalpha da biblioteca ctype.h, é possível verificar se o caractere é alfabético
        if (!isalpha(string[i]))
        {
            enviarDadosCliente(idSincronia, "# FALHA [CARACTERE INVÁLIDO]: insira somente caracteres alfabéticos nesse campo, sem espaços ou pontuações\n> Tente novamente: ");
            return 0;
        }
    }
    //Se chegou aqui é válido
    enviarDadosCliente(idSincronia, "> SUCESSO: palavra aprovada!");
    return 1;
}

/**
 * Verifica se a string passada como parâmetro contém formato permitido para e-mail
 * @return 1 caso a string seja válida; 0 caso string inválida
 */
short int validarStringEmail(char *string)
{
    //Variáveis para guardar as informações digitadas separadas
    char usuario[256], site[256], dominio[256];

    //Valida tamanho da string
    if (strlen(string) > 50 || strlen(string) < 6)
    {
        enviarDadosCliente("re", "# FALHA [QUANTIDADE DE CARACTERES]: verifique o dado digitado, deve conter entre 6 e 50 caracteres\n> Tente novamente: ");
        return 0;
    }

    // sscanf lê a entrada a partir da string no primeiro parâmetro, atribuindo para as variáveis. Retorna o número de campos convertidos e atribuídos com êxito.
    if (sscanf(string, "%[^@ \t\n]@%[^. \t\n].%3[^ \t\n]", usuario, site, dominio) != 3)
    {
        enviarDadosCliente("re", "# FALHA [E-MAIL INVÁLIDO]: verifique o e-mail digitado\n> Tente novamente: ");
        return 0;
    }
    //Se chegou aqui é porque a string é válida
    enviarDadosCliente("re", "> SUCESSO: e-mail aprovado!");
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
        enviarDadosCliente("ri", "# FALHA [IDENTIFICADOR INVÁLIDO] - Não contém tamanho permitido (mínimo 5 e máximo 15)\n> Tente novamente: ");
        return 0;
    }

    //Loop para passar pelos caracteres do identificador e verificar se contém caracteres válidos
    for (int i = 0; i < strlen(identificador); i++)
    {
        if (!isalnum(identificador[i]) && identificador[i] != '.')
        {
            enviarDadosCliente("ri", "# FALHA [IDENTIFICADOR INVÁLIDO] - Contém caracteres não permitidos\n# O identificador pode conter somente caracteres alfanuméricos e ponto final.\n> Tente novamente: ");
            return 0;
        }
    }

    strcpy(identificadorMaiusculo, identificador);    //Copiando o identificador para transformar em maiúsculo
    alternarCapitalLetras(identificadorMaiusculo, 1); //Tornando maiúsculo
    //Se o identificador for igual ao nome ou sobrenome, é inválido.
    if (!strcmp(identificadorMaiusculo, u.nome) || !strcmp(identificadorMaiusculo, u.sobrenome))
    {
        enviarDadosCliente("ri", "# FALHA [IDENTIFICADOR INVÁLIDO] - Identificador não pode ser seu nome ou sobrenome!\n> Tente novamente: ");
        return 0;
    }

    /*Verifica se o identificador já foi utilizado*/
    //Validação para caso o arquivo não possa ser aberto.
    if (testarArquivo(arquivoUsuarios, "ri"))
        return 0;

    ponteiroArquivos = fopen(arquivoUsuarios, "r"); //Abrir o arquivo com parâmetro "r" de read, apenas lê

    //Passa pelas linhas do arquivo verificando os identificadores já inseridos
    while (!feof(ponteiroArquivos))
    {
        //Lê linha por linha colocando os valores no formato, nas variáveis
        fscanf(ponteiroArquivos, ">%[^|]|%[^|]|%[^\n]\n", temp, identificadorArquivo, temp);

        alternarCapitalLetras(identificadorArquivo, 1); //Transformar o identificador recebido do arquivo em maiúsculo também para comparar com o que está tentando cadastrar
        //Realizando a comparação dos identificadores
        if (!strcmp(identificadorArquivo, identificadorMaiusculo))
        {
            //Se entrar aqui o identificador já foi utilizado
            fecharArquivo(ponteiroArquivos, "ri");
            enviarDadosCliente("ri", "# FALHA [IDENTIFICADOR INVÁLIDO] - Já está sendo utilizado!\n> Tente novamente: ");
            return 0;
        }
    }
    fecharArquivo(ponteiroArquivos, "ri"); //Fecha o arquivo
    //Se chegou até aqui, passou pelas validações, retorna 1 - true
    enviarDadosCliente("ri", "> SUCESSO: identificador aprovado!");
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
        enviarDadosCliente("rs", "# FALHA [SENHA INVÁLIDA] - Não contém tamanho permitido (mínimo 8 e máximo 30)");
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
                enviarDadosCliente("rs", "# FALHA [SENHA INVÁLIDA] - contém números ordenados em sequência");
                return 0;
            }

            //Verifica se a senha contém +2 números repetidos em sequência
            if (senha[i] == senha[i + 1] && senha[i] == senha[i + 2])
            {
                enviarDadosCliente("rs", "# FALHA [SENHA INVÁLIDA] - contém números repetidos em sequência");
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
            enviarDadosCliente("rs", "# FALHA [SENHA INVÁLIDA] - Sua senha contém caracteres que nao são nem alfanuméricos nem especiais ou espaço.\n# Verifique a digitação e tente novamente.\n# Caracteres permitidos:\n#\tEspeciais: ! \" # $ %% & ' ( ) * + , - . / : ; < = > ? @ [ \\ ] ^ _ ` { | } ~ [ESPAÇO]\n#\tNuméricos: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9\n#\tAlfabéticos: a b c d e f g h i j k l m n o p q r s t u v w x y z #\t\t\tA B C D E F G H I J K L M N O P Q R S T U V W X Y Z");
            return 0;
        }
    } //Fim do loop que passa pelos caracteres da senha

    //Valida a quantidade de caracteres especiais
    if (contEspeciais < 2)
    {
        enviarDadosCliente("rs", "# FALHA [SENHA INVÁLIDA] - Não contém caracteres especiais suficientes");
        return 0;
    }
    //Verifica se contém números e letras
    if ((contminusculas + contmaiusculas) == 0 || contNumeros == 0)
    {
        enviarDadosCliente("rs", "# FALHA [SENHA INVÁLIDA] - Não contém letras e números");
        return 0;
    }
    //Verifica se contém minúsculas
    if (contminusculas == 0)
    {
        enviarDadosCliente("rs", "# FALHA [SENHA INVÁLIDA] - Não contém qualquer letra minúscula");
        return 0;
    }
    //Verifica se contém maiúsculas
    if (contmaiusculas == 0)
    {
        enviarDadosCliente("rs", "# FALHA [SENHA INVÁLIDA] - Não contém qualquer letra maiúscula");
        return 0;
    }

    //Solicita a confirmação da senha
    enviarDadosCliente("rs", "* Solicitar a confirmação de senha...");
    //Compara as 2 senhas informadas, se forem diferentes vai retornar != 0, entrando na condição
    if (strcmp(receberDadosCliente("rs", "confirmação da senha"), senha))
    {
        enviarDadosCliente("rs", "# FALHA [SENHAS INCOMPATÍVEIS] - As senhas não coincidem");
        return 0;
    }

    enviarDadosCliente("rs", "> SUCESSO - Sua senha está de acordo com a política e foi aprovada!");
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
        perror("# ERRO - A geração de caracteres para criação do salt não foi bem sucedida");
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
short int testarArquivo(char *nomeArquivo, char *idSincronia)
{
    FILE *arquivo = fopen(nomeArquivo, "a");
    if (arquivo == NULL)
    {
        enviarDadosCliente(idSincronia, "# ERRO - Um arquivo necessário não pode ser acessado, não é possível realizar a operação.\n");
        printf("# ERRO - O arquivo '%s' não pode ser acessado, verifique.\n", nomeArquivo);
        return 1;
    }
    fecharArquivo(arquivo, idSincronia);
    return 0;
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
    imprimirDecoracao();
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    printf("# SISTEMA FINALIZADO.\n");
    exit(0);
}

/**
 * Zera os dados da estrutura do usuário para reutilização
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
 * Fecha o arquivo verificando se não houve erro ao fechar, se houver encerra o programa para prevenir problemas posteriores
 */
void fecharArquivo(FILE *arquivo, char *idSincronia)
{
    if (fclose(arquivo))
    {
        system("clear");
        printf("# ERRO - Ocorreu um erro ao fechar um arquivo necessário, o programa foi abortado.");
        enviarDadosCliente(idSincronia, "# ERRO - Ocorreu um erro ao fechar um arquivo necessário, o programa foi abortado.");
        finalizarConexao();
        finalizarPrograma();
    }
}