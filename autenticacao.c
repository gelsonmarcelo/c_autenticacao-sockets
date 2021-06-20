#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <sys/random.h>
#include <errno.h>

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
char arquivoNotas[] = "notas.txt";
char arquivoDisciplina[] = "disciplina.txt";
char arquivoCurso[] = "curso.txt";

struct Usuario u;    //Declaração da estrutura do usuário
struct Disciplina d; //Declaração da estrutura da disciplina
struct Nota n;       //Declaração da estrutura das notas

/*Declaração das funções*/

int pegarProximoId(char *arquivo);
int selecionarUsuario(short int idPapelProcurado);
int selecionarDisciplina(short int idCurso);
short int autenticar(short int ehLogin);
short int validarStringPadrao(char *string);
short int validarStringEmail(char *string);
short int validarIdentificador(char *identificador);
short int validarSenha(char *senha);
short int excluirDados();
short int inserirDadosPadrao(char *arquivo);
short int testarArquivo(char *nomeArquivo);
short int validarNota(char *nota);
char *alternarCapitalLetras(char *string, int flag);
char *descreverNomePapel(short int idPapel);
void cadastrarUsuario();
void mostrarPoliticaSenhas();
void mostrarPoliticaAcesso();
void gerarSalt();
void criptografarSenha();
void limparEstruturaUsuario();
void areaLogada();
void verDadosUsuario(int idUsuario);
void imprimirDecoracao();
void editarDadosUsuario();
void coletarDados(short int nome, short int sobrenome, short int email, short int identificador, short int senha, short int papel);
void atualizarLinhaArquivo(char *arquivo, char *linhaObsoleta, char *linhaAtualizada);
void pausarPrograma();
void operarDisciplina(int idDisciplina, short int verDescricao, short int alterarDescricao, short int alterarProfessor);
void matricularEstudanteDisciplina(int idEstudante, int idDisciplina);
void operarNotas(int idEstudante, int idDisciplina, short int verNotas, short int alterarNotas);
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

/**
 * Estrutura para organização dos dados da disciplina
 */
struct Disciplina
{
    int codigo;              //Mantém o código (ID do arquivo de dados) da disciplina
    char nome[MAX_DADOS];    //Guarda o nome da disciplina
    int idCurso;             //Armazena o ID do curso ao qual a disciplina pertence
    int idProfessor;         //Armazena o ID do professor atribuído à disciplina
    char descricao[MAX * 4]; //Guarda a descrição da disciplina
};

/**
 * Estrutura para organização dos dados da nota
 */
struct Nota
{
    int codigo;       //Mantém o código (ID do arquivo de dados) das notas
    float nota1;      //Guarda o valor da nota 1
    float nota2;      //Guarda o valor da nota 2
    int idEstudante;  //Salva o ID do estudante dono das notas
    int idDisciplina; //Salva o ID da disciplina que as notas pertencem
};

/**
 * Função principal
 */
int main()
{
    setlocale(LC_ALL, "Portuguese"); //Permite a utilização de acentos e caracteres do português nas impressões

    //Verifica arquivos necessários para o programa iniciar
    if (testarArquivo(arquivoUsuarios) || testarArquivo(arquivoNotas) || testarArquivo(arquivoDisciplina) || testarArquivo(arquivoCurso))
    {
        printf("\n# ERRO FATAL - um arquivo de dados essencial não pode ser aberto, o programa não pode ser iniciado.\n");
        finalizarPrograma();
    }

    //Se o arquivo do Curso ou da Disciplina estiver vazio vai inserir os dados default para trabalhar
    if (pegarProximoId(arquivoCurso) == 1 || pegarProximoId(arquivoDisciplina) == 1)
    {
        if (inserirDadosPadrao(arquivoCurso) || inserirDadosPadrao(arquivoDisciplina))
        {
            printf("\n# ERRO FATAL - Não foi possível inserir os dados padrão no arquivo do curso/disciplina, sem eles não é possível iniciar o programa.");
            finalizarPrograma();
        }
    }

    imprimirDecoracao();
    printf("\n\t\t>> PROGRAMA INICIADO COM SUCESSO <<\n");
    int operacao = 0; //Recebe um número que o usuário digitar para escolher a opção do menu

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
        printf("\n[3] Ver a política de criação de identificadores e senhas");
        printf("\n[4] Ver a política de acesso");
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

        system("cls || clear");

        //Escolhe a operação conforme o valor que o usuário digitou
        switch (operacao)
        {
        case 0: //Encerrar programa
            limparEstruturaUsuario();
            printf("\n# SISTEMA FINALIZADO.\n");
            return 0;
        case 1: //Login
            imprimirDecoracao();
            printf("\n\t\t\t>> AUTENTICAÇÃO <<\n\n");
            imprimirDecoracao();
            if (autenticar(1))
            {
                areaLogada();
                limparEstruturaUsuario();
            }
            break;
        case 2: //Cadastro
            imprimirDecoracao();
            printf("\n\t\t\t>> CADASTRO <<\n\n");
            imprimirDecoracao();
            cadastrarUsuario();
            break;
        case 3: //Ver política de identificadores e senhas
            imprimirDecoracao();
            printf("\n\t\t>> POLÍTICA DE IDENTIFICADORES E SENHAS <<\n\n");
            imprimirDecoracao();
            mostrarPoliticaSenhas();
            break;
        case 4: //Ver política de acesso
            imprimirDecoracao();
            printf("\n\t\t\t>> POLÍTICA DE ACESSO <<\n\n");
            imprimirDecoracao();
            mostrarPoliticaAcesso();
            break;
        default:
            printf("\n# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida (%d), tente novamente!\n", operacao);
            break;
        }
    } while (1);
    return 0;
}

/**
 * Insere os dados padrão no arquivo de curso ou disciplina, o que for passado como parâmetro
 * @return 1 em caso de falha; 0 em caso de sucesso
 */
short int inserirDadosPadrao(char *arquivo)
{
    char StringDados[110]; //Tamanho máximo dos dados padrão que serão salvos na variável

    //Teste do arquivo escolhido
    if (testarArquivo(arquivo))
        return 1;

    ponteiroArquivos = fopen(arquivo, "w");

    if (!strcmp(arquivo, arquivoCurso))
    {
        //id | Nome
        strcpy(StringDados, "1 | Análise e Desenvolvimento de Sistemas\n");
    }
    else if (!strcmp(arquivo, arquivoDisciplina))
    {
        //id                   | Nome              | idCurso | idProfessor | Descrição
        strcpy(StringDados, "1 | Segurança da Informação | 1 | 0 | Descrição padrão da disciplina de Segurança da Informação\n");
    }
    else
    {
        printf("\n# ERRO - O arquivo passado como parâmetro não pode ser utilizado para essa função: %s\n", arquivo);
        printf("\n# Podem ser utilizados somente os arquivos %s e %s\n", arquivoCurso, arquivoDisciplina);
        fecharArquivo(ponteiroArquivos);
        return 1;
    }
    //Insere os dados padrão no arquivo
    if (fputs(StringDados, ponteiroArquivos) == EOF)
    {
        printf("\n# ERRO - Problema para inserir os dados no arquivo %s\n", arquivo);
        perror("# - ");
        fecharArquivo(ponteiroArquivos);
        return 1;
    }
    else
    {
        printf("\n# SUCESSO - Dados padrão inseridos no arquivo %s\n", arquivo);
    }

    fecharArquivo(ponteiroArquivos);
    return 0;
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
        fscanf(ponteiroArquivos, "%d | %[^\n]", &id, temp);
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

    printf("\n> Forneça as informações necessárias para efetuar o cadastro:\n");
    coletarDados(1, 1, 1, 1, 1, 1);

    u.codigo = pegarProximoId(arquivoUsuarios); //Define o ID do usuário que está se cadastrando

    //Validação novamente para caso o arquivo não possa ser aberto, antes de gravar os dados recebidos do usuário
    if (testarArquivo(arquivoUsuarios))
        return;

    //Abrir o arquivo com parâmetro "a" de append, não sobrescreve as informações, apenas adiciona
    ponteiroArquivos = fopen(arquivoUsuarios, "a");

    //Passar dados cadastrados para a linha do usuário no formato que irão para o arquivo
    sprintf(u.linhaUsuario, "%d | %s | %s | %s | %s | %s | %s | %d\n", u.codigo, u.identificador, u.salt, u.senhaCriptografada, u.nome, u.sobrenome, u.email, u.papel);

    //Insere a string com todos os dados no arquivo e valida se não ocorreram erros
    if (fputs(u.linhaUsuario, ponteiroArquivos) == EOF)
    {
        perror("\n# ERRO - Houve algum problema para inserir os dados no arquivo.\n");
    }
    else
    {
        printf("\n# SUCESSO - Cadastro realizado: realize login para acessar o sistema.\n");
    }

    fecharArquivo(ponteiroArquivos); //Fecha o arquivo
    limparEstruturaUsuario();
}

/**
 * Coletar dados do usuário, chame a função definindo o parâmetro no valor 1 no respectivo dado que deseja solicitar ao usuário
 */
void coletarDados(short int nome, short int sobrenome, short int email, short int identificador, short int senha, short int papel)
{
    char entradaTemp[MAX];                           //Variável para fazer a entrada de valores digitados e realizar a validação antes de atribuir à variável correta
    memset(&entradaTemp[0], 0, sizeof(entradaTemp)); //Limpando a variável para garantir que não entre lixo de memória

    //Solicita o nome do usuário
    if (nome)
    {
        //Loop para validação do nome, enquanto a função que valida a string retornar 0 (falso) o loop vai continuar (há uma negação na frente do retorno da função)
        do
        {
            printf("\n> Informe seu primeiro nome: ");
            setbuf(stdin, NULL);          //Limpa a entrada par evitar lixo
            scanf("%[^\n]", entradaTemp); //Leitura do teclado, o %[^\n] vai fazer com que o programa leia tudo que o usuário digitar exceto ENTER (\n), por padrão pararia de ler no caractere espaço
        } while (!validarStringPadrao(entradaTemp));
        //Se sair do loop é porque a string é válida e pode ser copiada para a variável correta que irá guardar
        strcpy(u.nome, entradaTemp);
        //Transformar o nome em maiúsculo para padronização do arquivo
        alternarCapitalLetras(u.nome, 1);
        //Limpar a variável temporária para receber a próxima entrada
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    }

    //Solicita o sobrenome do usuário
    if (sobrenome)
    {
        do
        {
            printf("\n> Informe seu último sobrenome: ");
            setbuf(stdin, NULL);
            scanf("%[^\n]", entradaTemp);
        } while (!validarStringPadrao(entradaTemp));
        strcpy(u.sobrenome, entradaTemp);
        alternarCapitalLetras(u.sobrenome, 1);
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    }

    //Solicita o e-mail do usuário
    if (email)
    {
        do
        {
            printf("\n> Informe seu e-mail: ");
            setbuf(stdin, NULL);
            scanf("%[^\n]", entradaTemp);
        } while (!validarStringEmail(entradaTemp));
        strcpy(u.email, entradaTemp);
        alternarCapitalLetras(u.email, 1);
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    }

    //Solicita qual o papel do usuário no sistema
    if (papel)
    {
        do
        {
            printf("\n> Informe o número referente ao papel que lhe descreve: ");
            printf("\n[1] Coordenador");
            printf("\n[2] Professor");
            printf("\n[3] Estudante\n> ");
            u.papel = 0;
            setbuf(stdin, NULL);
            //Valida a entrada que o usuário digitou
            while (scanf("%d", &u.papel) != 1)
            {
                printf("\n# FALHA - Ocorreu um erro na leitura do valor informado.\n> Tente novamente: ");
                setbuf(stdin, NULL);
            };

            if (u.papel > 0 && u.papel < 4)
            {
                break;
            }
            else
            {
                printf("\n# FALHA - Escolha um número dentre as opções!\n");
            }
        } while (1);
    }

    //Se o identificador/senha será coletado, mostrar a política para que o usuário saiba o que será aceito antes de digitar
    if (identificador || senha)
    {
        imprimirDecoracao();
        mostrarPoliticaSenhas();
    }

    //Solicita que o usuário crie um identificador
    if (identificador)
    {
        do
        {
            printf("\n> Crie seu login: ");
            setbuf(stdin, NULL);
            scanf("%[^\n]", entradaTemp);
        } while (!validarIdentificador(entradaTemp));
        strcpy(u.identificador, entradaTemp);
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));
    }

    //Solicita o usuário crie uma senha
    if (senha)
    {
        do
        {
            setbuf(stdin, NULL);
            //getpass() é usado para desativar o ECHO do console e não exibir a senha sendo digitada pelo usuário, retorna um ponteiro apontando para um buffer contendo a senha, terminada em '\0' (NULL)
            strcpy(entradaTemp, getpass("\n> Crie uma senha: "));
        } while (!validarSenha(entradaTemp));
        strcpy(u.senha, entradaTemp);
        memset(&entradaTemp[0], 0, sizeof(entradaTemp));

        //Gerar o valor pseudoaleatório do salt desse usuário e jogar na variável salt dele
        gerarSalt();
        //Realiza a criptografia da senha com a função 'crypt' e define o valor na variável senhaCriptografada do usuário
        criptografarSenha();
    }
}

/**
 * Realizar a autenticação do usuário
 * @param ehLogin deve ser passado como 1 caso a chamada da função está sendo realizada para login e não somente autenticação, 
 * quando for login a função vai definir os dados do usuário, trazidos do arquivo, na estrutura
 * @return 1 em caso de sucesso e; 0 em outros casos
 */
short int autenticar(short int ehLogin)
{
    //Variáveis que guardam os dados lidos nas linhas do arquivo
    char identificadorArquivo[MAX_IDENTIFICADOR], saltArquivo[SALT_SIZE + 1], criptografiaArquivo[120], usuarioArquivo[MAX_DADOS], sobrenomeArquivo[MAX_DADOS], emailArquivo[MAX_DADOS];
    int codigoArquivo = 0, papelArquivo = 0;

    /*Coleta do login e senha para autenticação*/
    printf("\n> Informe suas credenciais:\n# LOGIN: ");
    setbuf(stdin, NULL);                   //Limpa o buffer de entrada par evitar lixo
    scanf("%[^\n]", &u.identificador);     //Realiza a leitura até o usuário pressionar ENTER
    strcpy(u.senha, getpass("# SENHA: ")); //Lê a senha com ECHO do console desativado e copia o valor lido para a variável u.senha, do usuário

    //Validação para caso o arquivo não possa ser aberto
    if (testarArquivo(arquivoUsuarios))
        return 0;

    //Abrir o arquivo com parâmetro "r" de read, apenas lê
    ponteiroArquivos = fopen(arquivoUsuarios, "r");

    //Loop que passa por todas as linhas do arquivo até o final
    while (!feof(ponteiroArquivos))
    {
        //Define os dados da linha lida nas variáveis na respectiva ordem do padrão lido
        fscanf(ponteiroArquivos, "%d | %s | %s | %s | %s | %s | %s | %d", &codigoArquivo, identificadorArquivo, saltArquivo, criptografiaArquivo, usuarioArquivo, sobrenomeArquivo, emailArquivo, &papelArquivo);
        //Copia o salt lido do arquivo nessa linha para a variável u.salt, onde o criptografarSenha() irá usar
        strcpy(u.salt, saltArquivo);
        //Criptografa a senha que o usuário digitou com o salt que foi lido na linha
        criptografarSenha();
        //Se o identificador lido no arquivo e a senha digitada, criptografada com o salt da linha do arquivo forem iguais, autentica o usuário
        if (!strcmp(identificadorArquivo, u.identificador) && !strcmp(criptografiaArquivo, u.senhaCriptografada))
        {
            //Quando for login basta acertar um identificador e senha qualquer salvo no arquivo para realizar
            if (ehLogin)
            {
                //Copiando dados para a estrutura
                u.codigo = codigoArquivo;
                strcpy(u.nome, usuarioArquivo);
                strcpy(u.sobrenome, sobrenomeArquivo);
                strcpy(u.email, emailArquivo);
                strcpy(u.identificador, identificadorArquivo);
                strcpy(u.salt, saltArquivo);
                strcpy(u.senhaCriptografada, criptografiaArquivo);
                u.papel = papelArquivo;
                //Salvar o formato da linha do usuário autenticado
                sprintf(u.linhaUsuario, "%d | %s | %s | %s | %s | %s | %s | %d\n", u.codigo, u.identificador, u.salt, u.senhaCriptografada, u.nome, u.sobrenome, u.email, u.papel);
                printf("\n\n# SUCESSO - Login realizado.\n\n");
                fecharArquivo(ponteiroArquivos); //Fecha o arquivo
                return 1;
            }
            //Se não for login, a autenticação só poderá liberar o acesso caso o usuário autenticando seja o usuário que está logado
            else if (u.codigo == codigoArquivo)
            {
                printf("\n\n# SUCESSO - Acesso autorizado.\n\n");
                fecharArquivo(ponteiroArquivos); //Fecha o arquivo
                return 1;
            }
            //Caso o usuário que tentou autenticar não seja o que está logado
            else
            {
                printf("\n\n# FALHA [ACESSO NÃO AUTORIZADO] - Esse usuário não pode realizar essa operação!\n\n");
                return 0;
            }
        }
    }
    //Se sair do loop é porque não autenticou por erro de login/senha
    fecharArquivo(ponteiroArquivos);
    /*Apenas se for autenticação para login, deve limpar os dados caso falhe a autenticação, 
    se for autenticação quando o usuário já está logado, não pode limpar os dados caso erre a senha.*/
    if (ehLogin)
        limparEstruturaUsuario();

    printf("\n# FALHA - Usuário e/ou senha incorretos!\n");
    return 0;
}

/**
 * Imprime a política com as regras para criação de identificadores e senhas
 */
void mostrarPoliticaSenhas()
{
    printf("\n\t\tIDENTIFICADOR/LOGIN\n");
    printf("\n-Não pode ser utilizado nome, sobrenome ou email;");
    printf("\n-Pode conter somente caracteres alfanuméricos e ponto final;");
    printf("\n-Deve ter no mínimo 5 caracteres e no máximo 15.");
    printf("\n\n\t\t\tSENHA\n");
    printf("\n-Deve conter no mínimo 8 caracteres e no máximo 30;");
    printf("\n-Deve conter, no mínimo, 2 caracteres especiais;");
    printf("\n-Deve conter números e letras;");
    printf("\n-Deve conter pelo menos uma letra maiúscula e uma minúscula;");
    printf("\n-Não pode conter mais de 2 números ordenados em sequência (Ex.: 123, 987);");
    printf("\n-Não pode conter mais de 2 números repetidos em sequência (Ex.: 555, 999);");
    printf("\n-Não pode conter caracteres que não sejam alfanuméricos (números e letras), especiais ou espaço.\n");
}

/**
 * Imprime a política com as regras para acessos por papel
 */
void mostrarPoliticaAcesso()
{
    printf("\n\t\tCOORDENADOR\n");
    printf("\n-Dados dos Estudantes: ver dados;");
    printf("\n-Disciplina: ver descrição da disciplina, alterar descrição da disciplina, matricular estudante;");
    printf("\n-Notas: ver notas, alterar notas.");
    printf("\n\n\t\tPROFESSOR\n");
    printf("\n-Dados dos Estudantes: ver dados;");
    printf("\n-Disciplina: ver descrição da disciplina, alterar descrição da disciplina;");
    printf("\n-Notas: ver notas, alterar notas.");
    printf("\n\n\t\tESTUDANTE\n");
    printf("\n-Dados dos Estudantes: ver dados*, alterar dados*;");
    printf("\n-Disciplina: ver descrição da disciplina, matricular estudante*;");
    printf("\n-Notas: ver notas*.");
    printf("\n*Somente próprias informações\n");
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
        printf("\n# FALHA [QUANTIDADE DE CARACTERES]: esse dado deve conter no mínimo 2 letras e no máximo 50, você inseriu %d letras!\n", strlen(string));
        return 0;
    }

    //Loop para passar pelos caracteres da string verificando se são alfabéticos
    for (int i = 0; i < strlen(string); i++)
    {
        //Usando a função isalpha da biblioteca ctype.h, é possível verificar se o caractere é alfabético
        if (!isalpha(string[i]))
        {
            printf("\n# FALHA [CARACTERE INVÁLIDO]: insira somente caracteres alfabéticos nesse campo, sem espaços ou pontuações.\n# '%c' não é alfabético!\n", string[i]);
            return 0;
        }
    }
    //Se chegou aqui é válido
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
        printf("\n# FALHA [QUANTIDADE DE CARACTERES]: verifique o dado digitado, deve conter entre 6 e 50 caracteres, você inseriu %d caracteres!\n", strlen(string));
        return 0;
    }

    // sscanf lê a entrada a partir da string no primeiro parâmetro, atribuindo para as variáveis. Retorna o número de campos convertidos e atribuídos com êxito.
    if (sscanf(string, "%[^@ \t\n]@%[^. \t\n].%3[^ \t\n]", usuario, site, dominio) != 3)
    {
        printf("\n# FALHA [E-MAIL INVÁLIDO]: verifique o e-mail digitado.\n");
        return 0;
    }
    //Se chegou aqui é porque a string é válida
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
        printf("\n# FALHA [IDENTIFICADOR INVÁLIDO] - Não contém tamanho permitido (mínimo 5 e máximo 15)\n");
        return 0;
    }

    //Loop para passar pelos caracteres do identificador e verificar se contém caracteres válidos
    for (int i = 0; i < strlen(identificador); i++)
    {
        if (!isalnum(identificador[i]) && identificador[i] != '.')
        {
            printf("\n# FALHA [IDENTIFICADOR INVÁLIDO] - Contém caracteres não permitidos\n# O identificador pode conter somente caracteres alfanuméricos e ponto final.\n");
            return 0;
        }
    }

    strcpy(identificadorMaiusculo, identificador);    //Copiando o identificador para transformar em maiúsculo
    alternarCapitalLetras(identificadorMaiusculo, 1); //Tornando maiúsculo
    //Se o identificador for igual ao nome ou sobrenome, é inválido.
    if (!strcmp(identificadorMaiusculo, u.nome) || !strcmp(identificadorMaiusculo, u.sobrenome))
    {
        printf("\n# FALHA [IDENTIFICADOR INVÁLIDO] - Identificador não pode ser seu nome ou sobrenome!\n");
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
        fscanf(ponteiroArquivos, "%s | %s | %[^\n]", temp, identificadorArquivo, temp);
        alternarCapitalLetras(identificadorArquivo, 1); //Transformar o identificador recebido do arquivo em maiúsculo também para comparar com o que está tentando cadastrar
        //Realizando a comparação dos identificadores
        if (!strcmp(identificadorArquivo, identificadorMaiusculo))
        {
            //Se entrar aqui o identificador já foi utilizado
            printf("\n# FALHA [IDENTIFICADOR INVÁLIDO] - Já está sendo utilizado!\n");
            fecharArquivo(ponteiroArquivos);
            return 0;
        }
    }
    fecharArquivo(ponteiroArquivos); //Fecha o arquivo
    //Se chegou até aqui, passou pelas validações, retorna 1 - true
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
        printf("\n# FALHA [SENHA INVÁLIDA] - Não contém tamanho permitido (mínimo 8 e máximo 30)\n");
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
                printf("\n# FALHA [SENHA INVÁLIDA] - | %c | faz parte de números ordenados em sequência\n", senha[i]);
                return 0;
            }

            //Verifica se a senha contém +2 números repetidos em sequência
            if (senha[i] == senha[i + 1] && senha[i] == senha[i + 2])
            {
                printf("\n# FALHA [SENHA INVÁLIDA] - | %c | faz parte de números repetidos em sequência\n", senha[i]);
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
            printf("\n\n# FALHA [SENHA INVÁLIDA] - Sua senha contém caracteres que nao são nem alfanuméricos nem especiais ou espaço.");
            printf("\n# Verifique a digitação e tente novamente.\n# Caracteres permitidos:");
            printf("\n#\tEspeciais: ! \" # $ %% & ' ( ) * + , - . / : ; < = > ? @ [ \\ ] ^ _ ` { | } ~ [ESPAÇO]");
            printf("\n#\tNuméricos: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9");
            printf("\n#\tAlfabéticos: a b c d e f g h i j k l m n o p q r s t u v w x y z \n#\t\t\tA B C D E F G H I J K L M N O P Q R S T U V W X Y Z\n");
            return 0;
        }
    } //Fim do loop que passa pelos caracteres da senha

    //Valida a quantidade de caracteres especiais
    if (contEspeciais < 2)
    {
        printf("\n# FALHA [SENHA INVÁLIDA] - Não contém caracteres especiais suficientes\n");
        return 0;
    }
    //Verifica se contém números e letras
    if ((contminusculas + contmaiusculas) == 0 || contNumeros == 0)
    {
        printf("\n# FALHA [SENHA INVÁLIDA] - Não contém letras e números\n");
        return 0;
    }
    //Verifica se contém minúsculas
    if (contminusculas == 0)
    {
        printf("\n# FALHA [SENHA INVÁLIDA] - Não contém qualquer letra minúscula\n");
        return 0;
    }
    //Verifica se contém maiúsculas
    if (contmaiusculas == 0)
    {
        printf("\n# FALHA [SENHA INVÁLIDA] - Não contém qualquer letra maiúscula\n");
        return 0;
    }
    //Solicita a confirmação da senha
    strcpy(confirmacaoSenha, getpass("\n> Confirme sua senha: "));
    //Compara as 2 senhas informadas, se forem diferentes vai retornar != 0, entrando na condição
    if (strcmp(confirmacaoSenha, senha))
    {
        printf("\n# FALHA [SENHAS INCOMPATÍVEIS] - As senhas não coincidem\n");
        return 0;
    }

    printf("\n# SUCESSO - Sua senha está de acordo com a política e foi aprovada!\n");
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
        perror("\n# ERRO - A geração de caracteres para criação do salt não foi bem sucedida\n");
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
 * Zera os dados da estrutura do usuário para reutilização
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
    memset(&u.linhaUsuario[0], 0, sizeof(u.linhaUsuario));
    u.papel = '0';
    u.codigo = '0';
}

/**
 * Opções para o usuário autenticado
 */
void areaLogada()
{
    int operador = 0; //Recebe o valor da entrada do usuário para escolher a operação
    imprimirDecoracao();
    printf("\n\t\tBEM-VINDO %s %s!\n", descreverNomePapel(u.papel), u.nome);

    //Menu de opções
    do
    {
        pausarPrograma();
        //Limpar a variável para evitar lixo de memória nas repetições
        operador = '\0';

        system("cls || clear");
        imprimirDecoracao();
        printf("\n\t\t\tLOGADO COMO %s.\n", u.nome);
        //Qualquer papel tem acesso
        printf("\n> Informe um número para escolher uma opção e pressione ENTER:");
        printf("\n___________________________________");
        printf("\n[0] ENCERRAR PROGRAMA");
        printf("\n[1] LOGOUT");
        printf("\n[2] EXCLUIR MEU CADASTRO");
        printf("\n___________________________________");
        printf("\n[3] Ver meus dados");
        printf("\n[4] Editar meus dados");
        printf("\n[5] Ver descrição da disciplina");
        printf("\n[6] Ver notas");
        //Coordenador e Professor tem acesso
        if (u.papel == 1 || u.papel == 2)
        {
            printf("\n[7] Ver dados estudantes");
            printf("\n[8] Alterar descrição de disciplina");
            printf("\n[9] Alterar notas");
        }
        //Coordenador tem acesso
        if (u.papel == 1)
        {
            printf("\n[10] Matricular estudante");
            printf("\n[11] Definir professor em disciplina");
        }
        //Somente se for estudante mostra essa opção como 7
        if (u.papel == 3)
        {
            printf("\n[7] Matricular-se em disciplina");
        }

        imprimirDecoracao();
        printf("\n> Informe o número: ");
        setbuf(stdin, NULL);
        //Valida a entrada que o usuário digitou
        while (scanf("%d", &operador) != 1)
        {
            printf("\n# FALHA - Ocorreu um erro na leitura do valor informado.\n> Tente novamente: ");
            setbuf(stdin, NULL);
        };

        //Escolha da operação a ser realizada
        switch (operador)
        {
        case 0: //Encerrar programa
            system("cls || clear");
            finalizarPrograma();
            break;
        case 1: //Logout
            system("clear || cls");
            printf("\n# LOGOUT - Você saiu\n");
            return;
        case 2: //Excluir conta
            system("clear || cls");
            imprimirDecoracao();
            printf("\n\t\t\t>> EXCLUIR CONTA <<\n");
            imprimirDecoracao();
            //Se os dados foram deletados com sucesso
            if (excluirDados())
            {
                setbuf(stdin, NULL);
                getchar();
                return;
            }
            else
            {
                printf("\n# OPERAÇÃO CANCELADA\n");
                setbuf(stdin, NULL);
                getchar();
            }
            break;
        case 3: //Ver dados do próprio usuário
            system("clear || cls");
            imprimirDecoracao();
            printf("\n\t\t\t>> MEUS DADOS <<\n");
            verDadosUsuario(u.codigo);
            imprimirDecoracao();
            break;
        case 4: //Editar dados do usuário
            imprimirDecoracao();
            printf("\n\t\t\t>> EDITAR MEUS DADOS <<\n");
            printf("\n# INFO - Visualize seus dados antes de editar...\n");
            verDadosUsuario(u.codigo);
            imprimirDecoracao();
            editarDadosUsuario(u.codigo);
            break;
        case 5: //Ver descrição da disciplina
            do
            {
                system("clear || cls");
                imprimirDecoracao();
                printf("\n\t\t>> DESCRIÇÃO DA DISCIPLINA <<\n");
                imprimirDecoracao();
                operarDisciplina(selecionarDisciplina(1), 1, 0, 0);
                printf("\n# <?> - Repetir a operação? [s/n]\n>");
                setbuf(stdin, NULL);
                if (getchar() != 's')
                    break; //Usuário deseja voltar ao menu principal, sai do loop
                setbuf(stdin, NULL);
            } while (1);
            break;
        case 6: //Ver notas do estudante
            do
            {
                system("clear || cls");
                imprimirDecoracao();
                printf("\n\t\t\t>> NOTAS <<\n\n");
                imprimirDecoracao();
                //Se o usuário logado for estudante, mostra apenas as notas dele
                if (u.papel == 3)
                {
                    printf("\n# INFO - Selecione a disciplina para ver suas notas...\n");
                    operarNotas(u.codigo, selecionarDisciplina(1), 1, 0);
                }
                else //Se for outro papel, o usuário seleciona de qual estudante quer ver as notas
                {
                    printf("\n# INFO - Selecione a disciplina e o estudante para ver as notas...\n");
                    operarNotas(selecionarUsuario(3), selecionarDisciplina(1), 1, 0);
                }
                printf("\n# <?> - Repetir a operação? [s/n]\n>");
                setbuf(stdin, NULL);
                if (getchar() != 's')
                    break; //Usuário deseja voltar ao menu principal, sai do loop
                setbuf(stdin, NULL);
            } while (1);
            break;
        case 7: //Ver dados dos estudantes se o usuário não for estudante e matricular em disciplina se for estudante
            imprimirDecoracao();
            if (u.papel != 3)
            {
                do
                {
                    system("clear || cls");
                    imprimirDecoracao();
                    printf("\n\t\t>> VER DADOS DE ESTUDANTES <<\n");
                    imprimirDecoracao();
                    printf("\n# INFO - Selecione o estudante para ver os dados...\n");
                    verDadosUsuario(selecionarUsuario(3));
                    printf("\n# <?> - Repetir a operação? [s/n]\n>");
                    setbuf(stdin, NULL);
                    if (getchar() != 's')
                        break; //Usuário deseja voltar ao menu principal, sai do loop
                    setbuf(stdin, NULL);
                } while (1);
            }
            else
            {
                do
                {
                    system("clear || cls");
                    imprimirDecoracao();
                    printf("\n\t\t>> MATRICULAR EM DISCIPLINA <<\n\n");
                    imprimirDecoracao();
                    printf("\n# INFO - Selecione a disciplina para matricular-se...\n");
                    matricularEstudanteDisciplina(u.codigo, selecionarDisciplina(1));
                    printf("\n# <?> - Repetir a operação? [s/n]\n>");
                    setbuf(stdin, NULL);
                    if (getchar() != 's')
                        break; //Usuário deseja voltar ao menu principal, sai do loop
                    setbuf(stdin, NULL);
                } while (1);
            }
            break;
        default: //Se o usuário escolheu outra opção...
            //Se for coordenador ou professor, em qualquer um dos 2 faz essas operações, isso evita que o estudante acesse essas opções mesmo não aparecendo no seu menu
            if (u.papel == 1 || u.papel == 2)
            {
                //Escolhas do coordenador e professor
                switch (operador)
                {
                case 8: //Alterar descrição da disciplina
                    do
                    {
                        system("clear || cls");
                        imprimirDecoracao();
                        printf("\n\t\t>> ALTERAR DESCRIÇÃO DA DISCIPLINA <<\n");
                        imprimirDecoracao();
                        printf("\n# INFO - Selecione a disciplina para alterar a descrição...\n");
                        operarDisciplina(selecionarDisciplina(1), 1, 1, 0);
                        printf("\n# <?> - Repetir a operação? [s/n]\n>");
                        setbuf(stdin, NULL);
                        if (getchar() != 's')
                            break; //Usuário deseja voltar ao menu principal, sai do loop
                        setbuf(stdin, NULL);
                    } while (1);
                    break;
                case 9:
                    do
                    {
                        system("clear || cls");
                        imprimirDecoracao();
                        printf("\n\t\t>> ALTERAR NOTAS DOS ESTUDANTES <<\n");
                        imprimirDecoracao();
                        printf("\n# INFO - Selecione a disciplina e o estudante para alterar as notas...\n");
                        operarNotas(selecionarUsuario(3), selecionarDisciplina(1), 1, 1);
                        printf("\n# <?> - Alterar notas de outros estudantes? [s/n]\n>");
                        setbuf(stdin, NULL);
                        getchar();
                        if (getchar() != 's')
                            break; //Usuário deseja voltar ao menu principal, sai do loop
                        setbuf(stdin, NULL);
                    } while (1);
                    break;
                default:
                    if (u.papel == 1)
                    {
                        //Escolhas exclusivas do coordenador
                        switch (operador)
                        {
                        case 10:
                            do
                            {
                                system("clear || cls");
                                imprimirDecoracao();
                                printf("\n\t\t\t>> MATRICULAR ESTUDANTE <<\n");
                                imprimirDecoracao();
                                printf("\n# INFO - Selecione a disciplina e o estudante para matricular...\n");
                                matricularEstudanteDisciplina(selecionarUsuario(3), selecionarDisciplina(1));
                                printf("\n# <?> - Repetir a operação? [s/n]\n>");
                                setbuf(stdin, NULL);
                                if (getchar() != 's')
                                    break; //Usuário deseja voltar ao menu principal, sai do loop
                                setbuf(stdin, NULL);
                            } while (1);
                            break;
                        case 11:
                            do
                            {
                                system("clear || cls");
                                imprimirDecoracao();
                                printf("\n\t\t>> DEFINIR PROFESSOR PARA DISCIPLINA <<\n");
                                imprimirDecoracao();
                                printf("\n# INFO - Selecione a disciplina e o professor para definir...\n");
                                operarDisciplina(selecionarDisciplina(1), 0, 0, 1);
                                printf("\n# <?> - Repetir a operação? [s/n]\n>");
                                setbuf(stdin, NULL);
                                if (getchar() != 's')
                                    break; //Usuário deseja voltar ao menu principal, sai do loop
                                setbuf(stdin, NULL);
                            } while (1);
                            break;
                        default: //Coordenador digitou uma opção inválida
                            printf("\n# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida, tente novamente!\n");
                            break;
                        }
                    }
                    else //Professor digitou uma opção inválida
                    {
                        printf("\n# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida, tente novamente!\n");
                    }
                    break;
                }
            }
            else //Estudante digitou uma opção inválida
            {
                printf("\n# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida, tente novamente!\n");
            }
            break;
        }
    } while (1);
}

/**
 * Imprime os dados do usuário
 * @param idUsuario ID do usuário que se deseja exibir os dados
 */
void verDadosUsuario(int idUsuario)
{
    //Teste do arquivo
    if (testarArquivo(arquivoUsuarios))
        return;

    //Variáveis para guardar os dados recebidos do arquivo
    char nome[MAX_DADOS], sobrenome[MAX_DADOS], email[MAX_DADOS], identificador[MAX_IDENTIFICADOR], salt[SALT_SIZE + 1], senhaCriptografada[120];
    int idPapel = 0, idLido = 0;

    ponteiroArquivos = fopen(arquivoUsuarios, "r"); //Abre o arquivo

    //Loop que passa pelas linhas do arquivo coletando os dados dos usuários e quando encontra o ID do usuário passado como parâmetro, para de procurar
    while (!feof(ponteiroArquivos))
    {
        //Lê as linhas até o final do arquivo, atribuindo os dados nas variáveis
        fscanf(ponteiroArquivos, "%d | %s | %s | %s | %s | %s | %s | %d\n", &idLido, identificador, salt, senhaCriptografada, nome, sobrenome, email, &idPapel);

        //Se encontrar a linha do ID do usuário procurado, encerra o loop
        if (idLido == idUsuario)
        {
            break;
        }
    }
    fecharArquivo(ponteiroArquivos);

    //Se sair do loop e o idLido por último não for o do usuário que foi passado como parâmetro, significa que não encontrou o usuário buscado
    if (idLido != idUsuario)
    {
        printf("\n# ERRO - Os dados do usuário selecionado não puderam ser localizados");
        return;
    }

    //Se chegar aqui exibe os dados que estão nas variáveis, que são referentes ao usuário procurado
    printf("\n¬ Código: %d", idUsuario);
    printf("\n¬ Nome Completo: %s %s", nome, sobrenome);
    printf("\n¬ E-mail: %s", email);
    printf("\n¬ Login: %s", identificador);
    printf("\n¬ Salt: %s", salt);
    printf("\n¬ Senha criptografada: %s", senhaCriptografada);
    printf("\n¬ Papel: %s\n", descreverNomePapel(idPapel));
}

/**
 * Excluir os dados do usuário do arquivo de dados
 * @return 1 se o cadastro foi deletado; 0 se foi cancelado pelo usuário ou por falha no arquivo
 */
short int excluirDados()
{
    printf("\n# AVISO - Tem certeza que deseja excluir seu cadastro?\n# Essa ação não pode ser desfeita! [s/n]\n>");
    setbuf(stdin, NULL);
    if (getchar() != 's')
    {
        //Usuário não confirmou a operação digitando 's', então encerra a função
        return 0;
    }

    //Validação para caso os arquivos não possa ser acessados
    if (testarArquivo(arquivoUsuarios) || testarArquivo("transferindo.txt"))
        return 0;

    ponteiroArquivos = fopen(arquivoUsuarios, "r"); //Arquivo de entrada
    FILE *saida = fopen("transferindo.txt", "w");   //Arquivo de saída
    char texto[MAX];                                //Uma string grande para armazenar as linhas lidas

    //Loop pelas linhas do arquivo para salvar as linhas que não são do usuário em questão e não salvar a linha dele no novo arquivo gerado
    while (fgets(texto, MAX, ponteiroArquivos) != NULL)
    {
        //Se a linha sendo lida no arquivo for diferente da linha do usuário atual, ela será copiada para o arquivo de saída
        if (strcmp(u.linhaUsuario, texto))
        {
            fputs(texto, saida);
        }
    }
    //Fecha os arquivos
    fecharArquivo(ponteiroArquivos);
    fecharArquivo(saida);

    //Deleta o arquivo com os dados antigos
    if (remove(arquivoUsuarios))
    {
        system("cls || clear");
        printf("\n# ERRO - Ocorreu um erro ao deletar um arquivo necessário, o programa foi abortado.\n");
        finalizarPrograma();
    }
    //Renomeia o arquivo onde foram passadas as linhas que não seriam excluidas para o nome do arquivo de dados de entrada
    if (rename("transferindo.txt", arquivoUsuarios))
    {
        system("cls || clear");
        printf("\n# ERRO - Ocorreu um erro ao renomear um arquivo necessário, o programa foi abortado.\n");
        finalizarPrograma();
    }
    limparEstruturaUsuario();
    printf("\n# SUCESSO - Seu cadastro foi deletado.\n");
    return 1;
}

/**
 * Apenas imprime as linhas de separação
 */
void imprimirDecoracao()
{
    printf("\n_________________________________________________________________________________");
    printf("\n*********************************************************************************\n");
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
        printf("\n# ERRO - O arquivo '%s' não pode ser acessado, verifique.\n", nomeArquivo);
        return 1;
    }
    fecharArquivo(arquivo);
    return 0;
}

/**
 * Disponibiliza um menu para o usuário escolher que dados do cadastro ele quer editar, e salva no arquivo de dados
 */
void editarDadosUsuario()
{
    //Validação para caso os arquivos não possa ser acessados
    if (testarArquivo(arquivoUsuarios))
        return;

    char linhaAtualizadaUsuario[MAX]; //Essa string serve para guardar os dados atualizados do usuário, e posteriormente ser passada na função
    int operacao = 0;                 //Recebe a entrada que o usuário digitar
    short int atualizar = 0;          //Flag que será usada para dizer se o programa pode ou não atualizar os dados ao sair do switch

    //Menu de opções
    do
    {
        pausarPrograma();
        operacao = '\0'; //Limpar a variável para evitar lixo de memória nas repetições
        atualizar = 1;   //Se não houver autenticação sempre permite atualização
        system("cls || clear");

        imprimirDecoracao();
        printf("\n\t\t\tEDITAR MEUS DADOS\n");
        printf("\n> Informe um número para escolher a informação que deseja editar e pressione ENTER:");
        printf("\n[1] Nome");
        printf("\n[2] Sobrenome");
        printf("\n[3] E-mail");
        printf("\n[4] Identificador");
        printf("\n[5] Senha");
        printf("\n[6] Papel");
        printf("\n_______________________________");
        printf("\n[7] SAIR");
        printf("\n[0] ENCERRAR PROGRAMA");
        imprimirDecoracao();
        printf("\n> Informe o número: ");

        setbuf(stdin, NULL);
        //Valida a entrada que o usuário digitou
        while (scanf("%d", &operacao) != 1)
        {
            printf("\n# FALHA - Ocorreu um erro na leitura do valor informado.\n> Tente novamente: ");
            setbuf(stdin, NULL);
        };

        //Escolhe a operação que o usuário digitou
        switch (operacao)
        {
        case 0: //Encerra o programa
            system("cls || clear");
            finalizarPrograma();
        case 1: //Coleta o nome
            coletarDados(1, 0, 0, 0, 0, 0);
            break;
        case 2: //Coleta o sobrenome
            coletarDados(0, 1, 0, 0, 0, 0);
            break;
        case 3: //Coleta o e-mail
            coletarDados(0, 0, 1, 0, 0, 0);
            break;
        case 4: //Solicita autenticação e coleta o identificador
            if (autenticar(0))
            {
                coletarDados(0, 0, 0, 1, 0, 0);
                break;
            }
            atualizar = 0;
            break;
        case 5: //Solicita autenticação e coleta a nova senha
            if (autenticar(0))
            {
                coletarDados(0, 0, 0, 0, 1, 0);
                break;
            }
            atualizar = 0;
            break;
        case 6: //Coleta o papel
            coletarDados(0, 0, 0, 0, 0, 1);
            break;
        case 7: //Sair da função
            return;
        default:
            printf("\n# FALHA [OPÇÃO INVÁLIDA] - Você digitou uma opção inválida, tente novamente!\n");
            break;
        }

        //Verifica se pode atualizar os dados do usuário com os dados coletados
        if (atualizar)
        {
            //Preparar linha com dados do usuário para inserir no arquivo
            sprintf(linhaAtualizadaUsuario, "%d | %s | %s | %s | %s | %s | %s | %d\n", u.codigo, u.identificador, u.salt, u.senhaCriptografada, u.nome, u.sobrenome, u.email, u.papel);
            atualizarLinhaArquivo(arquivoUsuarios, u.linhaUsuario, linhaAtualizadaUsuario);
            //Atualizar a string da linha que está no arquivo agora
            strcpy(u.linhaUsuario, linhaAtualizadaUsuario);
        }
    } while (1);
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
        printf("\n# ERRO - Ocorreu um erro ao deletar um arquivo necessário, o programa foi abortado.\n");
        finalizarPrograma();
    }
    //Renomeia o arquivo onde foram passadas as linhas que não seriam excluidas para o nome do arquivo de dados de entrada
    if (rename("transferindo.txt", arquivo))
    {
        system("cls || clear");
        printf("\n# ERRO - Ocorreu um erro ao renomear um arquivo necessário, o programa foi abortado.\n");
        finalizarPrograma();
    }
    printf("\n# SUCESSO - Os dados foram atualizados!\n");
}

/**
 * Lista todos os usuários de um grupo específico passado no parâmetro inteiro e dá opção de escolha ao usuário
 * @param idPapelProcurado 1-Coordenadores, 2-Professores ou 3-Estudantes
 * @return ID do usuário escolhido dentre os listados; 0 em caso de falha
 */
int selecionarUsuario(short int idPapelProcurado)
{
    //Teste do arquivo
    if (testarArquivo(arquivoUsuarios))
        return 0;

    int listaIds[MAX];                                                //Mantém uma lista dos IDs lidos para validar se o usuário está escolhendo um ID listado
    int idSelecionado = 0;                                            //Guarda o ID que o usuário escolheu
    int idPapelLido = 0, idUsuarioLido = 0;                           //Guardam ID do usuário e papel lidos
    int contador = 0;                                                 //Variável de controle de índices do vetor listaIds
    char nomeUsuarioLido[MAX_DADOS], sobrenomeUsuarioLido[MAX_DADOS]; //Guardam o nome e sobrenome lidos do usuário

    ponteiroArquivos = fopen(arquivoUsuarios, "r"); //Abre o arquivo

    printf("\n\t********* LISTANDO USUÁRIOS COM PAPEL: %s *********\n", descreverNomePapel(idPapelProcurado));

    /*Loop para passar pelas linhas do arquivo verificando os papéis dos usuários e quando o papel é compatível com o que 
    foi passado no parâmetro, ele exibe e guarda no vetor o valor do ID desse usuário*/
    while (!feof(ponteiroArquivos))
    {
        //Limpa variáveis para evitar possíveis erros na releitura
        idUsuarioLido = 0;
        idPapelLido = 0;

        //Lê as linhas até o final do arquivo, atribuindo o id da linha na variável id com formato inteiro
        fscanf(ponteiroArquivos, "%d | %s | %s | %s | %s | %s | %s | %d\n", &idUsuarioLido, temp, temp, temp, nomeUsuarioLido, sobrenomeUsuarioLido, temp, &idPapelLido);
        //Se o papel lido for igual ao que se está procurando, exibe o usuário na lista e salva seu ID no vetor
        if (idPapelLido == idPapelProcurado)
        {
            listaIds[contador] = idUsuarioLido;
            printf("\n[%d] %s %s", listaIds[contador], nomeUsuarioLido, sobrenomeUsuarioLido);
            contador++;
        }
    }
    fecharArquivo(ponteiroArquivos); //Fecha o arquivo

    //Encerra a função se não haviam usuários cadastrados com o papel escolhido
    if (contador == 0)
    {
        printf("\n# FALHA - Não há usuários cadastrados com esse papel\n");
        return 0;
    }

    //Solicita que o usuário selecione um ID e valida se esse ID está entre os listados (permitidos)
    do
    {
        printf("\n\n> Selecione o número que corresponde ao usuário desejado ou 0 para encerrar o programa: ");
        setbuf(stdin, NULL);
        //Valida a entrada do usuário
        while (scanf("%d", &idSelecionado) != 1)
        {
            printf("\n# FALHA - Ocorreu um erro na leitura. Digite novamente:\n> ");
            setbuf(stdin, NULL);
        };

        //Usuário cancelou
        if (idSelecionado == 0)
        {
            finalizarPrograma();
        }

        //Passa pelos IDs do vetor e quando o ID que o usuário escolheu for encontrado retorna esse ID
        for (int i = 0; i < contador; i++)
        {
            if (idSelecionado == listaIds[i])
            {
                return idSelecionado;
            }
        }
        //Se ainda não retornou o ID é porque o usuário selecionou um ID que não estava na lista
        printf("\n# FALHA - Esse ID não está na listagem, tente novamente informando um ID que está na lista mostrada");
    } while (1);
}

/**
 * Retorna o nome do papel com base no ID passado como parâmetro
 * @param idPapel é o ID do papel a ser descrito: 1-Coordenador, 2-Professor ou 3-Estudante
 * @return o nome descrito do papel
 */
char *descreverNomePapel(short int idPapel)
{
    if (idPapel == 1)
    {
        return "Coordenador";
    }
    else if (idPapel == 2)
    {
        return "Professor";
    }
    return "Estudante";
}

/**
 * Lista todos as disciplinas de um curso específico passado no parâmetro e solicita para o usuário escolher a desejada
 * @param idCurso o ID do curso para listar as disciplinas
 * @return ID da disciplina escolhida dentre as listadas; 0 em caso de falha
 */
int selecionarDisciplina(short int idCurso)
{
    //Teste do arquivo
    if (testarArquivo(arquivoDisciplina))
        return 0;

    int listaIds[MAX];                         //Mantém uma lista dos IDs lidos para validar se o usuário está escolhendo um ID listado
    int idSelecionado = 0;                     //Guarda o ID que o usuário escolheu
    int idCursoLido = 0, idDisciplinaLida = 0; //Guardam ID do curso e disciplina lidos
    int contador = 0;                          //Variável de controle de índices do vetor listaIds
    char nomeDisciplinaLida[MAX_DADOS];        //Guarda o nome da disciplina lida

    ponteiroArquivos = fopen(arquivoDisciplina, "r"); //Abre o arquivo

    printf("\n\t********* LISTANDO DISCIPLINAS DO CURSO *********\n");
    while (!feof(ponteiroArquivos))
    {
        //Limpa variáveis para evitar possíveis erros na releitura
        idDisciplinaLida = 0;
        idCursoLido = 0;

        //Lê as linhas até o final do arquivo, atribuindo o ID da linha na variável ID com formato inteiro
        fscanf(ponteiroArquivos, "%d | %[^|] | %d | %[^\n]", &idDisciplinaLida, nomeDisciplinaLida, &idCursoLido, temp);
        //Se o curso lido for igual ao que se está procurando, exibe a disciplina na lista e salva seu ID no vetor
        if (idCursoLido == idCurso)
        {
            listaIds[contador] = idDisciplinaLida;
            printf("\n[%d] %s", listaIds[contador], nomeDisciplinaLida);
            contador++;
        }
    }
    fecharArquivo(ponteiroArquivos); //Fecha arquivo

    //Encerra a função se não haviam disciplinas cadastrados no curso escolhido
    if (contador == 0)
    {
        printf("\n# FALHA - Não há disciplinas cadastrados nesse curso\n");
        return 0;
    }

    //Solicita que o usuário selecione um ID e valida se esse ID está entre os listados (permitidos)
    do
    {
        printf("\n\n> Selecione o número que corresponde à disciplina desejada ou 0 para encerrar o programa: ");
        setbuf(stdin, NULL);
        //Valida a entrada do usuário
        while (scanf("%d", &idSelecionado) != 1)
        {
            printf("\n# FALHA - Ocorreu um erro na leitura. Digite novamente:\n> ");
            setbuf(stdin, NULL);
        };

        //Usuário cancelou
        if (idSelecionado == 0)
        {
            finalizarPrograma();
        }

        //Passa pelos IDs do vetor e quando o ID que o usuário escolheu for encontrado retorna esse ID
        for (int i = 0; i < contador; i++)
        {
            if (idSelecionado == listaIds[i])
            {
                return idSelecionado;
            }
        }
        //Se ainda não retornou o ID é porque o usuário selecionou um ID que não estava na lista
        printf("\n# FALHA - Esse ID não está na listagem, tente novamente informando um ID que está na lista mostrada.");
    } while (1);
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
 * Essa função é capaz de exibir a descrição da disciplina, alterar a descrição da disciplina e 
 * alterar o professor da disciplina
 * @param idDisciplina ID da disciplina que deseja realizar uma das operações
 * @param verDescrição flag booleana para visualizar a descrição da disciplina
 * @param alterarDescricao flag booleana para alterar a descrição da disciplina
 * @param alterarProfessor flag booleana para alterar o professor da disciplina
 */
void operarDisciplina(int idDisciplina, short int verDescricao, short int alterarDescricao, short int alterarProfessor)
{
    //Se algum ID passado for 0 é porque houve uma falha na escolha da disciplina, então nem executa a função
    if (idDisciplina == 0)
        return;

    //Teste do arquivo
    if (testarArquivo(arquivoDisciplina))
        return;

    ponteiroArquivos = fopen(arquivoDisciplina, "r"); //Abre o arquivo

    //Loop que faz passar pelas linhas do arquivo
    while (!feof(ponteiroArquivos))
    {
        //Lê as linhas até o final do arquivo, atribuindo às variáveis, os dados conforme a formatação
        fscanf(ponteiroArquivos, "%d | %[^|] | %d | %d | %[^\n]", &d.codigo, d.nome, &d.idCurso, &d.idProfessor, d.descricao);

        //Se o código da disciplina é igual ao ID da disciplina que o usuário deseja operar
        if (d.codigo == idDisciplina)
        {
            //Fechar o arquivo já que não é mais necessário
            fecharArquivo(ponteiroArquivos);

            if (verDescricao)
                printf("\n# Descrição da disciplina %s\n¬ \"%s\"\n", d.nome, d.descricao);

            if (alterarDescricao || alterarProfessor)
            {
                char linhaAntiga[MAX * 5], linhaAtualizada[MAX * 5];
                //Insere os dados atuais que serão substituidos na variável linhaAntiga
                sprintf(linhaAntiga, "%d | %s| %d | %d | %s\n", d.codigo, d.nome, d.idCurso, d.idProfessor, d.descricao);

                if (alterarDescricao)
                {
                    //Colher a nova descrição e validar se não está vazia
                    do
                    {
                        memset(&d.descricao[0], 0, sizeof(d.descricao));
                        printf("\n> Insira a nova descrição da disciplina e pressione ENTER: ");
                        setbuf(stdin, NULL);
                        scanf("%[^\n]", d.descricao);

                        //Valida se a descrição não está vazia
                        if (!strcmp(d.descricao, ""))
                        {
                            printf("\n# FALHA - A descrição da disciplina não pode ficar vazia.\n");
                        }
                        else
                        {
                            break;
                        }
                    } while (1);
                }

                if (alterarProfessor)
                {
                    d.idProfessor = selecionarUsuario(2);
                }
                //Insere os dados atuais que serão substituidos na variável linhaAtualizada
                sprintf(linhaAtualizada, "%d | %s| %d | %d | %s\n", d.codigo, d.nome, d.idCurso, d.idProfessor, d.descricao);
                atualizarLinhaArquivo(arquivoDisciplina, linhaAntiga, linhaAtualizada);
            }
            //Finaliza a função já que já encontrou a disciplina e fez as operações necessárias
            return;
        }
    }
    fecharArquivo(ponteiroArquivos); //Fecha arquivo
    //Se chegar aqui não encontrou a disciplina selecionada
    printf("\n# FALHA - A disciplina solicitada não foi localizada\n");
}

/**
 * Matricula o estudante com ID passado no primeiro parâmetro na disciplina passada com ID no segundo parâmetro, 
 * criando o registro do usuário no arquivo de notas
 * @param idEstudante é o ID do estudante que deseja matricular
 * @param idDisciplina é o ID da disciplina onde deseja matricular
 */
void matricularEstudanteDisciplina(int idEstudante, int idDisciplina)
{
    //Se algum ID passado for 0 é porque houve uma falha na escolha do estudante ou disciplina, então nem executa a função
    if (idEstudante == 0 || idDisciplina == 0)
        return;

    //Validação do arquivo
    if (testarArquivo(arquivoNotas))
        return;

    /*Verificar se o estudante já está matriculado nessa disciplina*/
    ponteiroArquivos = fopen(arquivoNotas, "r"); //Abre arquivo

    //Loop que passa pelas linhas verificando se encontra o id do estudante e disciplina, que serão matriculados
    while (!feof(ponteiroArquivos))
    {
        //Lê as linhas até o final do arquivo, enviando os dados para as variáveis da estrutura
        fscanf(ponteiroArquivos, "%d | %f | %f | %d | %d", &n.codigo, &n.nota1, &n.nota2, &n.idEstudante, &n.idDisciplina);
        //Se a linha lida possui id do estudante igual o ID do estudante que será matriculado e o ID da disciplina também
        if (n.idEstudante == idEstudante && n.idDisciplina == idDisciplina)
        {
            //Estudante já está matriculado na disciplina
            fecharArquivo(ponteiroArquivos); //Fechar o arquivo já que não é mais necessário
            printf("\n# FALHA - Esse estudante já está matriculado nessa disciplina!\n");
            return;
        }
    }
    fecharArquivo(ponteiroArquivos); //Fecha arquivo

    /*Inicia o processo de matrícula do aluno*/
    char linhaNota[MAX];                       //Variável que guarda a linha com os dados para inserir no arquivo de notas
    int codigo = pegarProximoId(arquivoNotas); //Gera o código da linha para o arquivo de notas

    ponteiroArquivos = fopen(arquivoNotas, "a"); //Abre o arquivo

    //Passar dados default para a variável que será inserida no arquivo
    sprintf(linhaNota, "%d | %.2f | %.2f | %d | %d\n", codigo, -1.00, -1.00, idEstudante, idDisciplina);

    //Insere a string com todos os dados no arquivo
    if (fputs(linhaNota, ponteiroArquivos) == EOF)
    {
        perror("\n# ERRO - Problema para inserir os dados no arquivo!\n");
    }
    else
    {
        printf("\n# SUCESSO - Matrícula efetuada!\n");
    }

    fecharArquivo(ponteiroArquivos); //Fecha o arquivo
}

/**
 * Essa função pode exibir e/ou alterar as notas de determinado estudante
 * @param idEstudante ID do estudante que deseja realizar uma das operações
 * @param idDisciplina ID da disciplina que deseja realizar uma das operações
 * @param verNotas flag booleana para exibir as notas do estudante selecionado
 * @param alterarNotas flag booleana para alterar as notas do estudante selecionado
 */
void operarNotas(int idEstudante, int idDisciplina, short int verNotas, short int alterarNotas)
{
    //Se algum ID passado for 0 é porque houve uma falha na escolha do estudante ou disciplina, então nem executa a função
    if (idEstudante == 0 || idDisciplina == 0)
        return;

    //Teste do arquivo
    if (testarArquivo(arquivoNotas))
        return;

    ponteiroArquivos = fopen(arquivoNotas, "r"); //Abre o arquivo

    while (!feof(ponteiroArquivos))
    {
        //Lê as linhas até o final do arquivo, atribuindo o os valores da linha, nas respectivas variáveis
        fscanf(ponteiroArquivos, "%d | %f | %f | %d | %d", &n.codigo, &n.nota1, &n.nota2, &n.idEstudante, &n.idDisciplina);

        //Se o ID do estudante da linha é igual ao ID do estudante que foi passado no parâmetro e o ID da disciplina igual ao ID da disciplina no parâmetro
        if (n.idEstudante == idEstudante && n.idDisciplina == idDisciplina)
        {
            fecharArquivo(ponteiroArquivos); //Fechar o arquivo já que não é mais necessário

            //Se a flag ver notas está setada, vai exibir as notas do estudante solicitado
            if (verNotas)
            {
                //Se a nota for negativa, significa que o valor default (-1) está inserido
                if (n.nota1 < 0)
                {
                    //Exibe apenas um traço na nota
                    printf("\n¬ Nota 1: -");
                }
                else //Senão mostra o valor da nota
                {
                    printf("\n¬ Nota 1: %.2f", n.nota1);
                }

                //Mesma validação de nota negativa para nota 2
                if (n.nota2 < 0)
                {
                    printf("\n¬ Nota 2: -\n");
                }
                else
                {
                    printf("\n¬ Nota 2: %.2f\n", n.nota2);
                }
            }

            //Se a flag alterar notas está setada, vai alterar as notas do estudante solicitado
            if (alterarNotas)
            {
                char linhaAntiga[MAX], linhaAtualizada[MAX]; //Guardam a linha que será substituída e a linha que irá no lugar desta
                char notaDigitada[MAX];
                int operador = 0; //Recebe o valor da escolha da nota que o usuário quer alterar
                //Passando dados atuais para a linha que será substituída posteriormente
                sprintf(linhaAntiga, "%d | %.2f | %.2f | %d | %d\n", n.codigo, n.nota1, n.nota2, n.idEstudante, n.idDisciplina);

                do
                {
                    printf("\n> Escolha qual nota quer alterar: ");
                    printf("\n[1] Nota 1");
                    printf("\n[2] Nota 2");
                    printf("\n> Digite o número: ");
                    //Loop para colher as notas e validar
                    do
                    {
                        setbuf(stdin, NULL);
                        //Valida a entrada que o usuário digitou
                        while (scanf("%d", &operador) != 1)
                        {
                            printf("\n# FALHA - Ocorreu um erro na leitura do valor informado.\n> Tente novamente: ");
                            setbuf(stdin, NULL);
                        };

                        if (operador == 1)
                        {
                            //Loop para validar nota digitada
                            do
                            {
                                printf("\n> Insira a nota 1 (00.00): ");
                                setbuf(stdin, NULL);
                                scanf("%[^\n]", notaDigitada);
                            } while (!validarNota(notaDigitada));
                            n.nota1 = atof(notaDigitada);
                            break;
                        }
                        else if (operador == 2)
                        {
                            //Loop para validar nota digitada
                            do
                            {
                                printf("\n> Insira a nota 2 (00.00): ");
                                setbuf(stdin, NULL);
                                scanf("%[^\n]", notaDigitada);
                            } while (!validarNota(notaDigitada));
                            n.nota2 = atof(notaDigitada);
                            break;
                        }
                        else
                        {
                            printf("\n# FALHA - Digite o número correspondente à nota que quer alterar da lista exibida!\n> ");
                        }
                    } while (1);
                    printf("\n# <?> - Deseja atualizar as notas do estudante novamente? [s/n]\n>");
                    setbuf(stdin, NULL);
                    if (getchar() != 's')
                        break; //Usuário deseja parar de alterar as notas desse estudante, sai do loop
                    setbuf(stdin, NULL);
                } while (1);

                sprintf(linhaAtualizada, "%d | %.2f | %.2f | %d | %d\n", n.codigo, n.nota1, n.nota2, n.idEstudante, n.idDisciplina);

                atualizarLinhaArquivo(arquivoNotas, linhaAntiga, linhaAtualizada);
            }
            return;
        }
    }
    fecharArquivo(ponteiroArquivos); //Fecha arquivo

    //Se chegar aqui não encontrou a linha com o estudante e disciplina solicitados
    printf("\n# ERRO - Estudante não está matriculado na disciplina solicitada.\n");
}

/**
 * Realiza a validação da nota passada no parâmetro, verificando se contém vírgula, seu tamanho, formato e intervalo
 * @param nota é a nota a ser verificada em formato string
 * @return 1 caso a nota passe pelos testes; 0 caso seja inválida
 */
short int validarNota(char *nota)
{
    //Variáveis para guardar as informações digitadas separadas
    char inteiros[MAX], decimais[MAX];

    //Valida se o usuário digitou ',' ao invés de '.', necessário validar isso pois o primeiro número antes da vírgula será considerado válido apenas se o usuário digitar números com ,
    if (sscanf(nota, "%[^, \t\n],%[^\n]", inteiros, decimais) == 2)
    {
        printf("\n# FALHA [NOTA INVÁLIDA]: para nota decimal deve ser utilizado '.' (ponto) ao invés de ',' (vírgula), no formato '00.00'\n");
        return 0;
    }

    //Valida tamanho da string
    if (strlen(nota) > 5 || strlen(nota) < 1)
    {
        printf("\n# FALHA [QUANTIDADE DE CARACTERES]: verifique a nota digitada, deve conter entre 1 e 5 caracteres, você inseriu %d caracteres!\n", strlen(nota));
        return 0;
    }

    //Valida se a nota quando convertida para float retorna 0 (falha na conversão) e o usuário não digitou um 0 propositalmente
    if (!atof(nota) && strcmp(nota, "0") && strcmp(nota, "00") && strcmp(nota, "00.0") && strcmp(nota, "00.00"))
    {
        printf("\n# FALHA [NOTA INVÁLIDA]: verifique a nota digitada, deve estar no formato '00.00'\n");
        return 0;
    }

    //Valida intervalo da nota
    if (atof(nota) < 0 || atof(nota) > 10)
    {
        printf("\n# FALHA [NOTA INVÁLIDA]: verifique a nota digitada, deve estar entre 0 e 10\n");
        return 0;
    }

    return 1;
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

/**
 * Fecha o arquivo verificando se não houve erro ao fechar, se houver encerra o programa para prevenir problemas posteriores
 */
void fecharArquivo(FILE *arquivo)
{
    if (fclose(arquivo))
    {
        system("cls || clear");
        printf("\n# ERRO - Ocorreu um erro ao fechar um arquivo necessário, o programa foi abortado para prevenir mais erros.");
        printf("\n# Contate o desenvolvedor para verificar o problema.");
        finalizarPrograma();
    }
}