/*
 * ========================
 * ===== PLANIFICADOR =====
 * ========================
 */

#include "planificador.h"

int listening_socket;

int main(int argc, char** argv) {
	iniciar(argv);

	manejar_conexiones();
	cerrar();

	return EXIT_SUCCESS;
}

void manejar_conexiones(void) {
	listening_socket = levantar_servidor(PUERTO_PLANIFICADOR, 0);
	int socketCliente;

	while (seguir_ejecucion) {
		socketCliente = manejar_cliente(listening_socket, socketCliente,
				id_planificador);
	}

	loggear("Terminando proceso...");

	close(listening_socket);
	close(socketCliente);
}

void cerrar(void) {
	cerrar_ESIs();
	destruir_listas();
	destruir_semaforos();
	avisar_cierre(socket_coordinador, 0);

	close(socket_coordinador);
	close(listening_socket);
	exit(EXIT_SUCCESS);
}

void destruir_listas(void) {
	pthread_mutex_lock(&sem_ready_ESIs);
	destroyESIList(&ready_ESIs);
	pthread_mutex_unlock(&sem_ready_ESIs);

	pthread_mutex_lock(&sem_blocked_ESIs);
	destroyESIList(&ready_ESIs);
	pthread_mutex_unlock(&sem_blocked_ESIs);

	pthread_mutex_lock(&sem_finished_ESIs);
	destroyESIList(&ready_ESIs);
	pthread_mutex_unlock(&sem_finished_ESIs);
}

void destruir_semaforos(void) {
	pthread_mutex_destroy(&sem_ID);
	pthread_mutex_destroy(&sem_clock);
	pthread_mutex_destroy(&sem_ejecucion);
	pthread_mutex_destroy(&sem_ejecutando);
	pthread_mutex_destroy(&sem_ready_ESIs);
	pthread_mutex_destroy(&sem_ready_ESIs_size);
	pthread_mutex_destroy(&sem_blocked_ESIs);
	pthread_mutex_destroy(&sem_finished_ESIs);
}

void cerrar_ESIs(void) {
	cerrar_conexion_lista(ready_ESIs, &sem_ready_ESIs);
	cerrar_conexion_lista(blocked_ESIs, &sem_blocked_ESIs);
}

void cerrar_conexion_lista(t_esi_list lista, pthread_mutex_t* semaforo) {
	pthread_mutex_lock(semaforo);
	t_esi_node* puntero = lista.head;

	while (puntero != NULL) {
		aviso_con_ID aviso_terminacion = { .aviso = 99 };
		send_aviso_no_exit(aviso_terminacion, puntero->esi.socket);
		close(puntero->esi.socket);

		puntero = puntero->sgte;
	}
	pthread_mutex_unlock(semaforo);
}

void iniciar(char** argv) {
	iniciar_log("Planificador", "Nace el planificador...");

	cargar_configuracion(argv);
	iniciar_semaforos();
	iniciar_hilos();
	startSigHandlers();
}

void startSigHandlers(void) {
	signal(SIGINT, sigHandler_sigint);
}

void sigHandler_sigint(int signo) {
	log_warning(logger, "Tiraste un CTRL+C, macho, abortaste el proceso.");
	log_error(logger, strerror(errno));

	cerrar();
}

void iniciar_hilos(void) {
	pthread_attr_t console_attr;
	pthread_attr_init(&console_attr);

	pthread_attr_t coordi_attr;
	pthread_attr_init(&coordi_attr);

	pthread_create(&hiloDeConsola, &console_attr, consola, NULL);
	pthread_detach(hiloDeConsola);

	pthread_create(&hilo_coordinador, &coordi_attr, atender_coordinador, NULL);
}

void cargar_configuracion(char** argv) {

	executing_ESI = esi_vacio;

	ESI_id = 1;

	ESIs_size = 0;
	tiempo = 0;

	t_config* config = config_create(argv[1]);

	char* puerto_coordi = config_get_string_value(config, "PUERTO_COORDINADOR");
	PUERTO_COORDINADOR = transfer(puerto_coordi, strlen(puerto_coordi) + 1);
	log_info(logger, "Puerto Coordinador: %s", PUERTO_COORDINADOR);

	char* puerto_plani = config_get_string_value(config, "PUERTO_PLANIFICADOR");
	PUERTO_PLANIFICADOR = transfer(puerto_plani, strlen(puerto_plani) + 1);
	log_info(logger, "Puerto Planificador: %s", PUERTO_PLANIFICADOR);

	char* ip_coordi = config_get_string_value(config, "IP_COORDINADOR");
	IP_COORDINADOR = transfer(ip_coordi, strlen(ip_coordi) + 1);
	log_info(logger, "IP Coordinador: %s", IP_COORDINADOR);

	char* algoritmo = config_get_string_value(config,
			"ALGORITMO_PLANIFICACION");
	ALGORITMO_PLANIFICACION = dame_algoritmo(algoritmo);
	log_info(logger, "Algoritmo: %s", algoritmo);

	ESTIMACION_INICIAL = config_get_int_value(config, "ESTIMACION_INICIAL");
	log_info(logger, "Estimación inicial: %i", ESTIMACION_INICIAL);

	ALFA = (float) config_get_int_value(config, "ALFA");
	log_info(logger, "Alfa: %f", ALFA);

	CLAVES_BLOQUEADAS = config_get_array_value(config, "CLAVES_BLOQUEADAS");
	int i = 0;
	while (CLAVES_BLOQUEADAS[i] != NULL) {
		log_info(logger, "Clave bloqueada: %s", CLAVES_BLOQUEADAS[i]);
		i++;
	}

	loggear("Configuración cargada.");
	config_destroy(config);
}

void bloqueo_inicial(void) {
	int i = 0;

	while (CLAVES_BLOQUEADAS[i] != NULL) {
		log_trace(logger, "Enviando aviso de bloqueo de la clave %s.",
				CLAVES_BLOQUEADAS[i]);
		avisar_bloqueo(socket_coordinador, CLAVES_BLOQUEADAS[i]);

		log_info(logger, "La clave %s fue bloqueada con éxito.",
				CLAVES_BLOQUEADAS[i]);
		i++;
	}

}

algoritmo_planificacion dame_algoritmo(char* algoritmo_src) {
	algoritmo_planificacion algoritmo_ret;

	if (strcmp(algoritmo_src, "FIFO") == 0) {
		algoritmo_ret.tipo = FIFO;
		algoritmo_ret.desalojo = false;
	}

	else if (strcmp(algoritmo_src, "SJF-SD") == 0) {
		algoritmo_ret.tipo = SJF;
		algoritmo_ret.desalojo = false;
	}

	else if (strcmp(algoritmo_src, "SJF-CD") == 0) {
		algoritmo_ret.tipo = SRT;
		algoritmo_ret.desalojo = true;
	}

	else if (strcmp(algoritmo_src, "HRRN") == 0) {
		algoritmo_ret.tipo = HRRN;
		algoritmo_ret.desalojo = false;
	}

	return algoritmo_ret;
}

void* atender_coordinador(void* nada) {
	socket_coordinador = conectar_a(IP_COORDINADOR, PUERTO_COORDINADOR,
			id_planificador, 0);

	bloqueo_inicial();

	loggear("Hilo de coordinador terminado.");

	return NULL;
}

void iniciar_semaforos() {
	pthread_mutex_init(&sem_ID, NULL);
	pthread_mutex_init(&sem_clock, NULL);
	pthread_mutex_init(&sem_ejecucion, NULL);
	pthread_mutex_init(&sem_ejecutando, NULL);
	pthread_mutex_init(&sem_ready_ESIs, NULL);
	pthread_mutex_init(&sem_ready_ESIs_size, NULL);
	pthread_mutex_init(&sem_blocked_ESIs, NULL);
	pthread_mutex_init(&sem_finished_ESIs, NULL);
}

int manejar_cliente(int listening_socket, int socket_cliente,
		package_int server_packed) {

	log_info(logger, "Esperando cliente...");

	listen(listening_socket, BACKLOG);
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	socket_cliente = accept(listening_socket, (struct sockaddr *) &addr,
			&addrlen);

	log_info(logger, "Cliente conectado.");
	loggear("Esperando mensaje del cliente.");

	package_int cliente_packed = recibir_packed(socket_cliente);

	log_debug(debug_logger, "%i", cliente_packed.packed);

	loggear("Mensaje recibido exitosamente. Identificando cliente...");
	identificar_cliente(cliente_packed, socket_cliente);

	loggear("Enviando mensaje al cliente.");

	enviar_packed(server_packed, socket_cliente);

	loggear("Mensaje enviado. Handshake realizado.");

	return socket_cliente;
}

void identificar_cliente(package_int id, int socket_cliente) {
	pthread_t hilo_ESI;

	log_debug(debug_logger, "%i", id);

	if (id.packed == 2) {
		loggear(mensajeESI);
		pthread_create(&hilo_ESI, NULL, atender_ESI,
				(void *) (intptr_t) socket_cliente);
		loggear(mensajeESILista);
		pthread_detach(hilo_ESI);
	}

	else {

		salir_con_error("Cliente desconocido, cerrando conexion.",
				socket_cliente);
	}

	return;
}

void* atender_ESI(void* buffer) {
	int socket_ESI = (intptr_t) buffer;

	loggear("Hilo de ESI inicializado correctamente.");

	ESI esi = { .socket = socket_ESI };

	int this_id = asignar_ID(esi);
	esi.id = this_id;

	int status = 1;

	while (status) {
		status = recibir_mensaje(socket_ESI, this_id, esi);

		planificar();
	}

	planificar();

	log_trace(logger, "Hilo de ESI número %i terminado.", this_id);
	close(socket_ESI);

	return NULL;
}

int recibir_mensaje(int socket_cliente, int id, ESI esi) {
	aviso_con_ID aviso = recv_aviso_no_exit(socket_cliente);

	log_trace(logger, "Mensaje recibido del ESI numero: %i", id);

	if (aviso.aviso == ESI_FIN) {
		finishESI(esi);
		return 0;
	}

	else if (aviso.aviso == ESI_READY) {
		newESI(esi);
	}

	else if (aviso.aviso == ESI_BLOCK) {
		blockESI(esi);
	}

	else if (aviso.aviso == ESI_EXEC) {
		executeESI(esi);
	}

	else {
		abortESI(esi);
		return 0;
	}

	return 1;
}

void newESI(ESI esi) {
	pthread_mutex_lock(&sem_clock);
	esi.tiempo_arribo = tiempo;
	pthread_mutex_unlock(&sem_clock);

	esi.rafaga_real = 0;
	esi.rafaga_estimada = ESTIMACION_INICIAL;
	esi.rafaga_remanente = ESTIMACION_INICIAL;

	pthread_mutex_lock(&sem_ready_ESIs);
	pthread_mutex_lock(&sem_ready_ESIs_size);
	agregar_ESI(&ready_ESIs, esi);

	ESIs_size++;
	log_debug(logger, "ESIs_size: %i", ESIs_size);
	pthread_mutex_unlock(&sem_ready_ESIs_size);
	pthread_mutex_unlock(&sem_ready_ESIs);

	log_info(logger, "ESI número %i listo para ejecutar añadido a la cola.",
			esi.id);

	if (ALGORITMO_PLANIFICACION.desalojo) {
		desalojar();
	}
}

void blockESI(ESI esi) {
	pthread_mutex_lock(&sem_ready_ESIs);
	pthread_mutex_lock(&sem_ready_ESIs_size);

	aumentarRafaga(esi, &ready_ESIs);
	actualizarEstimacion(esi, &ready_ESIs);
	ESI aux = findByIDIn(esi.id, ready_ESIs);
	eliminar_ESI(&ready_ESIs, esi);

	ESIs_size--;
	pthread_mutex_unlock(&sem_ready_ESIs);
	pthread_mutex_unlock(&sem_ready_ESIs_size);

	pthread_mutex_lock(&sem_ejecutando);
	ejecutando = false;
	pthread_mutex_unlock(&sem_ejecutando);

	pthread_mutex_unlock(&sem_ejecucion);

	agregar_ESI(&blocked_ESIs, aux);

	log_info(logger, "ESI número %i fue bloqueado.", esi.id);

	if (executing_ESI.id == esi.id) {
		vaciar_ESI();
	}
}

void abortESI(ESI esi) {
	log_debug(debug_logger, "ESI ID: %i", esi.id);
	log_debug(debug_logger, "Executing ESI: %i", executing_ESI.id);
	log_warning(debug_logger, "El ESI %i se cayó o fue abortado.", esi.id);
	if (executing_ESI.id == esi.id) {
		pthread_mutex_lock(&sem_ready_ESIs);
		pthread_mutex_lock(&sem_ready_ESIs_size);
		if (esta(ready_ESIs, esi)) {
			eliminar_ESI(&ready_ESIs, esi);
			ESIs_size--;
			loggear("Me metí en el if de readyESIs");
		}

		pthread_mutex_unlock(&sem_ready_ESIs_size);
		pthread_mutex_unlock(&sem_ready_ESIs);

		vaciar_ESI();

		pthread_mutex_lock(&sem_ejecutando);
		ejecutando = false;
		pthread_mutex_unlock(&sem_ejecutando);

		pthread_mutex_unlock(&sem_ejecucion);

	}

	if (esta(blocked_ESIs, esi)) {
		eliminar_ESI(&blocked_ESIs, esi);
	}

}

void finishESI(ESI esi) {
	log_info(logger,
			"ESI número %i terminado. Moviendo a la cola de terminados y eliminando de la cola de listos.",
			esi.id);

	agregar_ESI(&finished_ESIs, esi);

	pthread_mutex_lock(&sem_ready_ESIs);
	pthread_mutex_lock(&sem_ready_ESIs_size);

	if (esta(ready_ESIs, esi)) {
		eliminar_ESI(&ready_ESIs, esi);
		ESIs_size--;
	}

	pthread_mutex_unlock(&sem_ready_ESIs_size);
	pthread_mutex_unlock(&sem_ready_ESIs);

	if (esta(blocked_ESIs, esi)) {
		eliminar_ESI(&blocked_ESIs, esi);
	}

	if (executing_ESI.id == esi.id) {
		vaciar_ESI();

		pthread_mutex_lock(&sem_ejecutando);
		ejecutando = false;
		pthread_mutex_unlock(&sem_ejecutando);

		pthread_mutex_unlock(&sem_ejecucion);

	}

	conseguir_desbloqueados();
	loggear("Agregado correctamente a la cola de terminados.");

}

void executeESI(ESI esi) {
	pthread_mutex_lock(&sem_ready_ESIs);
	aumentarRafaga(esi, &ready_ESIs);
	decrementarRemanente(esi, &ready_ESIs);
	pthread_mutex_unlock(&sem_ready_ESIs);

	pthread_mutex_lock(&sem_ejecutando);
	ejecutando = false;
	pthread_mutex_unlock(&sem_ejecutando);

	executing_ESI.rafaga_real++;

	log_info(logger, "ESI número %i ejecutó correctamente.", esi.id);

	conseguir_desbloqueados();

	pthread_mutex_unlock(&sem_ejecucion);
}

void conseguir_desbloqueados(void) {
	log_info(logger, "Preguntando al coordinador que ESI desbloquear.");
	aviso_con_ID aviso_desbloqueado = { .aviso = 15 };

	enviar_aviso(socket_coordinador, aviso_desbloqueado);

	while (1) {
		aviso_con_ID respuesta_desbloqueado = recibir_aviso(socket_coordinador);
		log_debug(debug_logger, "Aviso: %i", respuesta_desbloqueado.aviso);
		log_debug(debug_logger, "ID: %i", respuesta_desbloqueado.id);

		if (respuesta_desbloqueado.aviso == 0
				|| respuesta_desbloqueado.id == 0) {
			log_info(logger, "No hay más ESIs para desbloquear.");
			return;
		} else if (respuesta_desbloqueado.aviso == 15) {
			desbloquear_ESI(respuesta_desbloqueado.id);

			if (ALGORITMO_PLANIFICACION.desalojo) {
				desalojar();
			}
		} else {
			salir_con_error("Falló la recepción del desbloqueado.",
					socket_coordinador);
		}
	}

}

bool esta(t_esi_list lista, ESI esi) {
	t_esi_node* puntero = lista.head;

	while (puntero != NULL) {
		log_debug(debug_logger, "ESI: %i", esi.id);
		log_debug(debug_logger, "Puntero: %i", puntero->esi.id);
		if (puntero->esi.id == esi.id) {
			return true;
		}

		puntero = puntero->sgte;
	}

	return false;
}

void desalojar(void) {

	if (executing_ESI.id != esi_vacio.id) {

		pthread_mutex_lock(&sem_ready_ESIs);
		ESI esi = findByIDIn(executing_ESI.id, ready_ESIs);
		log_debug(debug_logger, "ESI: %i", esi.id);

		if (esi.id == ESI_error.id) {
			salir_con_error("El executing_ESI no se enconraba en la lista",
					socket_coordinador);
		}

		reencolar(esi);
		pthread_mutex_unlock(&sem_ready_ESIs);

		vaciar_ESI();

		log_info(logger, "ESI desalojado.");
	}
}

void reencolar(ESI esi) {
	ESI aux = findByIDIn(esi.id, ready_ESIs);
	eliminar_ESI(&ready_ESIs, esi);
	agregar_ESI(&ready_ESIs, aux);
}

void vaciar_ESI(void) {
	executing_ESI = esi_vacio;
}

bool no_hay_ESI() {
	return executing_ESI.id == esi_vacio.id;
}

void kill_ESI(ESI esi) {
	int socket_ESI = esi.socket;

	aviso_con_ID aborto = { .aviso = -1, .id = esi.id };
	enviar_aviso(socket_ESI, aborto);

	log_info(logger, "ESI número %i has fainted!", esi.id);

}

int asignar_ID(ESI esi) {
	int socket_ESI = esi.socket;

	aviso_con_ID aviso;
	aviso.aviso = 1;

	pthread_mutex_lock(&sem_ID);
	aviso.id = ESI_id;

	ESI_id++;
	pthread_mutex_unlock(&sem_ID);

	enviar_aviso(socket_ESI, aviso);

	log_debug(debug_logger, "Aviso: %i", aviso.aviso);
	log_debug(debug_logger, "ID: %i", aviso.id);

	return (int) aviso.id;

}

void planificar(void) {
	if (consola_planificacion && ESIs_size > 0 && !ejecutando) {
		pthread_mutex_lock(&sem_ejecucion);

		ESI next_esi = executing_ESI;

		if (no_hay_ESI()) {
			next_esi = dame_proximo_ESI();
			if (next_esi.id == ESI_error.id) {
				log_warning(logger, "No hay ESIs listos esperando.");
				log_debug(logger, "ESIs_size: %i", ESIs_size);
				pthread_mutex_unlock(&sem_ejecucion);
				return;
			}
			log_info(logger, "ESI número %i elegido.", next_esi.id);

		}

		ejecutar(next_esi);
	}
}

void ejecutar(ESI esi_a_ejecutar) {

	pthread_mutex_lock(&sem_ejecutando);
	ejecutando = true;
	pthread_mutex_unlock(&sem_ejecutando);

	int socket_ESI = esi_a_ejecutar.socket;
	executing_ESI = esi_a_ejecutar;

	loggear("Enviando orden de ejecucion.");
	aviso_con_ID orden_ejecucion = { .aviso = 2, .id = esi_a_ejecutar.id };

	send_aviso_no_exit(orden_ejecucion, socket_ESI);

	pthread_mutex_lock(&sem_clock);
	tiempo++;
	pthread_mutex_unlock(&sem_clock);

}

ESI dame_proximo_ESI() {
	ESI next_esi;
	switch (ALGORITMO_PLANIFICACION.tipo) {
	case FIFO:
		next_esi = headESIs(ready_ESIs);
		break;
	case SJF:
		next_esi = shortest_job(ready_ESIs);
		break;
	case SRT:
		next_esi = shortest_remaining(ready_ESIs);
		break;
	case HRRN:
		next_esi = highest_RR(ready_ESIs);
		break;
	default:
		log_error(logger, "FALLO EN EL ALGORITMO.");
		break;

	}

	next_esi.rafaga_real = 0;
	return next_esi;
}

ESI shortest_job(t_esi_list lista) {
	t_esi_node* puntero = lista.head;

	if (puntero == NULL) {
		return ESI_error;
	}

	ESI esi = headESIs(lista);

	while (puntero != NULL) {
		if (esMasCorto(esi, puntero->esi)) {
			esi = puntero->esi;
		}

		puntero = puntero->sgte;
	}

	return esi;
}

ESI shortest_remaining(t_esi_list lista) {
	t_esi_node* puntero = lista.head;

	ESI esi = headESIs(lista);

	while (puntero != NULL) {
		if (leQuedaMenosTiempo(esi, puntero->esi)) {
			esi = puntero->esi;
		}

		puntero = puntero->sgte;
	}

	return esi;
}

ESI highest_RR(t_esi_list lista) {
	t_esi_node* puntero = lista.head;

	ESI esi = headESIs(lista);

	while (puntero != NULL) {
		if (tieneMasRR(esi, puntero->esi)) {
			esi = puntero->esi;
		}

		puntero = puntero->sgte;
	}

	return esi;
}

bool esMasCorto(ESI primer_ESI, ESI segundo_ESI) {
	return estimated_time(segundo_ESI) < estimated_time(primer_ESI);
}

bool leQuedaMenosTiempo(ESI primer_ESI, ESI segundo_ESI) {
	return primer_ESI.rafaga_remanente > segundo_ESI.rafaga_remanente;
}

bool tieneMasRR(ESI primer_ESI, ESI segundo_ESI) {
	return response_ratio(primer_ESI) < response_ratio(segundo_ESI);
}

int wait_time(ESI esi) {
	return tiempo - esi.tiempo_arribo;
}

float estimated_time(ESI esi) {
	return tiempo_real(esi) + estimado(esi);
}

float tiempo_real(ESI esi) {
	return ((float) esi.rafaga_real) * (ALFA / 100);
}

float estimado(ESI esi) {
	return ((float) esi.rafaga_estimada) * (1 - (ALFA / 100));
}

float response_ratio(ESI esi) {
	return 1 + wait_time(esi) / esi.rafaga_estimada;
}

ESI findByIDIn(uint32_t id, t_esi_list lista) {
	t_esi_node* puntero = lista.head;

	while (puntero != NULL) {
		if (puntero->esi.id == id) {
			return puntero->esi;
		}

		puntero = puntero->sgte;
	}

	return ESI_error;
}

bool existe(uint32_t id) {
	ESI blocked = findByIDIn(id, blocked_ESIs);
	if (blocked.id != ESI_error.id) {
		return true;
	}

	ESI finished = findByIDIn(id, finished_ESIs);
	if (finished.id != ESI_error.id) {
		return true;
	}

	ESI ready = findByIDIn(id, ready_ESIs);
	if (ready.id != ESI_error.id) {
		return true;
	}

	return false;

}

void mostrar(t_esi_list lista, pthread_mutex_t* semaforo) {
	pthread_mutex_lock(semaforo);
	t_esi_node* puntero = lista.head;
	pthread_mutex_unlock(semaforo);

	if (puntero == NULL) {
		printf(" no hay ESIs en la cola");
	}

	int i = 0;
	while (puntero != NULL) {
		if (i > 0) {
			printf(",");
		}

		printf(" %i", puntero->esi.id);
		puntero = puntero->sgte;
		i++;
	}

	printf("\n");
}

void aumentarRafaga(ESI esi, t_esi_list* lista) {
	t_esi_node* puntero = lista->head;

	while (puntero != NULL) {
		if (puntero->esi.id == esi.id) {
			(puntero->esi.rafaga_real)++;
		}

		puntero = puntero->sgte;
	}
}

void reiniciarRafagaReal(ESI esi, t_esi_list* lista) {
	t_esi_node* puntero = lista->head;

	while (puntero != NULL) {
		if (puntero->esi.id == esi.id) {
			puntero->esi.rafaga_real = 0;
		}

		puntero = puntero->sgte;
	}
}

void actualizarEstimacion(ESI esi, t_esi_list* lista) {
	t_esi_node* puntero = lista->head;

	while (puntero != NULL) {
		if (puntero->esi.id == esi.id) {
			float estimated_aux = estimated_time(puntero->esi);
			puntero->esi.rafaga_estimada = estimated_aux;
			puntero->esi.rafaga_remanente = estimated_aux;
		}

		puntero = puntero->sgte;
	}

}

void actualizarTiempoArribo(ESI esi, t_esi_list* lista) {
	t_esi_node* puntero = lista->head;

	while (puntero != NULL) {
		if (puntero->esi.id == esi.id) {
			pthread_mutex_lock(&sem_clock);
			puntero->esi.tiempo_arribo = tiempo;
			pthread_mutex_unlock(&sem_clock);
		}

		puntero = puntero->sgte;
	}
}

void decrementarRemanente(ESI esi, t_esi_list* lista) {
	t_esi_node* puntero = lista->head;

	while (puntero != NULL) {
		if (puntero->esi.id == esi.id) {
			puntero->esi.rafaga_remanente--;
		}

		puntero = puntero->sgte;
	}
}

/*	=====================
 *	===== CONSOLITA =====
 *	=====================
 */

void* consola(void* nada) {
	int comando;
	printf("Bienvenido a la consola interactiva para el planificador \n");
	while (1) {
		if (display) {
			listarOpciones();
		}

		comando = recibirCodigo();
		interpretarYEjecutarCodigo(comando);

		if (!seguir_ejecucion) {
			break;
		}
	}

	return NULL;
}

float recibirCodigo() {
	int code = 0;

	scanf("%i", &code);
	return code;
}

void listarOpciones() {
	if (show_debug_commands) {
		printf("0: Enciende o apaga las opciones debug \n");
	}
	printf("1: Pausar o reactivar la planificación \n");
	printf("2: Bloquea un ESI detrás de una clave \n");
	printf("3: Desbloquea una clave \n");
	printf("4: Lista los procesos esperando un recurso \n");
	printf("5: Termina un ESI según su ID \n");
	printf("6: Muestra el estado de una clave \n");
	printf("7: Lista los ESI en deadlock \n");
	if (show_debug_commands) {
		printf("8: Muestra el clock interno del planificador \n");
		printf("9: Enciende o apaga el display de las opciones de consola \n");
		printf("10: Muestra datos de un ESI \n");
		printf("11: Muestra de todos los ESIs \n");
		printf("12: Bloquea una clave \n");
		printf("13: Desaloja al ESI actual \n");
		printf("14: Lista los comandos de consola \n");
		printf("99: Termina el proceso \n");
	}

	printf("Introduzca la opcion deseada \n");

}

void interpretarYEjecutarCodigo(int comando) {
	switch (comando) {
	case 0:
		show_debug();
		break;
	case 1:
		pausarOContinuar();
		break;
	case 2:
		bloquearSegunClave();
		break;
	case 3:
		desbloquear_clave();
		break;
	case 4:
		listar_bloqueados();
		break;
	case 5:
		matar();
		break;
	case 6:
		status();
		break;
	case 7:
		getDeadlock();
		break;
	case 8:
		mostrame_clock();
		break;
	case 9:
		display_console();
		break;
	case 10:
		datos_ESI();
		break;
	case 11:
		dame_datos();
		break;
	case 12:
		bloquear_clave();
		break;
	case 13:
		desalojar();
		break;
	case 14:
		show();
		break;
	case 99:
		terminar();
		break;
	case 420:
		weed();
		break;
	default:
		printf("Código incorrecto \n");
		break;
	}
}

void show(void) {
	bool aux = display;
	display = true;
	listarOpciones();
	display = aux;
}

void show_debug(void) {
	show_debug_commands = !show_debug_commands;
}

void datos_ESI(void) {
	int id;
	printf("Ingrese el ID del ESI: ");
	scanf("%i", &id);
	int id_as_uint = (uint32_t) id;

	ESI esi = findByIDIn(id_as_uint, ready_ESIs);

	if (esi.id != ESI_error.id) {
		printf("El ESI %i se encuentra en la cola de listos \n", id);
		printf("Su última ráfaga estimada es de %f \n", esi.rafaga_estimada);
		printf("Su última ráfaga real es de %i \n", esi.rafaga_real);
		printf("La estimación de su próxima ráfaga es de %f \n",
				estimated_time(esi));
		printf("Su ráfaga remanente es de %f \n", esi.rafaga_remanente);
		printf("Su último tiempo de arribo fue en t = %i \n",
				esi.tiempo_arribo);
		printf("Su tiempo de espera es de %i \n", wait_time(esi));
		printf("Su response ratio es de %f \n", response_ratio(esi));

		if (esi.id == executing_ESI.id) {
			printf("El ESI %i se encuentra actualmente ejecutando. \n", id);
		}

		return;
	}

	esi = findByIDIn(id_as_uint, finished_ESIs);

	if (esi.id != ESI_error.id) {
		printf("El ESI %i se encuentra en la cola de terminados \n", id);
		printf("Su última ráfaga estimada fue de %f \n", esi.rafaga_estimada);
		printf("Su última ráfaga real fue de %i \n", esi.rafaga_real);
		printf("La estimación de su próxima ráfaga sería de %f \n",
				estimated_time(esi));
		printf("Su ráfaga remanente es de %f \n", esi.rafaga_remanente);
		printf("Su último tiempo de arribo fue en t = %i \n",
				esi.tiempo_arribo);
		printf("Su response ratio sería de %f \n", response_ratio(esi));

	}

	esi = findByIDIn(id_as_uint, blocked_ESIs);

	if (esi.id != ESI_error.id) {
		printf("El ESI %i se encuentra en la cola de bloqueados \n", id);
		printf("Su última ráfaga estimada es de %f \n", esi.rafaga_estimada);
		printf("Su última ráfaga real es de %i \n", esi.rafaga_real);
		printf("La estimación de su próxima ráfaga es de %f \n",
				estimated_time(esi));
		printf("Su último tiempo de arribo fue en t = %i \n",
				esi.tiempo_arribo);
		printf("Su response ratio es de %f \n", response_ratio(esi));
	}

	printf("No se encontró ningún ESI en el sistema con ese ID \n");
}

void matar(void) {
	int id;
	printf("Introduzca el ESI ID: ");
	scanf("%i", &id);
	kill_esi((uint32_t) id);
}

void status(void) {
	char clave[40];
	printf("Ingrese la clave: ");
	scanf("%s", clave);

	aviso_con_ID aviso_status = { .aviso = 41 };
	enviar_aviso(socket_coordinador, aviso_status);

	uint32_t clave_size = (uint32_t) strlen(clave) + 1;
	package_int size_package = { .packed = clave_size };
	enviar_packed(size_package, socket_coordinador);
	enviar_cadena(clave, socket_coordinador);

	aviso_con_ID aviso_resultado = recibir_aviso(socket_coordinador);
	if (aviso_resultado.aviso == 0) {
		printf("La clave no se encuentra en el sistema \n");
		return;
	}

	if (aviso_resultado.aviso != 41) {
		salir_con_error("Falló la recepción del status", socket_coordinador);
	}

	package_int valor_size = recibir_packed(socket_coordinador);
	char* valor = recibir_cadena(socket_coordinador, valor_size.packed);

	package_int instancia_size = recibir_packed(socket_coordinador);
	char* instancia = recibir_cadena(socket_coordinador, instancia_size.packed);

	package_int bloqueados_size = recibir_packed(socket_coordinador);
	char* bloqueados = recibir_cadena(socket_coordinador,
			bloqueados_size.packed);

	printf("Valor: %s \n", valor);
	printf("Instancia: %s \n", instancia);
	printf("Bloqueados esperando: %s \n", bloqueados);

	free(valor);
	free(instancia);
	free(bloqueados);
}

void listar_bloqueados(void) {
	char clave[40];
	printf("Ingrese la clave: ");
	scanf("%s", clave);

	aviso_con_ID aviso_listar = { .aviso = 51 };
	enviar_aviso(socket_coordinador, aviso_listar);

	uint32_t clave_size = (uint32_t) strlen(clave) + 1;
	package_int size_package = { .packed = clave_size };
	enviar_packed(size_package, socket_coordinador);
	enviar_cadena(clave, socket_coordinador);

	aviso_con_ID aviso_resultado = recibir_aviso(socket_coordinador);
	if (aviso_resultado.aviso == 0) {
		printf("La clave no se encuentra en el sistema \n");
		return;
	}

	if (aviso_resultado.aviso != 51) {
		salir_con_error("Falló la recepción de los bloqueados",
				socket_coordinador);
	}

	package_int bloqueados_size = recibir_packed(socket_coordinador);
	char* bloqueados = recibir_cadena(socket_coordinador,
			bloqueados_size.packed);

	printf("Bloqueados esperando la clave: %s \n", bloqueados);

	free(bloqueados);
}

void bloquearSegunClave(void) {
	printf("Ingrese el ID del ESI: ");
	int id;
	scanf("%i", &id);
	uint32_t id_as_uint = (uint32_t) id;

	if (!existe(id)) {
		printf("El ESI no se encuentra en el sistema. \n");
		return;
	}

	else {
		ESI esi = { .id = id_as_uint };
		if (esta(blocked_ESIs, esi)) {
			printf("El ESI %i ya está bloqueado \n", id);
			return;
		}

		if (esta(finished_ESIs, esi)) {
			printf("El ESI %i ya terminó \n", id);
			return;
		}
	}

	printf("Ingrese la clave: ");
	char clave[40];
	scanf("%s", clave);

	aviso_con_ID aviso_bloqueo = { .aviso = 21 };
	enviar_aviso(socket_coordinador, aviso_bloqueo);

	uint32_t length = (uint32_t) strlen(clave) + 1;
	package_int size_package = { .packed = length };

	enviar_packed(size_package, socket_coordinador);
	enviar_cadena(clave, socket_coordinador);

	package_int id_package = { .packed = id_as_uint };
	enviar_packed(id_package, socket_coordinador);

	aviso_con_ID aviso_resultado = recibir_aviso(socket_coordinador);

	if (aviso_resultado.aviso != 21) {
		salir_con_error("Hubo un error bloqueando el ESI detrás de la clave",
				socket_coordinador);
	}

	pthread_mutex_lock(&sem_ready_ESIs);
	pthread_mutex_lock(&sem_ready_ESIs_size);
	ESI esi = findByIDIn(id_as_uint, ready_ESIs);
	eliminar_ESI(&ready_ESIs, esi);
	ESIs_size--;
	agregar_ESI(&blocked_ESIs, esi);
	pthread_mutex_unlock(&sem_ready_ESIs);
	pthread_mutex_unlock(&sem_ready_ESIs_size);

	printf("El ESI %i fue bloqueado tras la clave %s \n", id, clave);
}

void desbloquear_clave() {
	printf("Ingrese la clave a desbloquear: ");
	char clave[40] = "futbol:messi";
	scanf("%s", clave);

	int local_socket = socket_coordinador;

	avisar_desbloqueo(local_socket, clave);

	printf("La clave %s fue desbloqueada \n", clave);

	conseguir_desbloqueados();
	planificar();
}

void avisar_desbloqueo(int server_socket, char* clave) {
	aviso_con_ID desbloqueo_clave = { .aviso = 31 };
	enviar_aviso(server_socket, desbloqueo_clave);

	uint32_t clave_size = strlen(clave) + 1;
	package_int size_package = { .packed = clave_size };
	enviar_packed(size_package, server_socket);
	enviar_cadena(clave, server_socket);

	aviso_con_ID respuesta_desbloqueo = recibir_aviso(server_socket);
	log_debug(debug_logger, "Respuesta: %i", respuesta_desbloqueo.aviso);
}

void desbloquear_ESI(uint32_t id) {
	ESI esi = findByIDIn(id, blocked_ESIs);
	if (esi.id == ESI_error.id) {
		log_warning(logger, "El ESI %i no se encuentra en el sistema.", id);
		return;
	}

	pthread_mutex_lock(&sem_ready_ESIs);
	pthread_mutex_lock(&sem_ready_ESIs_size);

	eliminar_ESI(&blocked_ESIs, esi);
	agregar_ESI(&ready_ESIs, esi);
	actualizarTiempoArribo(esi, &ready_ESIs);
	reiniciarRafagaReal(esi, &ready_ESIs);

	ESIs_size++;

	pthread_mutex_unlock(&sem_ready_ESIs);
	pthread_mutex_unlock(&sem_ready_ESIs_size);

	log_info(logger, "El ESI %i fue desbloqueado.", id);
}

void bloquear_clave() {
	printf("Ingrese la clave a bloquear: ");
	char clave[40] = "futbol:messi";
	scanf("%s", clave);

	int local_socket = socket_coordinador;

	avisar_bloqueo(local_socket, clave);

	printf("Clave bloqueada \n");
}

void avisar_bloqueo(int server_socket, char* clave) {
	aviso_con_ID bloqueo_clave = { .aviso = 32 };
	enviar_aviso(server_socket, bloqueo_clave);

	uint32_t clave_size = strlen(clave) + 1;
	package_int size_package = { .packed = clave_size };
	enviar_packed(size_package, server_socket);
	enviar_cadena(clave, server_socket);

	aviso_con_ID respuesta_bloqueo = recibir_aviso(server_socket);
	log_debug(debug_logger, "Respuesta: %i", respuesta_bloqueo.aviso);

}

void dame_datos() {
	if (executing_ESI.id == esi_vacio.id)
		printf("ESI ejecutando: no hay ESI ejecutando actualmente \n");
	else
		printf("ESI ejecutando: %i \n", executing_ESI.id);

	printf("ESIs listos para ejecutar:");
	mostrar(ready_ESIs, &sem_ready_ESIs);

	printf("ESIs bloqueados:");
	mostrar(blocked_ESIs, &sem_blocked_ESIs);

	printf("ESIs terminados:");
	mostrar(finished_ESIs, &sem_finished_ESIs);
}

void display_console() {
	display = !display;
}

void mostrame_clock(void) {
	pthread_mutex_lock(&sem_clock);
	printf("El clock de instrucciones va por %i. \n", tiempo);
	pthread_mutex_unlock(&sem_clock);
}

void terminar(void) {
	printf("Eligio cerrar el planificador \n");
	cerrar();
}

void pausarOContinuar(void) {
	if (consola_planificacion) {
		printf("Pausando planificación...\n");

		consola_planificacion = false;

		sleep(0.5);

		printf("Planificación pausada.\n");

	}

	else {
		printf("Reanudando planificacion...\n");

		consola_planificacion = true;

		sleep(0.5);

		printf("Planificación reanudada.\n");
		planificar();

	}

}

void kill_esi(int id) {
	uint32_t id_as_uint = (uint32_t) id;

	pthread_mutex_lock(&sem_ready_ESIs_size);
	pthread_mutex_lock(&sem_ready_ESIs);
	ESI asesina3 = findByIDIn(id_as_uint, ready_ESIs);

	if (asesina3.id != ESI_error.id) {
		kill_ESI(asesina3);
		eliminar_ESI(&ready_ESIs, asesina3);
		ESIs_size--;
		printf("ESI %i abortado. \n", id);

		pthread_mutex_unlock(&sem_ready_ESIs_size);
		pthread_mutex_unlock(&sem_ready_ESIs);

		return;
	}
	pthread_mutex_unlock(&sem_ready_ESIs_size);
	pthread_mutex_unlock(&sem_ready_ESIs);

	pthread_mutex_lock(&sem_blocked_ESIs);
	asesina3 = findByIDIn(id_as_uint, blocked_ESIs);

	if (asesina3.id != ESI_error.id) {
		kill_ESI(asesina3);
		eliminar_ESI(&blocked_ESIs, asesina3);
		printf("ESI %i abortado. \n", id);

		pthread_mutex_unlock(&sem_blocked_ESIs);

		return;
	}
	pthread_mutex_unlock(&sem_blocked_ESIs);

	pthread_mutex_lock(&sem_finished_ESIs);
	asesina3 = findByIDIn(id_as_uint, finished_ESIs);

	if (asesina3.id != ESI_error.id) {
		printf("El ESI %i ya terminó, así que no pudo ser abortado. \n", id);
		pthread_mutex_unlock(&sem_finished_ESIs);
		return;
	}
	pthread_mutex_unlock(&sem_finished_ESIs);

	printf(
			"El ESI %i no se encuentra en el sistema, por lo que no se pudo abortar. \n",
			id);
}

void getDeadlock(void) {
	aviso_con_ID aviso_deadlock = { .aviso = 404 };
	enviar_aviso(socket_coordinador, aviso_deadlock);

	printf("ESIs en Deadlock:");
	int i = 0;

	while (1) {
		aviso_con_ID deadlock_id = recibir_aviso(socket_coordinador);
		log_debug(debug_logger, "Aviso: %i", deadlock_id.aviso);
		log_debug(debug_logger, "ID: %i", deadlock_id.id);

		if (deadlock_id.aviso == 0) {
			printf(" no hay ESIs en Deadlock \n");
			return;
		}

		if (deadlock_id.aviso == 414) {
			break;
		}

		if (!estaBloqueado(deadlock_id.id)) {
			continue;
		}

		if (i > 0) {
			printf(",");
		}

		printf(" %i", deadlock_id.id);
		i++;
	}

	printf("\n");
	printf("\n");
}

bool estaBloqueado(uint32_t id) {
	ESI esi = findByIDIn(id, blocked_ESIs);

	return esi.id == id;
}

void weed() {
	printf(WEED "S M O K E   W E E D   E V E R Y D A Y \n");
}
