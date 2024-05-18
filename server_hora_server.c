#include "server_hora.h"

bool_t
imprimir_1_svc(struct server_estructura arg1, int *result,  struct svc_req *rqstp)
{
	bool_t retval;
	printf("%s \t %s\t %s \t %s\n", arg1.user, arg1.op, arg1.fecha, arg1.hora);
	fflush(stdout);
	*result = 0;
	retval = TRUE;
	return retval;
}

int
server_hora_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
	xdr_free (xdr_result, result);
	return 1;
}
