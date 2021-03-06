/*
 * esi.h
 *
 *  Created on: 21 abr. 2018
 *      Author: utnso
 */

#ifndef ESI_H_
#define ESI_H_

#include <shared-library.h>

//Variables locales

uint32_t this_id;

char* IP_COORDINADOR;
char* PUERTO_COORDINADOR;
char* IP_PLANIFICADOR;
char* PUERTO_PLANIFICADOR;

int socket_coordinador;
int socket_planificador;

t_parsed_list parsed_ops;

t_list* lineas_parseadas;

aviso_con_ID aviso_abort = {
		.aviso = -10
};

aviso_con_ID aviso_fin = {
		.aviso = 0
};

aviso_con_ID aviso_ready = {
		.aviso = 1
};

aviso_con_ID aviso_bloqueo = {
		.aviso = 5
};

aviso_con_ID aviso_ejecute = {
		.aviso = 10
};

aviso_con_ID aviso_get = {
		.aviso = 11
};

aviso_con_ID aviso_set = {
		.aviso = 12
};

aviso_con_ID aviso_store = {
		.aviso = 13
};

aviso_con_ID aviso_ID = {
		.aviso = 2
};

//Funciones

void iniciar(char** argv);
	/*
	 * Descripción: inicia el log y llama a parsear() para guardar las lineas en la lista de parseadas.
	 * Argumentos:
	 * 		char** argv: vector de char*, que se usa con levantar_archivo() en la posición 1.
	 */

void cerrar(void);
	/*
	 * Descripción: cierra las conexiones con los servidores y cierra los sockets.
	 * Argumentos:
	 * 		void
	 */

void ejecutar_sentencias(void);
	/*
	 * Descripción: recibe el ID local y luego ejecuta las instrucciones parseadas a medida que se le indica.
	 * Argumentos:
	 * 		void
	 */

void establecer_conexiones(void);
	/*
	 * Descripción: establece las conexiones iniciales con el coordinador y el planificador.
	 * Argumentos:
	 * 		void
	 */

void fill_ID(uint32_t id);
	/*
	 * Descripción: rellena el ID en los avisos globales.
	 * Argumentos:
	 * 		uint32_t id: el ID con el que llenar.
	 */

t_esi_operacion parsear(char* line);
	/*
	 * Descripción: parsea una línea y devuelve el resultado en un t_esi_operacion.
	 * Argumentos:
	 * 		char* line: linea a parsear.
	 */

uint32_t recibir_ID(int server_socket);
	/*
	 * Descripción: recibe un aviso del servidor y en caso de ser un id, lo almacena en this_id.
	 * Argumentos:
	 * 		int server_socket: socket del cual se hace el recv().
	 */

char* siguiente_linea(FILE* fp);
	/*
	 * Descripción: devuelve la siguiente línea de un archivo.
	 * Argumentos:
	 * 		FILE* fp: archivo del cual se quiere la línea.
	 */

void error_de_archivo(char* archivo, int exit_value);
	/*
	 * Descripción: informa si hubo un error en la apertura del archivo y sale con error.
	 * Argumentos:
	 * 		char* archivo: el archivo a abrir
	 * 		int exit_value: valor de salida de error (2).
	 */

FILE* levantar_archivo(char* archivo);
	/*
	 * Descripción: devuelve un archivo abierto a partir de su nombre.
	 * Argumentos:
	 * 		char* archivo: el archivo a abrir.
	 */

void esperar_ejecucion(int socket_coordinador, int socket_planificador);
	/*
	 * Descripción: espera a recibir una orden de ejecución por parte del planificador y llama a
	 * 		solicitar_permiso() para ejecutar. En caso positivo, llama a ejecutar().
	 * Argumentos:
	 * 		int socket_coordinador: socket del coordinador.
	 * 		int socket_planificador: socket del planificador.
	 */

void ejecutar(int socket_coordinador, int socket_planificador);
	/*
	 * Descripción: ejecuta la siguiente instrucción como indique la lista de líneas parseadas,
	 * 		e informa el resultado al coordinador.
	 * Arumgnetos:
	 * 		int socket_coordinador: socket del coordinador.
	 * 		int socket_planificador: socket del planificador.
	 */

void cargar_configuracion(char** argv);
	/*
	 * Descripción: carga el archivo de configuración en variables globales.
	 * Argumentos:
	 * 		char** argv: array que contiene el archivo de configuración.
	 */

uint32_t get(t_esi_operacion parsed, int socket_coordinador);
	/*
	 * Descripción: hace la operación de get sobre la clave del parseo y le informa al coordinador.
	 * Argumentos:
	 * 		t_esi_operacion parsed: la sentencia parseada.
	 * 		int socket_coordinador: socket del coordinador.
	 */

uint32_t set(t_esi_operacion parsed, int socket_coordinador);
	/*
	 * Descripción: hace la operación de set sobre la clave del parseo y le informa al coordinador.
	 * Argumentos:
	 * 		t_esi_operacion parsed: la sentencia parseada.
	 * 		int socket_coordinador: socket del coordinador.
	 */

uint32_t store(t_esi_operacion parsed, int socket_coordinador);
	/*
	 * Descripción: hace la operación de store sobre la clave del parseo y le informa al coordinador.
	 * Argumentos:
	 * 		t_esi_operacion parsed: la sentencia parseada.
	 * 		int socket_coordinador: socket del coordinador.
	 */

void clear(t_parsed_list* lista);
	/*
	 * Descripción: libera la memoria ocupada por la lista.
	 * Argumentos:
	 * 		t_parsed_list* lista: puntero a la lista.
	 */

void avisar_ID(int sockfd);
	/*
	 * Descripción: envía el ID del ESI a un proceso y espera un OK del servidor. Llama a enviar_aviso()
	 * 		y recibir_packed(). Si el retorno del proceso es distinto de 42, llama a salir_con_error().
	 * Argumentos:
	 * 		int sockfd: socket por el cual enviar el mensaje.
	 */

op_response recibir_respuesta(int sockfd);
	/*
	 * Descripción: devuelve un op_response de acuerdo a la respuesta comunicada por el sockfd.
	 * Argumentos:
	 * 		int sockfd
	 */

bool verificarLongitud(char* string);
	/*
	 * Descripción: devuelve si la longitud (más 1) de string es menor o igual a 40.
	 * Argumentos:
	 * 		char* string
	 */

void startSigHandlers(void);
	/*
	 * Descripción:
	 * Argumentos:
	 * 		void
	 */

void sigHandler_sigint(int signo);
	/*
	 * Descripción:
	 * Argumentos:
	 * 		int signo
	 */

#endif /* ESI_H_ */
