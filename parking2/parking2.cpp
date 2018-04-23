// parking2.cpp: define el punto de entrada de la aplicación de consola.
//


#inlcude "header.h"

//  Funcion para tratamiento de argumentos
void registrarArgumentos(int argc, char *argv[], int*vel, int*debug);
//  Funciones de reserva de los IPCs e inicializacion de los mismos
void reservarIpcs(int nChof);
void initSemaforos(int nChof);
void initBuzones();
void initMemoria();

//  Funcion de los procesos choferes. Piden un trabajo al gestor y
//  llaman a las funciones aparcar() o desaparcar() de la
//  biblioteca.
void choferFunction(void);

//  Funciones de callback para interfaz con la biblioteca
void commit(HCoche c);
void permisoAvance(HCoche c);
void permisoAvanceCommit(HCoche c);


/*  Diferentes algoritmos de posicion de aparcamiento en la acera.
*  Llamados por la biblioteca cada vez que quiere aparcar un
*  nuevo coche.
*/
int primerAjuste(HCoche c);
int mejorAjuste(HCoche c);
int peorAjuste(HCoche c);
int siguienteAjuste(HCoche c);

//  Handlers de los procesos. Usadas cuando se termina la ejecucion 
//  del programa para liberar recursos.
void freeResources(int ss);

/*  Funciones de abstraccion para las operaciones con semaforos de SYSTEM V.
*  No llamadas directamente dentro del programa; son usadas dentro de las
*  macros de inicio y final de seccion critica:
*      - START_WRITE_CRITICAL_SECTION
*      - END_WRITE_CRITICAL_SECTION
*      - START_READ_CRITICAL_SECTION
*      - END_READ_CRITICAL_SECTION
*/
void semops(int pos, int typeOp, int quantity);


//  Estructura principal del programa.
struct _global {
	//  Punteros a las aceras/carreteras de cada algoritmo.
	//  Iniciadas en initMemoria()
	bool* memAceras[4];

	//  Representan manejadores opacos de los recursos IPC.
	int sem, mem, buzon;

	//  Puntero de toda la memoria compartida, usado para iniciar
	//  memAceras[] y memCarreteras[] y para liberar toda la
	//  memoria en freeResources()
	void* memp;

	//Valor para saber si es un hijo o el padre
	bool isChild;
}global;


int main(int argc, char *argv[]) {
	int vel,debug;
	TIPO_FUNCION_LLEGADA func[4] = { primerAjuste, siguienteAjuste, mejorAjuste, peorAjuste };

	global.isChild = FALSE;
	global.sem = -1;
	global.buzon = -1;
	global.mem = -1;

	registrarArgumentos(argc, argv, &vel, &debug);

	reservarIpcs(nChof);

	PARKING2_inicio(vel, func, global.sem, global.buzon, global.mem, global.debug);

	procrear(nChof, prioridad);

	PARKING2_simulaciOn();

	freeResources(SIGALRM);
}

void registrarArgumentos(int argc, char *argv[], int*vel, int *debug) {
	if (argc < 2 || argc > 3) {
		EXIT("%s\n", USAGE_ERROR_MSG);
	}

	*vel = atoi(argv[1]);
	if (*vel < 0) {
		EXIT("%s\n", USAGE_ERROR_MSG);
	}
	*debug = !strcmp(argv[3], "D");
	if (*debug != 0) {
		EXIT("%s\n", USAGE_ERROR_MSG);
	}
}
//----------- FUNCIONES DE RESERVA E INICIALIZACION DE IPCS ---------------------------
void reservarIpcs(int nChof) {

	initSemaforos(nChof);

	initBuzones();

	initMemoria();
}

void initSemaforos(int nChof) {
	int i;
	union semun semunion;

	//Reserva de semaforos
	EXIT_ON_FAILURE(global.sem = semget(IPC_PRIVATE, PARKING2_getNSemAforos() + 4 + 4 + 1, IPC_CREAT | 0600));

	//Valores iniciales  a los semaforos
	semunion.val = nChof + 3;
	EXIT_ON_FAILURE(semctl(global.sem, SEM_START, SETVAL, semunion));
	for (i = SEM_WRITE(PRIMER_AJUSTE); i <= SEM_READ(PEOR_AJUSTE); i++) {
		semunion.val = 0;
		EXIT_ON_FAILURE(semctl(global.sem, i, SETVAL, semunion));
	}
}

void initBuzones() {
	int i;
	struct mensajeOrden initialMsg = { 1 };

	//Reserva de los buzones
	EXIT_ON_FAILURE(global.buzon = msgget(IPC_PRIVATE, IPC_CREAT | 0600));

	//Enviamos un primer mensaje al buzon de orden para que el primer chofer pueda salir
	for (i = PRIMER_AJUSTE; i <= PEOR_AJUSTE; i++) {
		initialMsg.tipo = TIPO_ORDEN(1, i);
		EXIT_ON_FAILURE(msgsnd(global.buzon, &initialMsg, MSG_SIZE(initialMsg), 0));
	}
}

void initMemoria() {
	int i;
	bool* memAcerasTemp;
	Carretera* memCarrTemp;

	int tamannoMemAcera = sizeof(bool) * 4 * MAX_LONG_ROAD;
	int tamannoMemCarretera = sizeof(Carretera) * 4 * MAX_LONG_ROAD;
	int tamannoMemTotal = PARKING_getTamaNoMemoriaCompartida() + tamannoMemAcera + tamannoMemCarretera;

	//Reserva la memoria compartida y recoge un puntero a la misma
	EXIT_ON_FAILURE(global.mem = shmget(IPC_PRIVATE, tamannoMemTotal, IPC_CREAT | 0600));
	EXIT_IF_NULL(global.memp = (void*)shmat(global.mem, NULL, 0));

	memset(global.memp, 0, tamannoMemTotal);

	//Iniciamos los arrays de memoria memAceras[] y memCarreteras[] para un acceso facil a la misma dentro del programa 
	memAcerasTemp = (bool*)(global.memp + PARKING2_getTamaNoMemoriaCompartida());
	memCarrTemp = (Carretera*)(global.memp + PARKING2_getTamaNoMemoriaCompartida() + tamannoMemAcera);

	for (i = PRIMER_AJUSTE; i <= PEOR_AJUSTE; i++) {
		global.memAceras[i] = memAcerasTemp + (MAX_LONG_ROAD*i);
		global.memCarreteras[i] = memCarrTemp + (MAX_LONG_ROAD*i);
	}
}

//----------- FUNCION DE LOS PROCESOS CHOFERES ----------------------------------------
void choferFunction(void) {
	struct PARKING2_mensajeBiblioteca mensj = { 0,0,0 };
	struct mensajeOrden orden;

	msgCarretera datosPeticiones[MAX_DATOS_COCHE] = { 0 };

	REDEFINE_SIGNAL(SIGINT, childHandler);

	READY(FALSE);

	while (TRUE) {

		//Envia un mensaje al mailManager para pedir un nuevo comando
		mensj.tipo = TIPO_REQUEST;
		EXIT_ON_FAILURE(msgsnd(global.buzon, &mensj, MSG_SIZE(mensj), 0));
		EXIT_ON_FAILURE(msgrcv(global.buzon, &mensj, MSG_SIZE(mensj), TIPO_COMANDO, 0));

		//Si tiene que aparcar espera por la confirmacion del inmediatamente anterior para salir
		if (mensj.subtipo == PARKING_MSGSUB_APARCAR) {
			EXIT_ON_FAILURE(msgrcv(global.buzon, &orden, MSG_SIZE(orden), TIPO_ORDEN(PARKING_getNUmero(mensj.hCoche), PARKING_getAlgoritmo(mensj.hCoche)), 0));
			PARKING_aparcar(mensj.hCoche, &datosPeticiones, commit, permisoAvance, permisoAvanceCommit);
		}
		else {
			PARKING_desaparcar(mensj.hCoche, &datosPeticiones, permisoAvance, permisoAvanceCommit);
		}
	}
}

//----------- FUNCIONES DE CALLBACK DE INTERFAZ CON LA BIBLIOTECA ---------------------
void commit(HCoche c) {
	struct mensajeOrden mensj;
	mensj.tipo = TIPO_ORDEN(PARKING_getNUmero(c) + 1, PARKING_getAlgoritmo(c));

	EXIT_ON_FAILURE(msgsnd(global.buzon, &mensj, MSG_SIZE(mensj), 0));
}

void permisoAvance(HCoche c) {
	Carretera* carr = global.memCarreteras[PARKING_getAlgoritmo(c)];
	int posReserva, posOcupacionInicio, posOcupacionFin;

	// Si esta en carretera y se esta ocultando o si va a aparcar
	// siempre se da al coche permiso de avance.
	if (ESTA_EN_CARRETERA(c) && PARKING_getX(c) > 0) {
		posReserva = PARKING_getX2(c);
		posOcupacionInicio = posOcupacionFin = PARKING_getX2(c);    // La zona critica de ocupacion es solo la casilla de delante.
	}
	else if (ESTA_DESAPARCANDO_AVANCE(c)) {
		posReserva = PARKING_getX2(c) + PARKING_getLongitud(c) - 1; // Se reserva la ultima posicion del coche para evitar que
		posOcupacionInicio = PARKING_getX2(c);                      // otro coche avance dentro de su zona de desaparcamiento.
		posOcupacionFin = posReserva;                               // La zona critica de ocupacion es toda la longitud del coche
	}                                                               // en carretera.
	else {
		return;
	}

	reservarCarretera(c, carr, posReserva);
	pedirPermisoOcupacion(c, carr, posOcupacionInicio, posOcupacionFin);
}

void permisoAvanceCommit(HCoche c) {
	Carretera*carr = global.memCarreteras[PARKING_getAlgoritmo(c)];

	//Desreservamos la acera, es zona de exclusion mutua a ojos de la biblioteca gracias a un semaforo interno
	if (ESTA_DESAPARCANDO_COMMIT(c)) {
		bool* acera = global.memAceras[PARKING_getAlgoritmo(c)];
		memset(acera + PARKING_getX(c), FALSE, sizeof(bool)*PARKING_getLongitud(c));
	}

	actualizarCarretera(c, carr);

	recepcionPeticionReserva(c);
	recepcionPeticionOcupacion(c);

}

//----------- FUNCIONES DE ALGORITMOS DE POSICION DE APARCAMIENTO ---------------------
int primerAjuste(HCoche c) {

	int longitud;
	int posInicial, longLibre, i = -1;
	bool* acera;

	acera = global.memAceras[PARKING_getAlgoritmo(c)];

	longitud = PARKING_getLongitud(c);
	while (i < MAX_LONG_ROAD) {
		i++;
		longLibre = 0;
		while (acera[i] == FALSE && i < MAX_LONG_ROAD) {
			i++;
			longLibre++;
			if (longLibre == longitud) {
				posInicial = i - longitud;
				memset(acera + posInicial, TRUE, sizeof(bool)*longitud);
				return posInicial;
			}
		}
	}

	return -1;
}

int mejorAjuste(HCoche c) {

	int longitud, i, p, f, pa, fa;
	bool* acera;

	acera = global.memAceras[PARKING_getAlgoritmo(c)];
	longitud = PARKING_getLongitud(c);

	i = 0;
	p = f = pa = fa = -1;

	while (i<MAX_LONG_ROAD) {
		if (acera[i] == FALSE) {
			p = i;
			while (acera[i] == FALSE && i<MAX_LONG_ROAD) { i++; }
			f = i - 1;

			if (pa == -1 && (f - p + 1) >= longitud) {
				pa = p;
				fa = f;
			}
			else if ((f - p + 1) >= longitud && (f - p)<(fa - pa)) {
				pa = p;
				fa = f;
			}
		}
		i++;
	}


	if (pa != -1)
		memset(acera + pa, TRUE, sizeof(bool)*longitud);

	return pa;
}

int peorAjuste(HCoche c) {

	int longitud, i, p, f, pa, fa;
	bool* acera;

	acera = global.memAceras[PARKING_getAlgoritmo(c)];
	longitud = PARKING_getLongitud(c);

	i = 0;
	p = f = pa = fa = -1;

	while (i<MAX_LONG_ROAD) {
		if (acera[i] == FALSE) {
			p = i;
			while (acera[i] == FALSE && i<MAX_LONG_ROAD) { i++; }
			f = i - 1;

			if (pa == -1 && (f - p + 1) >= longitud) {
				pa = p;
				fa = f;
			}
			else if ((f - p + 1) >= longitud && (f - p)>(fa - pa)) {
				pa = p;
				fa = f;
			}
		}
		i++;
	}


	if (pa != -1)
		memset(acera + pa, TRUE, sizeof(bool)*longitud);

	return pa;
}

int siguienteAjuste(HCoche c) {

	static int start = -1;

	int posInicial, longLibre;
	bool* acera = global.memAceras[PARKING_getAlgoritmo(c)];
	int i = start;
	int contador = -1;
	int longitud = PARKING_getLongitud(c);

	while (acera[i + 1] == FALSE && i >= 0) i--;

	while (contador <= MAX_LONG_ROAD) {
		i = (i + 1 < MAX_LONG_ROAD) ? i + 1 : 0;
		contador++;
		longLibre = 0;
		while (acera[i] == FALSE && contador <= MAX_LONG_ROAD && i < MAX_LONG_ROAD) {
			i++;
			contador++;
			longLibre++;
			if (longLibre == longitud) {
				posInicial = i - longitud;
				memset(acera + posInicial, TRUE, sizeof(bool)*longitud);
				start = posInicial - 1;

				return posInicial;
			}
		}
	}
	return -1;
}

//----------- MANEJADORAS DE LOS PROCESOS ---------------------------------------------
void freeResources(int ss) {

	//  Si el hijo entra en esta funcion es a causa de un error.
	//  Envia SINGINT al padre y muere con codigo de error. 
	if (global.isChild == TRUE) {
		PRINT_ERROR(kill(SIGINT, getppid()));
		exit(EXIT_FAILURE);
	}

	int i;

	if (ss == SIGINT)
		PARKING_fin(1);
	else if (ss == -1)
		PARKING_fin(0);

	if (ss == SIGALRM) {
		PRINT_ERROR(waitpid(global.timer, NULL, 0));
		KILL_PROCESS(global.mailManager, SIGINT);
		for (i = 0; global.chofers[i] != 0; i++) {
			KILL_PROCESS(global.chofers[i], SIGINT);
		}
	}
	else if (ss == SIGINT) {
		PRINT_ERROR(waitpid(global.timer, NULL, 0));
		PRINT_ERROR(waitpid(global.mailManager, NULL, 0));
		for (i = 0; global.chofers[i] != 0; i++) {
			PRINT_ERROR(waitpid(global.chofers[i], NULL, 0));
		}
	}
	else {
		KILL_PROCESS(global.timer, SIGINT);
		KILL_PROCESS(global.mailManager, SIGINT);
		if (global.chofers != NULL) {
			for (i = 0; global.chofers[i] != 0; i++) {
				KILL_PROCESS(global.chofers[i], SIGINT);
			}
		}
	}
	if (global.chofers != NULL)
		free(global.chofers);

	if (-1 != global.sem)
		PRINT_ERROR(semctl(global.sem, 0, IPC_RMID));
	if (-1 != global.buzon)
		PRINT_ERROR(msgctl(global.buzon, IPC_RMID, NULL));
	if (-1 != global.mem && NULL != global.memp)
		PRINT_ERROR(shmdt(global.memp));
	if (-1 != global.mem)
		PRINT_ERROR(shmctl(global.mem, IPC_RMID, NULL));

	if (global.debug) {
		for (i = PRIMER_AJUSTE; i <= PEOR_AJUSTE; i++)
			fclose(global.dump_file[i]);
	}

	exit(EXIT_SUCCESS);
}

//----------- FUNCIONES DE USO DE SEMAFOROS SYSTEM V ----------------------------------

/*
*  @params
*      pos: semaforo dentro del vector
*      typeOp: WAIT, WAIT0 o SIGNAL
*      quantity: cantidad de operaciones
*/
void semops(int pos, int typeOp, int quantity) {
	struct sembuf sops;

	sops.sem_op = typeOp * quantity;
	sops.sem_num = pos;
	sops.sem_flg = 0;

	EXIT_ON_FAILURE(semop(global.sem, &sops, 1));
}
