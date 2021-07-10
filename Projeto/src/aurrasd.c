#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 1024



//Estrutura de uma lista ligada que armazena as informações de um filtro
typedef struct listaLigada{
	char *filtro;
	char *executavel;
	int cap_max;
	int utilizados;
	struct listaLigada *prox;
}*Lista;


//Estrutura de uma lista ligada que armazena as informações de um processo
typedef struct lista_espera{
	int task;
	int pid;
	char **args;
	int arg_num;
	int disp;
	struct lista_espera *prox;
}*ListaEspera;


int flag = 1;               //programa lê do pipe enquanto flag == 1
int server;                 //o apontador do pipe entre cliente e servidor
int chields = 0;            //variável que indica o numero atual de processos filhoes em execução
int tarefa= 0;              //variável que indica o numero da task atual
char *filter_folder;        //variável com o caminho para a pasta dos filtros
Lista filtros;              //lista de todos os filtros
ListaEspera listaEspera;    //lista dos processos em espera e em execução



//Função que imprime a lista de espera para efeitos de debug
void imprimeLista(){
	printf("Lista de espera\n");
	for(ListaEspera l = listaEspera; l; l = l->prox)
		printf("Elem: %d, %d\n", l->pid, l->disp);
}


//Função que adiciona uma processo à lista de espera dado a sua informação
void addListaEspera(int pid, char **args, int arg_num, int disp){
	ListaEspera l;
	for(l = listaEspera; l && l->prox != NULL; l = l->prox);

	ListaEspera nodo = malloc(sizeof(struct lista_espera));
	nodo-> task= ++tarefa; 
	nodo->pid = pid;
	nodo->args = args;
	nodo->arg_num = arg_num;
	nodo->disp = disp;
	nodo->prox = NULL;

	if(l) l->prox = nodo;
	else listaEspera = nodo;
}


//Função que remove um elemento com um dado pid da lista de espera
void removeListaEspera(int pid){

	  if(!listaEspera) return; 
	  ListaEspera ant= listaEspera;
	  ListaEspera l = listaEspera;
	  for(; l; l= l-> prox){
	    if(l-> pid == pid){
	      if(ant != listaEspera) ant->prox= l-> prox;
	      else listaEspera = l->prox;
	      break;
	    }
	    else ant = ant->prox;
	  }
}


//Função que concatena um array de strings numa string
char *strTrans(int argc, char*argv[]){
    char *ret = malloc(BUFFER_SIZE);
    int r = 0;
    for(int i=0;i<argc;i++){
        for(int j=0;argv[i][j] != '\0';j++)
            ret[r++] = argv[i][j];
    }
    return ret;
}


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


//Função que retorna a informação de uma filtro dado o seu nome
Lista getFiltro(char *filtro){
	for(Lista l = filtros; l; l = l->prox){
		if(compare(l->filtro, filtro)) return l;
	}
	return NULL;
}


//Função que retorna o cumprimento de uma string
int sizeOf(char *s){
	int size = 0;
	for(;s[size] != '\0'; size++);
	return size;
}


//Função que verifica se uma string contém um dado caracter
int contains(char *s1, char s2){
	for(int i=0;s1[i] != '\0'; i++){
		if(s1[i] == s2) return 1;
	}
	return 0;
}


char *lastToqued;
//strtok made in pedrocas.15
char *strToque(char *st, char *lim){
	char *s = st == NULL ? lastToqued : st;
	for(int i=0;s[i] != '\0'; i++){
		if(contains(lim, s[i])) {
			s[i] = '\0';
			lastToqued = &s[i + 1];
			return s;
		}
	}
	return s;
}


//Função que divide uma string em substrings por espaços ou enters
char **get_argumentos(char *linha, int *argNum){
	char *s= strdup(linha);
	int arg_num = 0;
	for(int i=0;s[i] != '\0'; i++) 
		if(s[i] == ' ') arg_num++;
	*argNum = arg_num;
	char **ret = malloc(sizeof(char *) * arg_num);
	char *limitadores = " \n";
	int i=0;
	for(char *token=strToque(s, limitadores); i< arg_num && token != NULL;i++, token = strToque(NULL, limitadores)){
		ret[i] = token;
	}
	return ret;
}


//Função que envia ao cliente as informações para serem impresas no comando status
void printStatus(char *pid){
	int filho;
	if((filho = fork()) < 0){
		perror("Erro filho print status\n");
		_exit(0);
	}
	else if(filho == 0){
		kill(getppid(), SIGUSR1);
		int file_pointer;
		if((file_pointer= open(pid, O_WRONLY | O_NONBLOCK)) < 0){
			perror("Nao abri!\n");
			kill(getppid(), SIGUSR2);
			exit(0);
		}
		for(ListaEspera t= listaEspera; t; t= t-> prox){
			
			char task_number[t-> task> 9? 2: 1];
			sprintf(task_number, "%d", t-> task);
			char **strings = malloc(sizeof(char *) * 30);
			strings[0] = "task #";
			strings[1] = task_number;
			strings[2] = ": transform ";
			int index= 3;
			for(int i= 1; i< t-> arg_num; i++){
				strings[index++] = t-> args[i];
				strings[index++] = " ";
			} 
			strings[index]= "\n";
			char *linha= strTrans(3 + 2 * (t-> arg_num - 1) + 1, strings);
			free(strings);
			write(file_pointer, linha, BUFFER_SIZE);
			//free(linha);
		}

		for(Lista l= filtros; l; l= l-> prox){
			char utilizados[l-> utilizados> 9? 2: 1];
			char max[l-> cap_max> 9? 2: 1];
			sprintf(utilizados, "%d", l-> utilizados);
			sprintf(max, "%d", l-> cap_max);
			char *strings[]= {"filter ", l-> filtro, ": ", utilizados, "/", max, " (runing/max)\n"};
			char *linha= strTrans(7, strings);
			write(file_pointer, linha, BUFFER_SIZE);
			//free(linha);
		}
		char server_pid[10];
		sprintf(server_pid, "%d\n", getppid());
		write(file_pointer, server_pid, sizeOf(server_pid));
		close(file_pointer);
		kill(getppid(), SIGUSR2);
		_exit(0);
	}
	
}


//Função que decrementa o numero de utilizados de um certo filtro
void decrementa(char * filtro){
	for(Lista l = filtros; l; l= l->prox)
		if(compare(l->filtro, filtro)) l->utilizados--;
	
}


//Função que incrementa o numero de utilizados de um certo filtro
void incrementa(char * filtro){
	for(Lista l = filtros; l; l= l->prox)
		if(compare(l->filtro, filtro)) l->utilizados++;
	
}


//Função que retorna o numero de filtros num array de strings iguais ao filtro dado
int quantosFiltros(char *filtro, char **args, int arg_num){
	int num = 0;
	for(int i=3; i<arg_num;i++){
		if(compare(filtro, args[i])) num++;
	}
	return num;
}


//Função que verifica a disponibilidade de uma série de filtros para um cliente
int verificaFiltros(char **args, int arg_num){
	for(Lista l = filtros; l; l = l->prox){
		for(int i=3;i<arg_num; i++){
			if(compare(l->filtro, args[i])){
				int num = quantosFiltros(l->filtro, args, arg_num);
				if(num > l->cap_max) return -1; //impossivel;
				if(l->utilizados + num > l->cap_max) return 0; //ainda nao disponivel
			}
		}
	}
	return 1; //disponivel
}


//Função que incrementa o numero de filtros utilizados por um cliente
void ocupaFiltro(char **args, int arg_num){
	for(int i=3; i < arg_num; i++){
			incrementa(args[i]);
		}
}


//Função que aplica uma série de filtros em série a um dado ficheiro
/*void executar(char **args, int arg_num){

	int pipes[arg_num - 4][2];
	int input, output;

	kill(atoi(&args[0][4]), SIGUSR1);

	for(int i=0; i < arg_num - 3; i++){
		if(i < arg_num - 4){
			if(pipe(pipes[i]) < 0){
				perror("Pipe de execução não criado\n");
				kill(atoi(&args[0][4]), SIGUSR2);
				_exit(0);
			}
		}

		int filho;
		if((filho = fork()) < 0){
			perror("Erro filho print transform\n");
			_exit(0);
		}
		else if(filho == 0){
			if(i == 0){
				if((input = open(args[1], O_RDONLY))< 0){
					perror("Erro ao abrir ficheiro input\n");
					kill(atoi(&args[0][4]), SIGUSR2);
					_exit(0);
				}
			}
			if(i == arg_num - 4){
				if((output= open(args[2], O_CREAT | O_WRONLY, 0640)) < 0){
					perror("Erro ao abrir ficheiro output\n");
					kill(atoi(&args[0][4]), SIGUSR2);
					_exit(0);
				}
			}
			if(i < arg_num - 4) close(pipes[i][0]);

			dup2 (i == 0 ? input : pipes[i - 1][0], 0);
			close(i == 0 ? input : pipes[i - 1][0]);

			dup2 (i == arg_num - 4 ? output : pipes[i][1], 1);
			close(i == arg_num - 4 ? output : pipes[i][1]);


			char *executavel = getFiltro(args[i + 3])->executavel;
			char *pasta[]    = {filter_folder, "/", executavel};
			char *caminho    = strTrans(3, pasta);
			execl(caminho, executavel, NULL);

			_exit(0);
		}
		else{
			if(i != 0) close(pipes[i - 1][0]);
			if(i != arg_num - 4) close(pipes[i][1]);
		}
		wait(NULL);
	}
	kill(atoi(&args[0][4]), SIGUSR2);
}*/


//Função que aplica uma série de filtros em série a um dado ficheiro
void executar(char **args, int arg_num){
	int input, output;

	kill(atoi(&args[0][4]), SIGUSR1);

	for(int i=0; i < arg_num - 3; i++){
		char nome[10];
		sprintf(nome, "tmp/%d%d", i, getpid());
		if(i < arg_num - 4){
			if(open(nome, O_CREAT, 0640) < 0){
				perror("Pipe de execução não criado\n");
				kill(atoi(&args[0][4]), SIGUSR2);
				_exit(0);
			}
		}

		int filho;
		if((filho = fork()) < 0){
			perror("Erro filho print transform\n");
			_exit(0);
		}
		else if(filho == 0){
			if(i == 0){
				if((input = open(args[1], O_RDONLY))< 0){
					perror("Erro ao abrir ficheiro input\n");
					kill(atoi(&args[0][4]), SIGUSR2);
					_exit(0);
				}
			}
			if(i == arg_num - 4){
				if((output= open(args[2], O_CREAT | O_WRONLY, 0640)) < 0){
					perror("Erro ao abrir ficheiro output\n");
					kill(atoi(&args[0][4]), SIGUSR2);
					_exit(0);
				}
			}
			int pipe;
			if(i < arg_num - 4){
				if((pipe= open(nome, O_WRONLY | O_NONBLOCK)) < 0){
					perror("Erro ao abrir pipe atual\n");
					kill(atoi(&args[0][4]), SIGUSR2);
					_exit(0);
				}
			}
			int pipe_ant;
			if(i > 0){
				char nome_ant[10];
				sprintf(nome_ant, "tmp/%d%d", i-1, getppid());
				if((pipe_ant = open(nome_ant, O_RDONLY))< 0){
					perror("Erro ao abrir pipe anterior\n");
					kill(atoi(&args[0][4]), SIGUSR2);
					_exit(0);
				}
			}

			dup2 (i == 0 ? input : pipe_ant, 0);
			close(i == 0 ? input : pipe_ant);
			dup2 (i == arg_num - 4 ? output : pipe, 1);
			close(i == arg_num - 4 ? output : pipe);

			char *executavel = getFiltro(args[i + 3])->executavel;
			char *pasta[]    = {filter_folder, "/", executavel};
			char *caminho    = strTrans(3, pasta);
			execl(caminho, executavel, NULL);

			_exit(0);
		}
		wait(NULL);
	}
	for(int i=0; i < arg_num - 3; i++) {
		char nome[10];
		sprintf(nome, "tmp/%d%d", i, getpid());
		remove(nome);
	}
	kill(atoi(&args[0][4]), SIGUSR2);
}


// Função que inicia o processo filho responsável por aplicar os filtros
int criaProcesso(char **args, int arg_num){
	int filho;
	if((filho = fork()) < 0){
		perror("Erro filho print transform\n");
		_exit(0);
	}
	else if(filho == 0){
		kill(getppid(), SIGUSR1);

		//Executar
		printf("A executar pedido de: %s\n", &args[0][4]);
		executar(args, arg_num);
		printf("A terminar pedido de: %s\n", &args[0][4]);

		kill(getppid(), SIGBUS);
		kill(getppid(), SIGUSR2);
		exit(1);
	}
	return filho;
}


//Função que limpa a lista de espera de processos termiandos e chama processos em espera
void cleanTransform(int del_pid){
	for(ListaEspera l = listaEspera; l;){
		int status;
		pid_t return_pid = waitpid(l-> pid, &status, WNOHANG);
		
		if(return_pid == l-> pid || l->pid == del_pid){
			for(int i=3; i < l->arg_num; i++){
				decrementa(l->args[i]);
			}
			int pid = l->pid;
			l = l->prox;
			removeListaEspera(pid);
		}
		else l = l->prox;

	}
	for(ListaEspera l = listaEspera; l;l = l->prox){
		int verifica = verificaFiltros(l->args, l->arg_num);
		if(l->disp == 0 && verifica){
			l->disp = 1;
			ocupaFiltro(l-> args, l-> arg_num);
			int pid = criaProcesso(l->args, l->arg_num);
			l->pid = pid;
		}
	}
}


//Função que atende a um pedido de transform
void transform(char **args, int arg_num){

	int disp = verificaFiltros(args, arg_num);
	if(disp > 0) ocupaFiltro(args, arg_num);
	else if(disp == -1) {
		printf("Pedido de: %s | Combinação de filtros inválida\n", &args[0][4]);
		kill(atoi(&args[0][4]), SIGBUS);
		return;
	}
	if(disp) printf("Pedido de: %s | Filtros disponiveis\n", &args[0][4]);
	else printf("Pedido de: %s | Filtros indisponiveis | Em espera\n", &args[0][4]);

	int pid = -5;
	if(disp) pid = criaProcesso(args, arg_num);
	addListaEspera(pid, args, arg_num, disp);
}


//Função que recebe constantemente os pedidos dos clientes
void pedidoCliente(){
	if((server= open("tmp/server", O_RDONLY))< 0){//----------------------
		perror("Pipe server encerrado!\n");
		_exit(0);
	}
	else{

		int size;
		char buffer[BUFFER_SIZE];

		while(flag){
			while((size = read(server, buffer, BUFFER_SIZE)) > 0){

				if(sizeOf(buffer) < 15 && sizeOf(buffer)> 0){
					printStatus(buffer);
				}else{
					int arg_num;
					char **args = get_argumentos(buffer, &arg_num);
					transform(args, arg_num);
				}
			}
		}
	}
}


//Função que carrega a informação do ficheiro de filtros
void loadFiltros(char *config_filename){
	int file_info;
	char *linha;
	if((file_info= open(config_filename, 0640, O_RDONLY))< 0){//-------------------------
		perror("Erro ao abrir ficheiro aurrasd.conf!\n");
		exit(0);
	}
	Lista lista= NULL, cabeca= lista;
	int i= 0;
	linha= malloc(1024);
	char c;
    while(read(file_info, &c, 1) == 1 && i < BUFFER_SIZE){
        linha[i++]=c;
        if(c=='\n'){
			Lista nodo= malloc(sizeof(struct listaLigada));
			nodo-> filtro= strToque(linha, " ");
			nodo-> executavel= strToque(NULL, " ");
			nodo-> cap_max= atoi(strToque(NULL, "\n"));
			nodo-> utilizados= 0;
			nodo-> prox= NULL;
			linha= malloc(1024);
			if(!lista){
				lista= nodo;
				cabeca= nodo;
			}
			else{
				lista-> prox= nodo;
				lista= lista-> prox;
			}
			i= 0;
        }
    }
    close(file_info);
	filtros = cabeca;
	printf("Filtros carregados\n");
}


//Função que cria o pipe com nome do servidor
void createPipe(){
	if(mkfifo("tmp/server", 0644)< 0){
		perror("Pipe server nao criado!\n");
		_exit(0);
	}
}


//Handler que encerra o programa
void sig_term(int signum){
	printf("A encerrar\n");
	close(server);
	unlink("tmp/server");
	for(int i= 0; i< chields || listaEspera; i++) wait(NULL);
	flag= 0;
}


//Handler que incrementa o contador de processos filhos ativos
void sig_chield_up(int signum){
	chields++;
}


//Handler que decrementa o contador de processos filhos ativos
void sig_chield_down(int signum){
	chields--;
}


//Handler que elimina um processo da lista de espera após e seu encerramento
void sig_clean(int signum, siginfo_t *si, void *data) {
	cleanTransform(si->si_pid);
}


//Função que cria os sinais
void createSignals(){
	if(signal(SIGTERM, sig_term)== SIG_ERR){
		perror("Erro no sinal SIGTERM!\n");
		exit(0);
	}

	if(signal(SIGUSR1, sig_chield_up)== SIG_ERR){
		perror("Erro no sinal SIGUSR1!\n");
		exit(0);
	}

	if(signal(SIGUSR2, sig_chield_down)== SIG_ERR){
		perror("Erro no sinal SIGUSR2!\n");
		exit(0);
	}

	struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sig_clean;
    if (sigaction(SIGBUS, &sa, 0) == -1) {
    	perror("Erro no sinal SIGBUS!\n");
		exit(0);
	}
}


//Função main
int main(int argc, char *argv[]){

	if(argc < 3) {
		printf("Input inválido\nA encerrar servidor\n");
		return 1;
	}
	filter_folder = argv[2];

	createSignals();

	createPipe();

	loadFiltros(argv[1]);

	pedidoCliente();

	printf("Servidor encerrado\n");

	return 0;
}