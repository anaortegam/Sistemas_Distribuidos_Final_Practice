#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "mensajes.h"

static char *ip_tuplas = NULL;

int obtenerVariablesEntorno()
{
	//Obtenemos las variables de entorno y se guardan en las variables globales
    ip_tuplas = getenv("IP_TUPLAS");
    if (ip_tuplas == NULL){
        printf("Variable IP_TUPLAS no definida\n");
        return -1;
    }
    
    return 0;
}

char * obtenerIP() {
	//Devuelve la IP
    return ip_tuplas;
}