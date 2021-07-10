#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

#define BUFFER_SIZE 1024

int wait = 1;  //cliente fica em pending enquanto wait == 1


//Função que compara duas strings
int compare(char a[],char b[])  {  
    int i=0; 
    while(a[i]!='\0' &&b[i]!='\0') 
    {  
       if(a[i]!=b[i]) return 0;  
       i++;  
    }  
    return 1;  
}


//Função que devolve uma string com o caminho do pipe de comunicação entre o servidor e o cliente
char *pidNameTmp(){
    char *strPidd = malloc(128);
    sprintf(strPidd, "tmp/%d", getpid());
    return strPidd;
}


//Função que devolve uma string com o pid do cliente
char *pidName(){
    char *strPidd = malloc(128);
    sprintf(strPidd, "%d", getpid());
    return strPidd;
}


//Função que concatena uma array de strings numa string
char *strTrans(int argc, char*argv[]){
    char *ret = malloc(BUFFER_SIZE);
    int r = 0;
    char *pid = pidNameTmp();
    for(int i=0;pid[i] != '\0';i++)
        ret[r++] = pid[i];
    ret[r++] = ' ';
    for(int i=2;i<argc;i++){
        for(int j=0;argv[i][j] != '\0';j++)
            ret[r++] = argv[i][j];
        ret[r++] = ' ';
    }
    ret[r++]= '\0';
    return ret;
}


//Função que pede o status ao servidor
void pedidoStatus(int server){
    int statOut;
    int bytes;
    char buffer[BUFFER_SIZE];

    char *pid = pidNameTmp();

    //Criar pipe de retorno de status
    if(mkfifo(pid, 0666)< 0){
        perror("Erro ao criar pipe de retorno de status\n");
        exit(1);
    }

    write(server, pid, BUFFER_SIZE);

    if((statOut= open(pid, O_RDONLY))< 0){
        perror("Erro ao abrir pipe de retorno de status\n");
        exit(1);
    }
    
    //Ler e imprimir status
    while((bytes= read(statOut, buffer, BUFFER_SIZE))> 0)
            write(1, buffer, bytes);
    //printf("%s\n", pidName());
    close(statOut);
    unlink(pid);
    
}


//Handler ativado quando o servidor inicia o processo de transform do cliente
void sig_processing(int signum){
    printf("Processing...\n");
}

//Handler ativado quando o servidor tiver feito o trabalho
void sig_done(int signum){
    printf("Done.\n");
    wait = 0;
}

//Handler ativado em caso de input inválido
void sig_invalido(int signum){
    printf("Input Inválido.\n");
    wait = 0;
}


//Função que envia o pedido de transform ao servidor
void pedidoTransform(char *info, int server){

    //Processing Signal
    if(signal(SIGUSR1, sig_processing) == SIG_ERR){
        perror("Error setting processing signal");
        exit(1);
    }

    //Done Signal
    if(signal(SIGUSR2, sig_done) == SIG_ERR){
        perror("Error setting done signal");
        exit(1);
    }

    //Invalid Signal
    if(signal(SIGBUS, sig_invalido) == SIG_ERR){
        perror("Error setting done signal");
        exit(1);
    }

    //Write to buffer
    int size = 0;
    for(;info[size] != '\0';size++);
    write(server, info, size + 1);
    printf("Pending...\n");
    while(wait);
}


//Função que abre o pipe de acesso ao servidor
int openPipe(){
    int server;
    //Conectar com servidor
    if((server= open("tmp/server", O_WRONLY))< 0){
        perror("Servidor não inicializado\n");
        exit(1);
    }
    else{
        printf("Conecção establecida com servidor\n");
    }
    return server;
}


//Função main
int main(int argc, char *argv[]){
    int server = openPipe();

    
    if(argc == 1){
        printf("./aurras status\n");
        printf("./aurras transform input-filename output-filename filter-id-1 filter-id-2 ...\n");
    }
    else if(argc == 2 && compare(argv[1], "status")) 
        pedidoStatus(server);
    else if(argc > 4 && compare(argv[1], "transform")) 
        pedidoTransform(strTrans(argc, argv), server);
    else 
        printf("Comando inválido\n");
    
    return 0;
}