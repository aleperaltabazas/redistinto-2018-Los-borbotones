/*
 * shared-library.c
 *
 *  Created on: 21 abr. 2018
 *      Author: utnso
 */

#include "shared-library.h"

void serializar_pedido(package_pedido pedido, char** message) {
	memcpy(*message, &(pedido.pedido), sizeof(pedido.pedido));
}

void deserializar_pedido(package_pedido *pedido, char** package) {
	memcpy(&pedido->pedido, *package, sizeof(pedido->pedido));
}

void avisar_cierre(int server_socket) {
	int status = 1;
	package_pedido pedido_de_fin = { .pedido = 0 };

	int packageSize = sizeof(pedido_de_fin.pedido);
	char *message = malloc(packageSize);

	serializar_pedido(pedido_de_fin, &message);

	loggear("Enviando aviso de fin.");

	while (status) {
		int envio = send(server_socket, message, packageSize, 0);

		status = 0;

		if(envio < 0){
			loggear("Fallo el envio. Intentando de nuevo en 5.");
			status = 1;

			sleep(5);

			//HORRIBLE pero no se me ocurre mucho mas de como hacerlo
		}
	}

	loggear("Aviso exitoso.");
}

int levantar_servidor(char* puerto) {
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;		//Le indicamos localhost
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(NULL, puerto, &hints, &server_info);

	int listening_socket = socket(server_info->ai_family,
			server_info->ai_socktype, server_info->ai_protocol);

	bind(listening_socket, server_info->ai_addr, server_info->ai_addrlen);
	freeaddrinfo(server_info);

	return listening_socket;
}

int conectar_a(char *ip, char *puerto, char* mensaje) {
	struct addrinfo hints;
	struct addrinfo *serverInfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// IPv4 o IPv6
	hints.ai_socktype = SOCK_STREAM;	// Protocolo TCP

	getaddrinfo(ip, puerto, &hints, &serverInfo);
	int server_socket;
	server_socket = socket(serverInfo->ai_family, serverInfo->ai_socktype,
			serverInfo->ai_protocol);

	int conexion = connect(server_socket, serverInfo->ai_addr,
			serverInfo->ai_addrlen);

	if (conexion < 0) {
		salir_con_error("Fallo la conexion con el servidor.", server_socket);
	}

	loggear("Conectó sin problemas");

	freeaddrinfo(serverInfo);

	char package[PACKAGE_SIZE];

	send(server_socket, mensaje, strlen(mensaje) + 1, 0);

	loggear("Mensaje enviado.");
	int res = recv(server_socket, (void*) package, PACKAGE_SIZE, 0);

	if (res != 0) {
		loggear(
				"Mensaje recibido desde el servidor. Identificando servidor...");
		chequear_servidor((char*) package, server_socket);

	} else {
		salir_con_error("Fallo el envio de mensaje de parte del servidor.",
				server_socket);
	}

	loggear("Cerrando conexion con servidor y terminando.");

	return server_socket;
}

void chequear_servidor(char* mensaje, int server_socket) {
	char mensajeCoordinador[] = "Coordinador: taringuero profesional.";
	char mensajePlanificador[] =
			"My name is Planificador.c and I'm the fastest planifier alive...";

	if (strcmp(mensaje, mensajeCoordinador) == 0) {
		loggear(mensajeCoordinador);
	} else if (strcmp(mensaje, mensajePlanificador) == 0) {
		loggear(mensajePlanificador);
	} else {
		salir_con_error("Servidor desconocido, cerrando conexion.",
				server_socket);
	}

	return;
}

void iniciar_log(char* nombre, char* mensajeInicial) {
	logger = log_create("ReDisTinto.log", nombre, true, LOG_LEVEL_TRACE);
	loggear(mensajeInicial);
}

void loggear(char* mensaje) {
	log_trace(logger, mensaje);
}

void salir_con_error(char* mensaje, int socket) {
	log_error(logger, mensaje);
	close(socket);

	exit_gracefully(1);
}

void exit_gracefully(int return_val) {
	//log_destroy(logger);
	exit(return_val);
}
