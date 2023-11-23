#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> 
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKENS 100

static int job_counter = 0; // Inicializa el contador de trabajos
pid_t job_id = 0;

void print_prompt();
void cd_command(char *directory);
int execute_command(char *tokens[],int cant,char *commands[],int countcom);
int mode_stdin();
int mode_batch_file();
int execute_line(char*);
int execute_commands_pipe(char *commands[],int countcom, int background);
int exec_process(char *command);
int get_new_job_id();
int compararCadenas(const char *cadena1, const char *cadena2);
void sigtstp_handler(int sig);
void sigquit_handler(int sig);
void sigint_handler(int sig);


int main(int argc,char *argv[]) 
{
    int excute_state;
    signal (SIGINT, sigint_handler); // CTRL + C
    signal (SIGTSTP, sigtstp_handler); // CTRL + Z
    signal (SIGQUIT, sigquit_handler); // CTRL + /

    if(argc == 1){
        excute_state = mode_stdin();
    }else{
        excute_state = mode_batch_file(argv[1]); //./myshell ../resources/Ejemplo.txt
    }

    return excute_state;
}

void print_prompt() 
{
    char cwd[1024];
    char hostname[128];
    if ((getcwd(cwd, sizeof(cwd)) != NULL )&&(gethostname(hostname, sizeof(hostname)) == 0)) {
        printf("\n%s@%s:%s$ ", getenv("USER"), hostname, cwd); //imprimo el prompt
    }
}

void cd_command(char *directory) 
{
    if (directory == NULL) 
    {
        directory = getenv("HOME"); // voy al directorio de inicio si no ingreso parametros
    } else if (strcmp(directory, "-") == 0) {
        directory = getenv("OLDPWD"); // vuelvo al directorio anterior si ingreso cd --
    }

    if (chdir(directory) == 0)  //chdir cambia al directorio que se le pasa como parametro
    {
        setenv("OLDPWD", getenv("PWD"), 1); //Actualiza la variable de entorno "OLDPWD" con el valor de PWD
        setenv("PWD", directory, 1); //Actualiza la variable de entorno "PWD" al directorio actual 
    } else {
        //no se puedo acceder al directorio porque no existe o no se puede acceder  
        perror("chdir");
    }
}

int execute_command(char *tokens[], int count,char *commands[],int countcom) 
{
    int val_ret = 0;
    if ((tokens == NULL) && (commands == NULL)) {

        return 0;  // No se enviaron comandos

    } else if((tokens != NULL)){
        if (strcmp(tokens[0], "cd") == 0) {
            
            cd_command(tokens[1]);

        } else if (strcmp(tokens[0], "clr") == 0) {
            
            system("clear");

        } else if (strcmp(tokens[0], "echo") == 0) {
            
            for (int i = 1; tokens[i] != NULL; i++) {
                printf("%s ", tokens[i]);
            }

            printf("\n");

        } else if (strcmp(tokens[0], "quit") == 0) {
            
            return 1;  // Señal para volver a la shell

        } else if ( count>0 && strcmp(tokens[count - 1],"&") == 0) {
            if(countcom == 0){
                // Si el último token contiene un ampersand BACKGROUND
                tokens[count - 1] = '\0'; // Eliminar el ampersand y reemplazo por un NULL
                pid_t pid = fork();
                if (pid == 0) { // Proceso hijo
                    if (execvp(tokens[0], tokens) == -1) {
                        perror("execvp in backgroun");
                        exit(EXIT_FAILURE);
                    } 
                    
                } else if (pid < 0) {
                    perror("fork");
                } else { // Proceso padre
                    printf("[%d] %d\n", get_new_job_id(), pid);
                }
            }else{
                //Ejecuto comandos con pipes
            val_ret = execute_commands_pipe(commands,countcom,1);
            }

        }
    } else {
        if(countcom == 0){
            // El comando no esta previsto
            pid_t pid = fork();
            if (pid == 0) { // Proceso hijo
                if (execvp(tokens[0], tokens) == -1) { //TODO debe recibir PATCH
                    perror("execvp");// En caso de error
                    exit(EXIT_FAILURE);
                }  
            } else if (pid < 0) {
                perror("fork"); // Error en fork
            } else { // Proceso padre
                job_id = pid;
                int status;
                waitpid(pid, &status, WUNTRACED);//esperar a que el proceso hijo termine o WUNTRACED detine
                job_id = 0;
            }
        }else{
            //Ejecuto comandos con pipes
            val_ret = execute_commands_pipe(commands,countcom,0);
        }
    }

    return val_ret;
}

int execute_commands_pipe(char *commands[],int countcom,int background)
{   
        // create pipes
        int pipes[countcom][2];
        for (int i = 0; i<countcom; i++)
        {
            if(pipe(pipes[i]) == -1)
            {
                perror("Error al crear la tubería");
                return 1;
            }
        }

        int n_proc = countcom+1;
        pid_t pids[n_proc];

        int i;
        for (i=0;i<n_proc;i++){
            if ((pids[i]= fork()) == 0)
                break;
            else if (pids[i]< 0)
            {
                perror("Error al crear proceso hijo");
                exit(EXIT_FAILURE);
            }
        }
        
        if (i == n_proc) // parent process
        {
            for (int i = 0; i<countcom; i++)
            {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            for (int i = 0; i<n_proc; i++){
                wait(NULL);
            }

            return 0;
        }

        if (i == 0)
        {
            // primer programa
            dup2(pipes[i][1], STDOUT_FILENO); // extremo de escritura al stdout
        }
        else if (i == n_proc-1)
        {
            // ultimo programa
            dup2(pipes[i-1][0], STDIN_FILENO); // extremo de lectura al stdin
        }
        else 
        {
            dup2(pipes[i-1][0], STDIN_FILENO); // extremo izquierdo de lectura al stdin
            dup2(pipes[i][1], STDOUT_FILENO); // extremo drecho de escritura al stdout
        }

        for (int i = 0; i<countcom; i++)
        {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }

        if(exec_process(commands[i])){
            exit(EXIT_FAILURE); // failed executing program
        }
        
}

int exec_process(char *command)
{   
    char input[MAX_INPUT_SIZE];//buffer auxiliar para guardar la entrada
    char *tokens[MAX_TOKENS];
    int token_count;
    strncpy(input, command, sizeof(input));    // Copia la línea de entrada en la variable 'input'
    // parseo las inputs separadas por espacio, tab o salto de linea
    token_count = 0;
    char *token = strtok(input, " \t\n"); //Divide la cadena utilizando espacios en blanco, tabulaciones y saltos de línea como delimitadores
    while (token != NULL) {
        tokens[token_count] = token; //genero los tokens
        token_count++;
        token = strtok(NULL, " \t\n"); //Cuando strtok encuentra NULL como primer argumento, sabe que debe continuar desde la posición actual en la cadena donde se detuvo en la última llamada
        }
    tokens[token_count] = NULL; // ultimo elemeno es vacio

    if (execvp(tokens[0], tokens) == -1) {
        return 1;
    }else{
        return 0;
    }
    
}
int mode_stdin()
{
    char input[MAX_INPUT_SIZE];
    int retorno;

    while (1) {
        print_prompt();
        fgets(input, sizeof(input), stdin); //tomo la entrada del usuario y almaceno en input
        retorno = execute_line(input);
        if (retorno)
        {
            break;
        }
    }

    return retorno; // se ejecuto el comando correctamente
}

int mode_batch_file(char* patch)
{
    FILE *archivo;
    char linea[MAX_INPUT_SIZE];

    archivo = fopen(patch, "r");

    printf("Batch file loading...\n");

    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }   

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        print_prompt();
        printf("%s", linea);
        execute_line(linea);
    }

    return 0;
}

int execute_line(char* line)
{

    char input[MAX_INPUT_SIZE];//buffer auxiliar para guardar la entrada
    char *tokens[MAX_TOKENS];
    int token_count;
    char command[MAX_INPUT_SIZE]; //buffer auxiliar para guardar la entrada
    char *commands[10];  // Soporta hasta 10 comandos en una tubería
    int command_count = 0;
    
    strncpy(command, line, sizeof(command));    // Copia la línea de entrada en la variable 'command'
        
    // ----------------------- Dividir la entrada en comandos mediate '|' TUBERIAS------------------------------------//
    char *tokenpipe = strtok(command, "|");// Tokenizar el comando de entrada usando "|"
    if((compararCadenas(tokenpipe, line) != 0)){ // existe al menos un pipe
        while ((tokenpipe != NULL) && command_count < 10) {
            commands[command_count] = tokenpipe; //relleno los comandos separados por una pipe y los cuento
            command_count++;// Incrementar el recuento de comandos
            tokenpipe = strtok(NULL, "|"); // Encontrar el próximo token (comando) en la cadena
        }
    }
    //------------------------ Dividir por comandos separados -------------------------------------------------------//
   
    if(command_count == 0){
        strncpy(input, line, sizeof(input));    // Copia la línea de entrada en la variable 'input'
        // parseo las inputs separadas por espacio, tab o salto de linea
        token_count = 0;
        char *token = strtok(input, " \t\n"); //Divide la cadena utilizando espacios en blanco, tabulaciones y saltos de línea como delimitadores
        while (token != NULL) {
            tokens[token_count] = token; //genero los tokens
            token_count++;
            token = strtok(NULL, " \t\n"); //Cuando strtok encuentra NULL como primer argumento, sabe que debe continuar desde la posición actual en la cadena donde se detuvo en la última llamada
        }
        tokens[token_count] = NULL; // ultimo elemeno es vacio
    }

    //----------------------------trato de ejecutar el comando (si se realizo bien retorno 0)-------------------------//
    if(command_count == 0){
        if (execute_command(tokens,token_count,NULL,0) != 0) {
            return 1;  // si no se pudo ejecutar o es un quit corto la ejecucion del ciclo
        }
    }else{
        //Ejecuto un proceso con Pipes
        if (execute_command(NULL,0,commands,command_count)!= 0) {
            return 1;  // si no se pudo ejecutar o es un quit corto la ejecucion del ciclo
        }
    }
    
    return 0;
}

// Función para comparar dos cadenas
int compararCadenas(const char *cadena1, const char *cadena2) {
    // Si ambas cadenas son nulas, son iguales
    if (cadena1 == NULL && cadena2 == NULL) {
        return 0;
    }

    // Si una cadena es nula y la otra no, son diferentes
    if (cadena1 == NULL || cadena2 == NULL) {
        return 1;
    }

    // Comparar las cadenas utilizando strcmp
    return strcmp(cadena1, cadena2);
}

// Función para obtener un nuevo ID de trabajo
int get_new_job_id() 
{
    return ++job_counter;
}

void sigint_handler(int sig) 
{
    if (job_id > 0) {
        // Envía SIGINT al trabajo en primer plano en lugar de terminar la shell
        kill(job_id, SIGINT);
    }
}

void sigtstp_handler(int sig) 
{
    if (job_id > 0) {
        // Envía SIGTSTP al trabajo en primer plano en lugar de suspender la shell
        kill(job_id, SIGTSTP);
    }
}

void sigquit_handler(int sig) 
{
    if (job_id > 0) {
        // Envía SIGQUIT al trabajo en primer plano en lugar de terminar la shell
        kill(job_id, SIGQUIT);
    }
}