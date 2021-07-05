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
int socketDescritor; //Variável que guarda o descritor do socket
int maxMsg = 1024;   //Tamanho da mensagem trocada entre cliente/servidor, possui um valor default, mas poderá ser definida de acordo com o tamanho que o servidor enviar após o cliente conectar

struct Usuario u; //Declaração da estrutura do usuário

/*Declaração das funções*/

void cadastrarUsuario();
short int autenticar();
void areaLogada();
void limparEstruturaUsuario();
void imprimirDecoracao();
void pausarPrograma();
void enviarDadosServidor(char *idSincronia, char *dadosCliente);
char *receberDadosServidor(char *idSincronia, short int imprimirMsg);
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
    char salt[SALT_SIZE + 1];              //Guarda o salt do usuário
    char senha[MAX_SENHA];                 //Guarda a senha do usuário
    char senhaCriptografada[120];          //Guarda o valor da senha criptografada
};

/**
 * Função principal
 */
int main()
{
    setlocale(LC_ALL, "Portuguese"); //Permite a utilização de acentos e caracteres do português nas impressões do console

    puts("> INICIANDO CLIENTE...");
    pausarPrograma();
    system("clear");

    int operacao;            //Recebe um número que o usuário digitar para escolher a opção do menu
    char operacaoString[3];  //Usado para passar a operação digitada para o servidor
    int comprimentoServidor; //Receber tamanho máximo em bytes da comunicação, definido pelo servidor

    /******* CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/
    struct sockaddr_in servidor; //Estrutura que guarda dados do servidor que irá se conectar

    //Cria o socket do cliente
    if ((socketDescritor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("# ERRO FATAL - Criação do socket falhou");
        finalizar(0, 1);
    }

    //Atribuindo informações das configurações do servidor, na sua estrutura
    servidor.sin_addr.s_addr = inet_addr(ENDERECO_SERVIDOR); //Endereço IP
    servidor.sin_family = AF_INET;                           //Para comunicação IPV4
    servidor.sin_port = htons(PORTA);                        //Porta de conexão do servidor

    //Conecta no servidor
    if (connect(socketDescritor, (struct sockaddr *)&servidor, sizeof(servidor)) == -1)
    {
        perror("# ERRO FATAL - Não foi possível conectar no servidor");
        finalizar(0, 1);
    }
    /******* FIM-CONFIGURAÇÃO DO SOCKET E CONEXÃO *******/

    imprimirDecoracao();
    printf("\n\t\t>> CONECTADO AO SERVIDOR COM SUCESSO <<\n");

    /*Se o comprimento que o servidor mandar for menor que 4 não vai ser possível trocar informações pois não sobra espaço para os dados de fato, 
    visto que todas as mensagens possuem, no mínimo, 2 caracteres de sincronização e 1 '/' para separar da mensagem, ocupando 3 bytes */
    if ((comprimentoServidor = atoi(receberDadosServidor("mx", 0))) < 4)
    {
        printf("# ERRO - Não foi possível identificar o comprimento máximo aceitável de leitura e escrita das mensagens pelo servidor, o tamanho máximo considerado será de %d.\n", maxMsg);
    }
    else
    {
        maxMsg = comprimentoServidor;
    }

    //Loop do menu de opções
    do
    {
        operacao = '\0'; //Limpar a variável para evitar lixo de memória nas repetições
        pausarPrograma();
        system("clear");

        imprimirDecoracao();
        printf("\n\t\t>> MENU DE OPERAÇÕES <<\n");
        printf("\n> Informe um número para escolher uma opção e pressione ENTER:");
        printf("\n[1] Login");
        printf("\n[2] Cadastro");
        printf("\n[3] Ver política de identificadores e senhas");
        printf("\n[0] Encerrar programa");
        imprimirDecoracao();
        printf("\n> Informe o número: ");

        setbuf(stdin, NULL); //Limpa qualquer coisa no buffer antes de ler a entrada do usuário
        //Valida a entrada que o usuário digitou (caso não digite um número)
        while (scanf("%d", &operacao) != 1)
        {
            printf("\n# FALHA - Ocorreu um erro na leitura do valor informado.\n> Tente novamente: ");
            setbuf(stdin, NULL);
        };

        //Atribui o valor que o usuário digitou para a variável string e envia para o servidor
        sprintf(operacaoString, "%d", operacao);
        enviarDadosServidor("op", operacaoString);

        //Usuário quer encerrar o programa
        if (operacao == 0)
        {
            finalizar(1, 1);
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
                limparEstruturaUsuario(); //Quando sair da área logada precisa limpar a estrutura
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
            receberDadosServidor("pt", 1);
            break;
        default:
            imprimirDecoracao();
            printf("\n# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida, tente novamente!\n");
            break;
        }
    } while (1);
    return EXIT_SUCCESS;
}

/**
 * Escreve uma mensagem no socket a ser lido pelo servidor
 * @param idSincronia é um identificador com 2 letras, definido em ambas mensagem de envio (cliente) e recebimento (servidor) para verificar se a mensagem recebida é a 
 * informação que o programa esperava, isso define se a aplicação cliente e servidor estão sincronizadas e comunicando de forma adequada
 * @param dadosCliente é a string com dados que o cliente precisa enviar ao servidor
 */
void enviarDadosServidor(char *idSincronia, char *dadosCliente)
{
    char *msgCliente = malloc(maxMsg); //Armazena a mensagem a ser enviada, contendo id e dados

    /*Validar o comprimento da string a ser enviada, a mensagem não pode ser maior do que MAX_MSG menos o comprimento do ID de sincronia.
    Caso o valor seja maior do que o permitido, o usuário pode reinserir o dado que gerou o erro para enviar*/
    while (strlen(dadosCliente) > (maxMsg - strlen(idSincronia)))
    {
        printf("\n# ERRO - Os dados que pretende enviar ao servidor (%ld caracteres), excederam o limite de %ld caracteres, envio cancelado.\n# INFO - Digite novamente o dado que deseja enviar: ", strlen(dadosCliente), maxMsg - strlen(idSincronia));
        setbuf(stdin, NULL);           //Limpar o que está no buffer
        scanf("%[^\n]", dadosCliente); //Coletar novamente o que o usuário deseja enviar para o servidor
    }
    //Insere o id de sincronização e dados em formato específico na mensagem que será escrita no socket
    sprintf(msgCliente, "%s/%s", idSincronia, dadosCliente);

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
 * Lê a mensagem escrita pelo servidor no socket de comunicação, separa e verifica o id de sincronização e retorna a mensagem recebida
 * @param idSincronia é um identificador com 2 letras, definido em ambas mensagem de envio (servidor) e recebimento (cliente) para verificar se a mensagem recebida é a 
 * informação que o programa esperava, isso define se a aplicação cliente e servidor estão sincronizadas e comunicando de forma adequada
 * @param imprimirMsg flag que determina se, mesmo que a mensagem não seja de erro, imprima para o cliente visualizar.
 * Se a mensagem for de erro, sempre será impressa para o cliente
 * @return a mensagem recebida do cliente sem o id de sincronização
 */
char *receberDadosServidor(char *idSincronia, short int imprimirMsg)
{
    int comprimentoMsg;                 //Guarda o comprimento da mensagem recebida pelo servidor
    char *msgServidor = malloc(maxMsg); //Guarda a mensagem recebida

    //Lê dados escritos pelo servidor no socket e valida
    if ((comprimentoMsg = read(socketDescritor, msgServidor, maxMsg)) < 0)
    {
        perror("# ERRO FATAL - Falha ao receber dados do servidor");
        finalizar(1, 1);
    }
    msgServidor[comprimentoMsg] = '\0'; //Adiciona NULL no final da string

    // printf("\n§ msgCliente Recebida: %s\n", msgServidor);

    //Separa o identificador de sincronização e compara se era essa mensagem que o programa esperava receber
    if (strcmp(strsep(&msgServidor, "/"), idSincronia))
    {
        //Se não for, houve algum problema na sincronização cliente/servidor e eles não estão comunicando corretamente
        puts("\n# ERRO FATAL - Servidor e cliente não estão sincronizados...");
        finalizar(1, 1);
    }
    //Sempre que o primeiro caractere da mensagem for uma #, a mensagem precisa ser exibida para o usuário, pois indica um erro
    if (msgServidor[0] == '#')
    {
        puts(msgServidor);
    }
    else if (imprimirMsg) //Senão for erro e a flag imprimirMsg estiver ativada, deve imprimir a mensagem
    {
        puts(msgServidor);
    }
    return msgServidor; //Retorna somente a parte da mensagem recebida, após o /
}

/**
 * Função para cadastrar novo usuário, solicita os dados ao usuário e envia para o servidor
 */
void cadastrarUsuario()
{
    char entradaTemp[MAX]; //Variável para fazer a entrada de valores digitados

    //Servidor vai enviar se detectou algum erro com o arquivo de dados e se obteve sucesso em pegar o próximo ID do arquivo
    if (receberDadosServidor("fi", 0)[0] == '#')
        return;

    printf("\n> Forneça as informações necessárias para efetuar o cadastro:\n");
    //Solicita o nome do usuário
    printf("\n> Informe seu primeiro nome: ");
    //Limpar a variável temporária para receber a próxima entrada
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    //Loop para coleta e envio do nome para o servidor
    do
    {
        setbuf(stdin, NULL); //Limpa a entrada par evitar lixo
        //Leitura do teclado, o %[^\n] vai fazer com que o programa leia tudo que o usuário digitar exceto ENTER (\n), por padrão pararia de ler no caractere espaço
        scanf("%[^\n]", entradaTemp);
        enviarDadosServidor("rp", entradaTemp);        //Envia o nome para o servidor validar
    } while (receberDadosServidor("rp", 0)[0] == '#'); //Se o primeiro caractere da resposta do servidor for #, precisa repetir pois a string é inválida

    //Solicita o sobrenome do usuário
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    printf("\n> Informe seu último sobrenome: ");
    do
    {
        setbuf(stdin, NULL);
        scanf("%[^\n]", entradaTemp);
        enviarDadosServidor("ru", entradaTemp);
    } while (receberDadosServidor("ru", 0)[0] == '#');

    //Solicita o e-mail do usuário
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    printf("\n> Informe seu e-mail: ");
    do
    {
        setbuf(stdin, NULL);
        scanf("%[^\n]", entradaTemp);
        enviarDadosServidor("re", entradaTemp);
    } while (receberDadosServidor("re", 0)[0] == '#');

    //Solicita que o usuário crie um identificador
    memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    printf("\n> Crie seu login: ");
    do
    {
        setbuf(stdin, NULL);
        scanf("%[^\n]", entradaTemp);
        enviarDadosServidor("ri", entradaTemp);
    } while (receberDadosServidor("ri", 0)[0] == '#');

    //Solicita o usuário crie uma senha
    do
    {
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));
        setbuf(stdin, NULL);
        //getpass() é usado para desativar o ECHO do console e não exibir a senha sendo digitada pelo usuário, retorna um ponteiro apontando para um buffer contendo a senha, terminada em '\0' (NULL)
        strcpy(entradaTemp, getpass("\n> Crie uma senha: ")); //Coleta a senha
        enviarDadosServidor("rs", entradaTemp);               //Envia a senha para o servidor validar
        //Se o primeiro caractere da resposta do servidor for '*', servidor solicitou a confirmação da senha
        if (receberDadosServidor("rs", 0)[0] == '*')
        {
            strcpy(entradaTemp, getpass("\n> Confirme a senha: ")); //Solicita ao usuário a confirmação da senha
            enviarDadosServidor("rs", entradaTemp);                 //Envia a confirmação de senha ao servidor
            //Se o servidor responder com sucesso, o primeiro caractere da resposta será '>' e pode sair do loop
            if (receberDadosServidor("rs", 0)[0] == '>')
                break;
        }
    } while (1);
    limparEstruturaUsuario();      //Limpar dados informados pois vai voltar para o menu não autenticado
    receberDadosServidor("rf", 1); //Confirma se os dados do usuário foram inseridos no arquivo com sucesso
}

/**
 * Solicita credenciais e envia para o servidor fazer a autenticação
 * @return 1 em caso de sucesso; 0 em outros casos
 */
short int autenticar()
{
    int repetir = 0;                    //Variável para controlar se o loop da autenticação deve ser repetido
    char *msgServidor = malloc(maxMsg); //Guarda a mensagem recebida do servidor
    do
    {
        /*Coleta do login e senha para autenticação*/
        printf("\n> Informe suas credenciais:\n# LOGIN: ");
        setbuf(stdin, NULL);                        //Limpa o buffer de entrada
        scanf("%[^\n]", u.identificador);           //Realiza a leitura até o usuário pressionar ENTER
        enviarDadosServidor("li", u.identificador); //Envia o identificador para o servidor
        strcpy(u.senha, getpass("# SENHA: "));      //Lê a senha com ECHO do console desativado e copia o valor lido para a variável u.senha, do usuário
        enviarDadosServidor("ls", u.senha);         //Envia a senha para o servidor
        imprimirDecoracao();

        strcpy(msgServidor, receberDadosServidor("au", 0)); //Servidor responde se autenticou
        if (!strcmp(msgServidor, "# FALHA - Houve um problema ao ler a linha no arquivo de dados."))
        {
            return 0;
        }
        else if (msgServidor[0] == '#') //Se o primeiro caractere for '#', não autenticou
        {
            puts("\n<?> Tentar novamente? [1]Sim / [0]Não:");
            //Recebe confirmação para repetir a autenticação
            while (scanf("%d", &repetir) != 1)
            {
                //Se o usuário digitar algo que não é número
                puts("# FALHA - Digite [1] para autenticar novamente e [0] para voltar para o menu inicial: ");
                setbuf(stdin, NULL); //Limpa entrada
            }

            //Se repetir != 0
            if (repetir)
            {
                enviarDadosServidor("cm", "1"); //Envia confirmação para repetir no servidor também e cai para o final do loop, repetindo
            }
            else
            {
                enviarDadosServidor("cm", "0"); //Envia comando para interromper a função autenticar() no servidor
                return 0;                       //Interrompe a função autenticar() no cliente
            }
        }
        else if (msgServidor[0] == '>') //Senão se o primeiro caractere for '>', autenticação foi realizada com sucesso
        {
            puts("\n> SUCESSO - Login realizado!");
            //Divide a variável recebida com os dados do servidor, distribuindo nas respectivas variáveis da estrutura do usuário logado
            if (sscanf(msgServidor, ">%d|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^\n]", &u.codigo, u.identificador, u.salt, u.senhaCriptografada, u.nome, u.sobrenome, u.email) != 7)
            {
                //Se não conseguir atribuir dados à todas as variáveis...
                printf("\n# FALHA: Houve um problema ao interpretar os dados do servidor\n");
            }
            return 1; //Autenticou
        }
        else //Senão, ocorreu alguma coisa errada no servidor, há algum problema na comunicação
        {
            puts("# ERRO FATAL - O servidor enviou uma mensagem desconhecida.\n");
            finalizar(1, 1); //Encerra aplicação
        }
    } while (1);
    //Se chegar aqui de alguma forma, não autenticou
    return 0;
}

/**
 * Opções para o usuário autenticado
 */
void areaLogada()
{
    int operacao = 0;         //Recebe o valor da entrada do usuário para escolher a operação
    char operacaoString[3];   //Usado para passar a operação digitada para o servidor
    char msgServidor[maxMsg]; //Guarda a mensagem recebida do servidor
    imprimirDecoracao();
    printf("\n\t\tBEM-VINDO %s!\n", u.nome);

    //Loop do menu de opções do usuário logado
    do
    {
        pausarPrograma();
        operacao = '\0'; //Limpar a variável para evitar lixo nas repetições
        system("clear");
        //Mostra o menu
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

        setbuf(stdin, NULL); //Limpa a entrada
        //Valida a entrada que o usuário digitou
        while (scanf("%d", &operacao) != 1)
        {
            //Se for diferente de um número inteiro
            printf("\n# FALHA - Ocorreu um erro na leitura do valor informado.\n> Tente novamente: ");
            setbuf(stdin, NULL); //Limpa a entrada novamente
        };

        //Atribui o valor que o usuário digitou para a variável string e envia para o servidor
        sprintf(operacaoString, "%d", operacao);
        enviarDadosServidor("ap", operacaoString);

        system("clear"); //Limpa tela
        //Usuário escolheu encerrar o programa
        if (operacao == 0)
        {
            limparEstruturaUsuario();
            finalizar(1, 1);
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
            //Se o usuário digitar algo diferente de 's' o sistema não confirma a exclusão
            if (getchar() != 's')
            {
                setbuf(stdin, NULL); //Limpa valor do getchar()
                getchar();           //Captura o ENTER que ficou, após o getchar() anterior
                //Quando o servidor entrar na opção '2', vai aguardar confirmação para excluir o cadastro
                enviarDadosServidor("cd", "0");
                puts("\n# OPERAÇÃO CANCELADA\n");
                break;
            }
            setbuf(stdin, NULL);            //Limpa valor do getchar()
            getchar();                      //Captura o ENTER que ficou, após o getchar() anterior
            enviarDadosServidor("cd", "1"); //Confirma que o cadastro deve ser excluído
            //Se o primeiro caractere da mensagem recebida do servidor for '>', ele conseguiu excluir o cadastro
            if (receberDadosServidor("dl", 1)[0] == '>')
            {
                limparEstruturaUsuario(); //Limpa a estrutura do usuário que foi excluído e já não está mais logado
                return;                   //Sai da função
            }                             //Senão, o servidor não conseguiu deletar o cadastro, então o usuário permanece logado
            break;
        case 3: //Ver dados do usuário
            imprimirDecoracao();
            printf("\n\t\t>> MEUS DADOS <<\n");
            imprimirDecoracao();
            receberDadosServidor("du", 1); //Recebe e imprime os dados do usuário, pois a flag imprimirMsg está ativa
            break;
        case 4: //Recebe a política de senhas do servidor
            imprimirDecoracao();
            printf("\n\t>> VER POLÍTICA DE IDENTIFICADORES E SENHAS <<\n");
            imprimirDecoracao();
            receberDadosServidor("pt", 1); //Recebe e imprime a política, pois a flag imprimirMsg está ativa
            break;
        default: //Opção inválida
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
    //Limpa o ENTER pressionado no getchar()
    setbuf(stdin, NULL);
}

/**
 * Limpa os dados da estrutura do usuário
 */
void limparEstruturaUsuario()
{
    memset(&u.nome[0], 0, sizeof(u.nome));
    memset(&u.sobrenome[0], 0, sizeof(u.sobrenome));
    memset(&u.email[0], 0, sizeof(u.email));
    memset(&u.identificador[0], 0, sizeof(u.identificador));
    memset(&u.salt[0], 0, sizeof(u.salt));
    memset(&u.senha[0], 0, sizeof(u.senha));
    memset(&u.senhaCriptografada[0], 0, sizeof(u.senhaCriptografada));
    u.codigo = '0';
}

/**
 * Encerra o programa e/ou a comunicação com o servidor conectado, dependendo da flag ativada
 * @param conexao defina 1 se a conexão com o servidor conectado deve ser encerrada
 * @param programa defina 1 se o programa deve ser finalizado
 */
void finalizar(short int comunicacao, short int programa)
{
    if (comunicacao)
    {
        imprimirDecoracao();

        //Fecha o socket
        if (close(socketDescritor))
        {
            perror("# ERRO - Não foi possível fechar a comunicação");
            return;
        }
        puts("> INFO - Você foi desconectado");
    }

    if (programa)
    {
        imprimirDecoracao();
        setbuf(stdin, NULL);  //Limpa buffer de entrada
        setbuf(stdout, NULL); //Limpa buffer de saída
        printf("# SISTEMA FINALIZADO.\n");
        exit(0); //Finaliza aplicação
    }
}
