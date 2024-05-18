#include "mensajes.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include "server_hora.h"

#define MAXSIZE 256
#define MAX_COMMAND_LENGTH 100

pthread_mutex_t mutex;
pthread_mutex_t usuarios;
pthread_mutex_t conectados;
pthread_mutex_t publicaciones;


pthread_cond_t cond;
int mensaje_no_copiado = 1;
int res;
int err;

int llamar_rpc(char *fecha, char *hora, char *username, char *op){
    //Se llama a la función imprimir_1 del servidor RPC
    CLIENT *clnt;
    enum clnt_stat retval_1;
    int result_1;
    struct server_estructura imprimir_1_arg1;
    char *host;

    host = obtenerIP();
    if (host == NULL) {
        printf("Error: IP_TUPLAS no definido correctamente\n");
        return -1;
    }

    //Crear la conecion con el servidor RPC
    clnt = clnt_create (host, SERVER_HORA, SERVER_HORA_V1, "tcp");
	if (clnt == NULL) {
		clnt_pcreateerror (host);
		exit (1);
	}

    strcpy(imprimir_1_arg1.fecha, fecha);
    strcpy(imprimir_1_arg1.hora, hora);
    strcpy(imprimir_1_arg1.user, username);
    strcpy(imprimir_1_arg1.op, op);
    retval_1 = imprimir_1(imprimir_1_arg1, &result_1, clnt);

    if (retval_1 != RPC_SUCCESS) {
        clnt_perror (clnt, "call failed");
    }
    //Destruir la conexión con el servidor RPC
    clnt_destroy (clnt);

    return result_1;
}
int buscar_registrado(char *user){
    pthread_mutex_lock(&usuarios);
    FILE *fp= fopen("usuarios.txt", "r");
    if (fp != NULL) {
        // Check if the username already exists in the file
        char line[MAXSIZE];
        int username_exists = 0;
        while (fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\n")] = '\0'; // Remove newline character
            if (strcmp(line, user) == 0) {
                username_exists = 1;
                break;
            }
        }
        if (username_exists==0) {
            fclose(fp);
            pthread_mutex_unlock(&usuarios);
            return 1;
        } 
        pthread_mutex_unlock(&usuarios);
        fclose(fp);
    }
    else {
        printf("s > Error opening file usuarios.txt");
        return 2;
    }
    return 0;
}

int buscar_conectado(char *user){
    // Abrir el archivo de texto en modo lectura
    pthread_mutex_lock(&conectados);
    FILE *fc = fopen("conectados.txt", "r");
    if (fc != NULL) {
        char line[MAXSIZE];
        int username_conectado = 0;
        // Leer línea por línea del archivo
        while (fgets(line, sizeof(line), fc) != NULL) {
            line[strcspn(line, "\n")] = '\0'; // Eliminar el carácter de nueva línea

            // Utiliza strtok para dividir la línea en tokens
            char *token = strtok(line, " ");
            
            // Compara solo el primer token (el nombre de usuario)
            if (token != NULL && strcmp(token, user) == 0) {
                username_conectado = 1;
                break;
            }
        }
        if (!username_conectado) {
            fclose(fc);
            pthread_mutex_unlock(&conectados);
            return 1;
        } 
        pthread_mutex_unlock(&conectados);
        fclose(fc);
    }else {
        perror("s > Error opening file");
        return 2;
    }
    return 0;
}

int registrar(char *buffer, char *op) {
    // Si comienza con "REGISTER", el resto del buffer es el nombre de usuario
    if(strlen(buffer -21) > 257){
        printf("s > Nombre de usuario demasiado largo\n");
        return 2;
    }
    char *fecha = strtok(buffer, " ");
    char *hora = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    
    printf("REGISTER FROM %s\n", username);

    int result_rpc = llamar_rpc(fecha, hora, username, op);
    if(result_rpc != 0){
        printf("s > Error al llamar a la función RPC\n");
    }
    
    //Buscar si el usuario ya está registrado
    int result = buscar_registrado(username);

    //Si el usuario ya está registrado, se imprime un mensaje y devuelve 1
    if(result==0){
        printf("s > El nombre de usuario '%s' ya está registrado\n", username);
        return 1; 
    }
    //Si el usuario no está registrado, se agrega al archivo usuarios.txt
    else if(result == 1){
        pthread_mutex_lock(&usuarios);
        FILE *fp = fopen("usuarios.txt", "a+");
        if (fp != NULL) {
            fprintf(fp, "%s\n", username);
            printf("s > Se ha registrado el nombre de usuario '%s' \n", username);
            fclose(fp);
            pthread_mutex_unlock(&usuarios);
            return 0;
        } else {
            perror("s > Error opening file");
            return 2;
        }
    }
    //Si hay un error al abrir el archivo devuelve 2
    else{
        return 2;
    }
   
}
int unregister(char *buffer, char *op) {
    
    // Si comienza con "UNREGISTER", el resto del buffer es el nombre de usuario
    char *fecha = strtok(buffer, " ");
    char *hora = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    printf("UNREGISTER FROM %s\n", username);

    int result_rpc = llamar_rpc(fecha, hora, username, op);
    if(result_rpc != 0){
        printf("s > Error al llamar a la función RPC\n");
    }

    // Abrir el archivo de texto en modo lectura y escritura
    pthread_mutex_lock(&usuarios);
    FILE *fp = fopen("usuarios.txt", "r+");
    if (fp != NULL) {
        // Crear un archivo temporal
        FILE *temp = tmpfile();
        if (temp == NULL) {
            fclose(fp);
            pthread_mutex_unlock(&usuarios);
            printf("s > Error creating temporary file");
            return 2;
        }

        // Variable para indicar si se encontró el usuario
        int username_found = 0;

        // Leer línea por línea del archivo original
        char line[MAXSIZE];
        while (fgets(line, sizeof(line), fp) != NULL) {
            // Eliminar el salto de línea si existe
            line[strcspn(line, "\n")] = '\0';
            // Comparar la línea con el usuario
            if (strcmp(line, username) == 0) {
                // Si se encuentra el usuario, establecer la variable en 1
                username_found = 1;
            } else {
                // si la línea no era el usuario, se escribe en el archivo temporal
                fprintf(temp, "%s\n", line);
            }
        }
        // Si se encontró el usuario, se sobrescribe el archivo original con el archivo temporal

        if (username_found) {
            rewind(temp); // Colocar el puntero de archivo al inicio del archivo temporal

            // Sobrescribir el archivo original con el contenido del archivo temporal
            freopen("usuarios.txt", "w", fp);
            while (fgets(line, sizeof(line), temp) != NULL) {
                fputs(line, fp);
            }
            fclose(fp);
            pthread_mutex_unlock(&usuarios);
            fclose(temp);
            
        } else {
            printf("s > Nombre de usuario '%s' no encontrado en el registro\n", username);
            fclose(fp);
            pthread_mutex_unlock(&usuarios);
            fclose(temp);
            return 1;
        }
    } else {
        printf("s > Error opening file");
        return 2;
    }
    //Se borran de conectados si estaba
    pthread_mutex_lock(&conectados);
    FILE *fb = fopen("conectados.txt", "a+");
    if (fb != NULL) {
        //Se crea un archivo temporal
        FILE *temp = tmpfile();
        if (temp == NULL) {
            fclose(fb);
            pthread_mutex_unlock(&conectados);
            printf("s > Error borrando la conexión\n");
            return 2;
        }
        // variable para indicar si se el usuario tenía algún archivo publicado
        int usuario_encontrado = 0;
        // Leer línea por línea del archivo original
        char line[MAXSIZE];
        char original_line[MAXSIZE];
        while (fgets(line, sizeof(line), fb) != NULL) {
            strcpy(original_line, line);
            line[strcspn(line, "\n")] = '\0'; // Eliminar el carácter de nueva línea
            
            //Se obtiene la primera palabra, que es el usuario
            char *token1 = strtok(line, " ");
        
            //Se borra un archivo si es del mismo usuario
            if (token1 != NULL && strcmp(token1, username)==0) {
                usuario_encontrado= 1;
            } else {
                // archivo no encontrado, escribir la línea en el archivo temporal
                fprintf(temp, "%s", original_line);
            }
        }

        if (usuario_encontrado) {
            rewind(temp); // Colocar el puntero de archivo al inicio del archivo temporal
            // Sobrescribir el archivo original con el contenido del archivo temporal
            freopen("conectados.txt", "w", fb);
            while (fgets(line, sizeof(line), temp) != NULL) {
                fputs(line, fb);
            }
            fclose(fb);
            pthread_mutex_unlock(&conectados);
            fclose(temp);
        } else {
            fclose(fb);
            pthread_mutex_unlock(&conectados);
            fclose(temp);
        }
    } else {
        perror("Error opening file");
        return 2;
    }

    //Se borran sus publicaciones
    pthread_mutex_lock(&publicaciones);
    FILE *fpb = fopen("publicaciones.txt", "a+");
    if (fpb != NULL) {
        //Se crea un archivo temporal
        FILE *temp = tmpfile();
        if (temp == NULL) {
            fclose(fpb);
            pthread_mutex_unlock(&publicaciones);
            printf("s > Error borrando la publicación\n");
            return 2;
        }
        // variable para indicar si se el usuario tenía algún archivo publicado
        int archivo_encontrado = 0;
        // Leer línea por línea del archivo original
        char line[MAXSIZE];
        char original_line[MAXSIZE];
        while (fgets(line, sizeof(line), fpb) != NULL) {
            strcpy(original_line, line);
            line[strcspn(line, "\n")] = '\0'; // Eliminar el carácter de nueva línea
            
            //Se obtiene la primera palabra, que es el usuario
            char *token1 = strtok(line, " ");
        
            //Se borra un archivo si es del mismo usuario
            if (token1 != NULL && strcmp(token1, username)==0) {
                archivo_encontrado= 1;
            } else {
                // archivo no encontrado, escribir la línea en el archivo temporal
                fprintf(temp, "%s", original_line);
            }
        }

        if (archivo_encontrado) {
            rewind(temp); // Colocar el puntero de archivo al inicio del archivo temporal
            // Sobrescribir el archivo original con el contenido del archivo temporal
            freopen("publicaciones.txt", "w", fpb);
            while (fgets(line, sizeof(line), temp) != NULL) {
                fputs(line, fpb);
            }
            printf("s > El nombre de usuario '%s' ha sido eliminado del registro\n", username);
            fclose(fpb);
            pthread_mutex_unlock(&publicaciones);
            fclose(temp);
            return 0;
        } else {
            printf("s > El nombre de usuario '%s' ha sido eliminado del registro\n", username);
            fclose(fpb);
            pthread_mutex_unlock(&publicaciones);
            fclose(temp);
            return 0;
        }
    } else {
        perror("Error opening file");
        return 2;
    }

}
int publicar(char *buffer, char *op) {
    char *fecha = strtok(buffer, " ");
    char *hora = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *namefile = strtok(NULL, " ");
    char *description = strtok(NULL, "\0");
   
    printf("PUBLISH FROM %s\n", username);

    char *op_namefile = malloc(strlen(op) + strlen(namefile) + 2);
    strcpy(op_namefile, op);
    strcat(op_namefile, "\t");
    strcat(op_namefile, namefile);
    int result_rpc = llamar_rpc(fecha, hora, username, op_namefile);
    if(result_rpc != 0){
        printf("s > Error al llamar a la función RPC\n");
    }
    free(op_namefile);
    //Se comprueba si el usuario está registrado
    int result = buscar_registrado(username);
    if(result ==1){
        //Si no está registrado devuelve 1
        printf("s > El nombre de usuario '%s' no está registrado\n", username);
        return 1;
    }
    else if (result == 2){
        //Error al abrir el archivo
        return 4;
    }
    else{
        result = buscar_conectado(username);
        if(result == 0){
            pthread_mutex_lock(&publicaciones);
            FILE *fpb = fopen("publicaciones.txt", "a+");
            if (fpb != NULL) {
                // Leer línea por línea del archivo original
                char line[MAXSIZE];
                int archivo_publicado = 0;
                while (fgets(line, sizeof(line), fpb) != NULL) {
                    line[strcspn(line, "\n")] = '\0'; // Eliminar el carácter de nueva línea
                    
                    char *token = strtok(line, " ");
                    // Obtener el segundo token de la línea
                    token = strtok(NULL, " ");
                   
                    if (token != NULL && strcmp(token, namefile) == 0) {
                        archivo_publicado = 1;
                        break;
                    }
                }
                if (archivo_publicado) {
                    printf("s > El nombre del fichero '%s' ya está publicado\n", namefile);
                    fclose(fpb);
                    pthread_mutex_unlock(&publicaciones);
                    return 3;
                } 
                else{
                    fprintf(fpb, "%s", username);
                    fprintf(fpb, "%s", " ");
                    fprintf(fpb, "%s", namefile);
                    fprintf(fpb, "%s", " ");
                    fprintf(fpb, "%s\n", description);
                    printf("s > Publicación '%s' agregada al archivo publicaciones.txt\n", namefile);
                    fclose(fpb);
                    pthread_mutex_unlock(&publicaciones);
                    return 0;
                }
            } else {
                perror("s > Error opening file");
                return 4;
            }
        }
        else if(result == 1){
            printf("s > El nombre de usuario '%s' no está conectado\n", username);
            return 2;
        }
        else{
            //Error al abrir el fichero
            return 4;
        }
    }
    return 0;
}

int conectar(char *buffer, char *op){
    char *fecha = strtok(buffer, " ");
    char *hora = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *ip = strtok(NULL, " ");
    char *port = strtok(NULL, "\0");
    printf("CONNECT FROM %s\n", username);
    
    int result_rpc = llamar_rpc(fecha, hora, username, op);
    if(result_rpc != 0){
        printf("s > Error al llamar a la función RPC\n");
    }
    //Se comprueba si el usuario está registrado
    int result = buscar_registrado(username);

    if(result ==1){
        //Si no está registrado devuelve 1
        printf("s > El nombre de usuario '%s' no está registrado\n", username);
        return 1;
    }
    else if (result == 2){
        //Error al abrir el archivo
        return 3;
    }
    else{
        //Se comprueba si el usuario ya está conectado
        result = buscar_conectado(username);
        if(result==0){
            //Si ya está conectado devuelve 2
            printf("s > El nombre de usuario '%s' ya está conectado\n", username);
            return 2;
        }
        else if(result == 1){
            pthread_mutex_lock(&conectados);
            FILE *fc = fopen("conectados.txt", "a+");
            if(fc != NULL){
                //Escribir en el archivo conectados.txt
                fprintf(fc, "%s", username);
                fprintf(fc, "%s", " ");
                fprintf(fc, "%s", ip);
                fprintf(fc, "%s", " ");
                fprintf(fc, "%s\n", port);
                printf("s > Nombre de usuario '%s' conectado\n", username);
                fclose(fc);
                pthread_mutex_unlock(&conectados);
                return 0;
            }
            else{
                //Error al abrir el fichero
                return 3;
            }
        }
        else{
            //Error al abrir el fichero
            return 3;
        }
    }
}

int borrar(char *buffer, char *op){
    char *fecha = strtok(buffer, " ");
    char *hora = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *namefile = strtok(NULL, "\0");
   
    printf("DELETE FROM %s\n", username);
    
    char *op_namefile = malloc(strlen(op) + strlen(namefile) + 2);
    strcpy(op_namefile, op);
    strcat(op_namefile, "\t");
    strcat(op_namefile, namefile);
    int result_rpc = llamar_rpc(fecha, hora, username, op_namefile);
    if(result_rpc != 0){
        printf("s > Error al llamar a la función RPC\n");
    }
    free(op_namefile);
    int result = buscar_registrado(username);
    //Se comprueba si el usuario está registrado
    if(result ==1){
        //Si no está registrado devuelve 1
        printf("s > El nombre de usuario '%s' no está registrado\n", username);
        return 1;
    }
    else if (result == 2){
        //Error al abrir el archivo
        return 4;
    }
    else{
        //Se comprueba si el usuario ya está conectado
        result = buscar_conectado(username);
        if(result==0){
            pthread_mutex_lock(&publicaciones);
            //Si ya está conectado se busca la publicación
            FILE *fpb = fopen("publicaciones.txt", "a+");
            if (fpb != NULL) {
                //Se crea un archivo temporal
                FILE *temp = tmpfile();
                if (temp == NULL) {
                    fclose(fpb);
                    pthread_mutex_unlock(&publicaciones);
                    printf("s > Error borrando la publicación\n");
                    return 4;
                }
                // variable para indicar si se encontró el archivo
                int archivo_encontrado = 0;
                // Leer línea por línea del archivo original
                char line[MAXSIZE];
                char original_line[MAXSIZE];
                while (fgets(line, sizeof(line), fpb) != NULL) {
                    strcpy(original_line, line);
                    line[strcspn(line, "\n")] = '\0'; // Eliminar el carácter de nueva línea
                    
                    //Se obtiene la primera palabra, que es el usuario
                    char *token1 = strtok(line, " ");
                    
                    // Obtener el segundo token de la línea que es el nombre del archivo
                    char *token2 = strtok(NULL, " ");
                    //Solo se puede borrar un archivo si es del mismo usuario
                    if (token2 != NULL && strcmp(token2, namefile) == 0 && token1 != NULL && strcmp(token1, username)==0) {
                        archivo_encontrado= 1;
                    } else {
                        // archivo no encontrado, escribir la línea en el archivo temporal
                        fprintf(temp, "%s", original_line);
                    }
                }

                if (archivo_encontrado) {
                    rewind(temp); // Colocar el puntero de archivo al inicio del archivo temporal
                    // Sobrescribir el archivo original con el contenido del archivo temporal
                    freopen("publicaciones.txt", "w", fpb);
                    while (fgets(line, sizeof(line), temp) != NULL) {
                        fputs(line, fpb);
                    }
                    printf("s > El fichero '%s' ha sido eliminado del archivo publicaciones.txt\n", namefile);
                    fclose(fpb);
                    fclose(temp);
                    pthread_mutex_unlock(&publicaciones);
                    return 0;
                } else {
                    printf("s > Fichero '%s' no encontrado en el archivo publicaciones.txt\n", namefile);
                    fclose(fpb);
                    fclose(temp);
                    pthread_mutex_unlock(&publicaciones);
                    return 3;
                }
            } else {
                perror("Error opening file");
                return 4;
            }
          
        }
        else if(result == 1){
            printf("s > El nombre de usuario '%s' no está conectado\n", username);
            return 2;
        }
        else{
            //Error al abrir el fichero
            return 4;
        }
    }    
    return 0;
}

void list_users(char *buffer, int sc, char *op){
    // Si comienza con "LIST_USERS", el resto del buffer es el nombre de usuario
    int result;
    char *fecha = strtok(buffer, " ");
    char *hora = strtok(NULL, " ");
    char *username = strtok(NULL, "\0");
    
    printf("LIST_USERS FROM %s\n", username);
    int result_rpc = llamar_rpc(fecha, hora, username, op);
    if(result_rpc != 0){
        printf("s > Error al llamar a la función RPC\n");
    }

    result = buscar_registrado(username);
    //Se comprueba si el usuario está registrado
    if(result ==1){
        //Si no está registrado devuelve 1
        printf("s > El nombre de usuario '%s' no está registrado\n", username);
    }
    else if (result == 2){
        //Error al abrir el archivo
        result = 3;
    }
    else{
        //Se comprueba si el usuario ya está conectado
        result = buscar_conectado(username);
        if(result == 1){
            printf("s > El nombre de usuario '%s' no está conectado\n", username);
            result = 2;
        }
        else if (result == 2){
            //Error al abrir el fichero
            result = 3;
        }
    }
    //Enviar el resultado al cliente
    result = htonl(result);
    send(sc, &result, sizeof(result), 0);
    //Si el resultado es 0 se envía la lista de usuarios
    if(result == 0){
        pthread_mutex_lock(&conectados);
        FILE *fc2 = fopen("conectados.txt", "r");
        // Contar la cantidad de usuarios
        int num_users = 0;
        char line[MAXSIZE];
        while (fgets(line, sizeof(line), fc2) != NULL) {
            num_users++;
        }
        fclose(fc2);
        
        num_users = htonl(num_users);
        // Enviar la cantidad de usuarios al cliente
        send(sc, &num_users, sizeof(num_users), 0);

        FILE *fc3 = fopen("conectados.txt", "r");
        // Enviar la información de cada usuario al cliente
        while (fgets(line, sizeof(line), fc3) != NULL) {
            char *username;
            char *ip;
            char *port;

            line[strcspn(line, "\n")] = '\0';

            // Dividir la línea en tokens utilizando el espacio como delimitador
            username = strtok(line, " ");
            ip = strtok(NULL, " ");
            port = strtok(NULL, " ");
            // Obtener la longitud de cada cadena
            int username_len = strlen(username);
            int ip_len = strlen(ip);
            int port_len = strlen(port);

            // Enviar las cadenas reales al cliente, agregando un delimitador al final de cada cadena
            send(sc, username, username_len, 0);
            send(sc, "\0", 1, 0); // Delimitador
            send(sc, ip, ip_len, 0);
            send(sc, "\0", 1, 0); // Delimitador
            send(sc, port, port_len, 0);
            send(sc, "\0", 1, 0); // Delimitador

        }
        fclose(fc3);
        pthread_mutex_unlock(&conectados);
    }
    
}
void list_content(char * buffer, int sc, char *op){
    char *fecha = strtok(buffer, " ");
    char *hora = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *user_remoto = strtok(NULL, "\0");
    int result;

    printf("LIST_CONTENT FROM %s\n", username);

    int result_rpc = llamar_rpc(fecha, hora, username, op);
    if(result_rpc != 0){
        printf("s > Error al llamar a la función RPC\n");
    }

    result = buscar_registrado(username);
    if(result ==1){
        //Si no está registrado devuelve 1
        printf("s > El nombre de usuario '%s' no está registrado\n", username);
        result = 1;
    }
    else if (result == 2){
        //Error al abrir el archivo
        result = 4;
    }
    else{
        result = buscar_registrado(user_remoto);
        if(result == 1){
            //Si no está registrado devuelve 3
            printf("s > El nombre de usuario '%s' no está registrado\n", user_remoto);
            result = 3;
        }
        else if (result == 2){
            //Error al abrir el archivo
            result = 4;
        }
        else{
            result = buscar_conectado(username);
            if(result == 1){
                printf("s > El nombre de usuario '%s' no está conectado\n", username);
                result = 2;
            }
            else if (result == 2){
                //Error al abrir el fichero
                result = 4;
            }
        }
    }
    //Enviar el resultado al cliente
    result = htonl(result);
    send(sc, &result, sizeof(result), 0);

    //Si el resultado es 0 se envía la lista de contenidos
    if(result == 0){
        pthread_mutex_lock(&publicaciones);
        FILE *fc2 = fopen("publicaciones.txt", "r");
        // Contar la cantidad de usuarios
        int num_files= 0;
        char line[MAXSIZE];
        //Cuenta el número de archivos publicados por el usuario remoto
        while (fgets(line, sizeof(line), fc2) != NULL) {
            char *user;
            line[strcspn(line, "\n")] = '\0';
            user = strtok(line, " ");
            if(user != NULL) {
                if(strcmp(user, user_remoto) == 0){
                    num_files++;
                }
            }
        }
        fclose(fc2);
        num_files = htonl(num_files);
        // Enviar la cantidad de archivos al cliente
        send(sc, &num_files, sizeof(num_files), 0);

        FILE *fc3 = fopen("publicaciones.txt", "r");
        // Enviar la información de cada archivo al cliente
        while (fgets(line, sizeof(line), fc3) != NULL) {
            char *username;
            char *namefile;
            char *des;

            line[strcspn(line, "\n")] = '\0';

            // Dividir la línea en tokens utilizando el espacio como delimitador
            username = strtok(line, " ");
            namefile = strtok(NULL, " ");
            des = strtok(NULL, " ");
            
            if(strcmp(username, user_remoto) == 0){
                // Obtener la longitud de cada cadena
                int namefile_len = strlen(namefile);
                int des_len = strlen(des);

                // Enviar las cadenas reales al cliente, agregando un delimitador al final de cada cadena
                send(sc, namefile, namefile_len, 0);
                send(sc, "\0", 1, 0); // Delimitador
                send(sc, des, des_len, 0);
                send(sc, "\0", 1, 0); // Delimitador

                }
            }
            pthread_mutex_unlock(&publicaciones);
            fclose(fc3);
        } 
}

int desconectar(char *buffer, char *op){
    // Si comienza con "DISCONNECT", el resto del buffer es el nombre de usuario
    char *fecha = strtok(buffer, " ");
    char *hora = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    int result = buscar_registrado(username);
    
    printf("DISCONNECT FROM %s\n", username);

    int result_rpc = llamar_rpc(fecha, hora, username, op);
    if(result_rpc != 0){
        printf("s > Error al llamar a la función RPC\n");
    }

    if(result == 0){
        // Abrir el archivo de texto en modo lectura y escritura
        pthread_mutex_lock(&conectados);
        FILE *fc = fopen("conectados.txt", "a+");
        if (fc != NULL) {
            // Crear un archivo temporal
            FILE *temp = tmpfile();
            if (temp == NULL) {
                fclose(fc);
                pthread_mutex_unlock(&conectados);
                printf("s > Error creating temporary file");
                return 4;
            }
            // variable para indicar si se encontró el usuario
            int usuario_encontrado = 0;
            // Leer línea por línea del archivo original
            char line[MAXSIZE];
            char original_line[MAXSIZE];
            while (fgets(line, sizeof(line), fc) != NULL) {
                strcpy(original_line, line);
                line[strcspn(line, "\n")] = '\0'; // Eliminar el carácter de nueva línea

                char *token = strtok(line, " ");           
                if (token != NULL && strcmp(token, username) == 0) {
                    usuario_encontrado = 1;
                } else {
                    // Usuario no encontrado, escribir la línea en el archivo temporal
                    fprintf(temp, "%s", original_line);
                }
            }
        //Si se encuentra el usuario, se sobrescribe el archivo original con el archivo temporal
        if (usuario_encontrado) {
                rewind(temp); // Colocar el puntero de archivo al inicio del archivo temporal
                // Sobrescribir el archivo original con el contenido del archivo temporal
                freopen("conectados.txt", "w", fc);
                while (fgets(line, sizeof(line), temp) != NULL) {
                    fputs(line, fc);
                }
                printf("s > El usuario'%s' ha sido desconectado\n", username);
                fclose(fc);
                fclose(temp);
                pthread_mutex_unlock(&conectados);
                return 0;
            } else {
                printf("s > Usuario '%s' no conectado\n", username);
                fclose(fc);
                fclose(temp);
                pthread_mutex_unlock(&conectados);
                return 2;
            }
        }else {
            perror("s > Error opening file");
            return 3;
        }
       
    }
    else if(result == 1){
        printf("s > El nombre de usuario '%s' no está registrado\n", username);
        return 1;
    }
    else{
        return 3;
    }    
}

int get_user_port_ip(char *user, int sc){
    pthread_mutex_lock(&conectados);
    FILE *fc = fopen("conectados.txt", "r");
    if (fc != NULL) {
        char line[MAXSIZE];
        // Enviar la información de cada usuario al cliente
        while (fgets(line, sizeof(line), fc) != NULL) {
            char *username;
            char *ip;
            char *port;

            line[strcspn(line, "\n")] = '\0';

            // Dividir la línea en tokens utilizando el espacio como delimitador
            username = strtok(line, " ");
            ip = strtok(NULL, " ");
            port = strtok(NULL, " ");
            if(strcmp(username, user) == 0){
                // Obtener la longitud de cada cadena
                int ip_len = strlen(ip);
                int port_len = strlen(port);

                // Enviar las cadenas reales al cliente, agregando un delimitador al final de cada cadena
                send(sc, ip, ip_len, 0);
                send(sc, "\0", 1, 0); // Delimitador
                send(sc, port, port_len, 0);
                send(sc, "\0", 1, 0); // Delimitador

            }
        }
        fclose(fc);
        pthread_mutex_unlock(&conectados);
    }else {
        perror("Error opening file");
        return 3;
    }
    return 0;

}
void get_file(char *buffer, int sc, char *op){
    char *fecha = strtok(buffer, " ");
    char *hora = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *user_remoto = strtok(NULL, "\0");
    int result;
    printf("GET_FILE FROM %s\n", username);
    
    //Se comprueba si los usuarios están registrados y conectados
    int result1 = buscar_registrado(username);
    int result2 = buscar_registrado(user_remoto);
    int result3 = buscar_conectado(username);  
    int result4 = buscar_conectado(user_remoto);
    if(result1 == 0 && result2 == 0 && result3 == 0 && result4 == 0){
        result = htonl(result);
        send(sc, &result, sizeof(result), 0);
        result = get_user_port_ip(user_remoto, sc);
        if(result == 0){
            printf("s > Se ha enviado la información del usuario remoto\n");
        }
    }
    //sino se envia un error
    else{
        result = htonl(2);
        send(sc, &result, sizeof(result), 0);
    }
}

void execute_command(int *s) {
    // Copiar el socket a una variable local
    int sc;
    pthread_mutex_lock(&mutex);
    sc = (* (int *)s);
    mensaje_no_copiado = 0;

	pthread_cond_signal(&cond);

	pthread_mutex_unlock(&mutex);

    // Recibir la operación desde el cliente
    char buffer[MAX_COMMAND_LENGTH];
    char received_char;
    int index = 0;
    int result;
    
    while (1) {
        //Se van recibiendo bytes desde el cliente y se guardan en recieved_char
        ssize_t bytes_received = recv(sc, &received_char, sizeof(received_char), 0);
        if (bytes_received <= 0) {
            printf("s > Error al recibir datos del cliente\n");
            close(sc);
            pthread_exit(NULL);
        }
        
        //Cuando se haya recibido el byte de terminación '\0' se termina el bucle
        if (received_char == '\0') {
            break;  // Final de la cadena.
        }
        //Se guarda el byte recibido en el buffer
        buffer[index++] = received_char;
    }
    //Se añade el terminador nulo al final del buffer
    buffer[index] = '\0'; 
    
    char *op = strtok(buffer, " ");
    char *resto = strtok(NULL, "\0"); // Obtener el resto de la cadena
    //Se comprueba la operación recibida y se realiza la función correspondiente

    if (strncmp(op, "REGISTER", strlen("REGISTER")) == 0) {
        result = registrar(resto, op);
    }
    else if (strncmp(op, "UNREGISTER", strlen("UNREGISTER")) == 0) {
        result = unregister(resto, op);
    }
    else if (strncmp(op, "CONNECT", strlen("CONNECT")) == 0) {
        result = conectar(resto, op);
    }
    else if (strncmp(op, "PUBLISH", strlen("PUBLISH")) == 0) {
        result = publicar(resto, op);
    }
    else if (strncmp(op, "DELETE", strlen("DELETE")) == 0) {
        result = borrar(resto, op);
    }
    else if (strncmp(op, "LIST_USERS", strlen("LIST_USERS")) == 0) {
        list_users(resto, sc, op);
    }
    else if (strncmp(op, "LIST_CONTENT", strlen("LIST_CONTENT")) == 0) {
        list_content(resto, sc, op);
    }
    else if (strncmp(op, "DISCONNECT", strlen("DISCONNECT")) == 0) {
        result = desconectar(resto, op);
    }
    else if (strncmp(op, "GET_FILE", strlen("GET_FILE")) == 0) {
        get_file(resto, sc, op);
    }
    printf("s > ");
    fflush(stdout); // Vaciar el búfer de salida

    //Si la operación no es LIST_USERS, LIST_CONTENT o GET_FILE se envía el resultado al cliente, en las otras funciones se envía en la función
    if(strncmp(buffer, "LIST_USERS", strlen("LIST_USERS")) != 0 && strncmp(buffer, "LIST_CONTENT", strlen("LIST_CONTENT")) != 0 && strncmp(buffer, "GET_FILE", strlen("GET_FILE")) != 0){
        result = htonl(result);
        send(sc, &result, sizeof(result), 0);
        // Después de enviar el resultado, envía el byte de terminación '\0'
        char termination_char = '\0';
        send(sc, &termination_char, sizeof(termination_char), 0);
    }
    close(sc);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    //Comprobación de argumentos
    if (argc != 3) {
        printf("FAILURE: %s -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "-p") != 0) {
        printf("FAILURE: %s -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //Variables de los hilos
    pthread_t hilo;
    pthread_attr_t t_attr;	

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&usuarios, NULL);
    pthread_mutex_init(&publicaciones, NULL);
    pthread_mutex_init(&conectados, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_attr_init(&t_attr);

    pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

    //Obtiene el puerto de los argumentos y se comprueba que sea un número válido
    char *endptr;
    long PORT = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || PORT < 0 || PORT > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    //Variables para la conexión
    signal(SIGINT, SIG_DFL);
    struct sockaddr_in server_addr, client_addr;
    socklen_t size;
    int sd, sc;
    int val;

    //Creación del socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error en el socket");
        return 0;
    }
    val = 1;

    //Configuración del socket
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(int));
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    //Asignación del puerto y direccion al socket
    err = bind(sd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
	if (err == -1) {
		printf("s > Error en bind\n");
		return -1;
	}

    


    //Se pone el servidor a escuchar
    err = listen(sd, SOMAXCONN);
	if (err == -1) {
		printf("s > Error en listen\n");
		return -1;
	}

    size = sizeof(client_addr);
    //Se imprime la dirección y puerto del servidor
    printf("s > init server %s:%hu\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
    
    printf("s > ");
    fflush(stdout); // Vaciar el búfer de salida

    err = obtenerVariablesEntorno();
    if (err == -1) {
        printf("s > Error en obtenerVariablesEntorno\n");
        return -1;
    }
    //Bucle para aceptar conexiones
    while(1) {
        //Se acepta la conexión
    	sc = accept(sd, (struct sockaddr *)&client_addr, (socklen_t *)&size);

		if (sc == -1) {
			printf("s > Error en accept\n");
			return -1;
		}        

        //Se crea un hilo para cada conexión
        if(pthread_create(&hilo, &t_attr, (void *)execute_command, (int *) &sc) == 0){
            pthread_mutex_lock(&mutex);
			while (mensaje_no_copiado)
				pthread_cond_wait(&cond, &mutex);
			mensaje_no_copiado = 1;
			pthread_mutex_unlock(&mutex);
        }
        else{
            perror("s > error creación de hilo");
            return -1;
        }
    }
    close(sd);
    
    return(0);
}
