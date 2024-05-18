struct server_estructura {
    char user[256];
    char op[512];
    char fecha[256];
    char hora[256];
};

program SERVER_HORA {
    version SERVER_HORA_V1 {
        int IMPRIMIR(struct server_estructura)= 1;
    } = 1; 
} = 99; 
