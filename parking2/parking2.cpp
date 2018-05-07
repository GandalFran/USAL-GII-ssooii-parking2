/*
*	@autor: Gabino Luis Lazo (i1028058)
*	@autor: Francisco Pinto Santos (i0918455)
*
*	Segunda prActica SSOO II - PARKING2
*      http://avellano.usal.es/~ssooii/PARKING/parking2.htm
*/


#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <Windows.h>

#define PARKING2_IMPORTS

#include "parking2.h"

#define USAGE_ERROR_MSG "Usage: parking.exe <velocidad> [D]"
#define DLL_LOAD_ERROR "DLL couldn't be loaded."
#define FUNCTION_LOAD_ERROR "function couldn't be loaded."
#define THREAD_CREATION_ERROR "The threath couldn't be created."
#define IPC_CREATION_ERROR "The IPC couldn't be created."
#define OP_FAILED "A internal operation failed."

#define MAX_LONG_ROAD 80
	
#define EXIT_IF_WRONG_VALUE(ReturnValue,ErrorValue,ErrorMsg)							\
    do{																					\
        if((ReturnValue) == (ErrorValue)){												\
            fprintf(stderr, "\n[%d:%s] ERROR: %s", __LINE__, __FUNCTION__,ErrorMsg);	\
            exit(EXIT_FAILURE);																	\
        }																				\
    }while(0)

//--------------- MOVIMIENTOS ---------------------------------------------------------
#define ESTA_DESAPARCANDO_AVANCE(coche)     (Funciones.GetY(coche)  == 1 && Funciones.GetY2(coche) == 2)
#define ESTA_DESAPARCANDO_COMMIT(coche)     (Funciones.GetY2(coche) == 1 && Funciones.GetY(coche)  == 2)
#define ESTA_APARCANDO_AVANCE(coche)        (Funciones.GetY2(coche) == 1 && Funciones.GetY(coche)  == 2)
#define ESTA_APARCANDO_COMMIT(coche)        (Funciones.GetY(coche)  == 1 && Funciones.GetY2(coche) == 2)
#define ESTA_EN_CARRETERA(coche)            (Funciones.GetY2(coche) == 2 && Funciones.GetY(coche)  == 2)


typedef int(*PARKING_Inicio)(TIPO_FUNCION_LLEGADA *, TIPO_FUNCION_SALIDA *, long, int);
typedef int(*PARKING_Fin)(void);
typedef int(*PARKING_Aparcar)(HCoche, void *datos, TIPO_FUNCION_APARCAR_COMMIT,
												   TIPO_FUNCION_PERMISO_AVANCE,
												   TIPO_FUNCION_PERMISO_AVANCE_COMMIT);
typedef int (*PARKING_Desaparcar)(HCoche, void *datos,
										  TIPO_FUNCION_PERMISO_AVANCE,
										  TIPO_FUNCION_PERMISO_AVANCE_COMMIT);
typedef int(*PARKING_Get)(HCoche);
typedef void*(*PARKING_GetDatos)(HCoche);

typedef HCoche* PHCoche;
typedef HANDLE CARRETERA, *PCARRETERA;
typedef BOOL ACERA, *PACERA;
typedef struct _DatosCoche {
	HCoche hc;
	HANDLE EventOrdenAnterior;
	HANDLE EventOrdenActual;
} DATOSCOCHE, *PDATOSCOCHE;

HANDLE TurnoCoche[4] = { 0, 0, 0, 0 };

PCARRETERA Carretera[4];
PACERA Acera[4];

struct _Funciones{
	HMODULE WINAPI ParkingLibrary;

	PARKING_Get GetNumero;
	PARKING_Get GetLongitud;
	PARKING_Get GetAlgoritmo;
	PARKING_Get GetX;
	PARKING_Get GetX2;
	PARKING_Get GetY;
	PARKING_Get GetY2;
	PARKING_GetDatos GetDatos;
	PARKING_Aparcar Aparcar;
	PARKING_Desaparcar Desaparcar;
	PARKING_Inicio Inicio;
	PARKING_Fin Fin;
}Funciones;


void InitFunctions();

int Llegada(HCoche hc);
int Salida(HCoche hc);
DWORD WINAPI AparcarRoutine(LPVOID lpParam);
DWORD WINAPI DesaparcarRoutine(LPVOID lpParam);

void AparcarCommit(HCoche hc);
void PermisoAvance(HCoche hc);
void PermisoAvanceCommit(HCoche hc);

int PrimerAjuste(HCoche c);
int MejorAjuste(HCoche c);
int PeorAjuste(HCoche c);
int SiguienteAjuste(HCoche c);



int main(int argc, char** argv)
{
	long Velocidad;
	BOOL Debug = FALSE;

	if (argc > 3 || argc < 2) {
		fprintf(stderr, USAGE_ERROR_MSG);
		return 1;
	}

	Velocidad = atol(argv[1]);

	if (argc == 3) {
		Debug = strcmp(argv[2], "D") == 0 ? TRUE : FALSE;
	}

	InitFunctions();

	for (int i = PRIMER_AJUSTE; i <= PEOR_AJUSTE; i++) {
		EXIT_IF_WRONG_VALUE(Acera[i] = (PBOOL)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ACERA) * MAX_LONG_ROAD), NULL, OP_FAILED);
		EXIT_IF_WRONG_VALUE(Carretera[i] = (PCARRETERA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CARRETERA) * MAX_LONG_ROAD), NULL, OP_FAILED);
		for (int j = 0; j < MAX_LONG_ROAD; j++) {
			EXIT_IF_WRONG_VALUE(Carretera[i][j] = CreateMutex(NULL, FALSE, NULL),NULL,IPC_CREATION_ERROR);
		}
	}


	TIPO_FUNCION_LLEGADA FuncionesLlegada[] = { Llegada, Llegada, Llegada, Llegada };
	TIPO_FUNCION_SALIDA FuncionesSalida[] = { Salida, Salida, Salida, Salida };
	
	Funciones.Inicio(FuncionesLlegada, FuncionesSalida, Velocidad, Debug);

	Sleep(30000);

	Funciones.Fin();

	return 0;
}

void InitFunctions(){
	EXIT_IF_WRONG_VALUE(Funciones.ParkingLibrary = LoadLibrary(TEXT("parking2.dll")), NULL, DLL_LOAD_ERROR);
	
	EXIT_IF_WRONG_VALUE(Funciones.GetNumero = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getNUmero"), NULL, FUNCTION_LOAD_ERROR);
	EXIT_IF_WRONG_VALUE(Funciones.GetLongitud = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getLongitud"), NULL, FUNCTION_LOAD_ERROR);
	EXIT_IF_WRONG_VALUE(Funciones.GetAlgoritmo = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getAlgoritmo"), NULL, FUNCTION_LOAD_ERROR);
	EXIT_IF_WRONG_VALUE(Funciones.GetX = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getX"), NULL, FUNCTION_LOAD_ERROR);
	EXIT_IF_WRONG_VALUE(Funciones.GetX2 = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getX2"), NULL, FUNCTION_LOAD_ERROR);
	EXIT_IF_WRONG_VALUE(Funciones.GetY = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getY"), NULL, FUNCTION_LOAD_ERROR);
	EXIT_IF_WRONG_VALUE(Funciones.GetY2 = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getY2"), NULL, FUNCTION_LOAD_ERROR);

	EXIT_IF_WRONG_VALUE(Funciones.GetDatos = (PARKING_GetDatos)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getDatos"), NULL, FUNCTION_LOAD_ERROR);

	EXIT_IF_WRONG_VALUE(Funciones.Inicio = (PARKING_Inicio)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_inicio"), NULL, FUNCTION_LOAD_ERROR);
	EXIT_IF_WRONG_VALUE(Funciones.Fin = (PARKING_Fin)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_fin"), NULL, FUNCTION_LOAD_ERROR);

	EXIT_IF_WRONG_VALUE(Funciones.Aparcar = (PARKING_Aparcar)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_aparcar"), NULL, FUNCTION_LOAD_ERROR);
	EXIT_IF_WRONG_VALUE(Funciones.Desaparcar = (PARKING_Desaparcar)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_desaparcar"), NULL, FUNCTION_LOAD_ERROR);
}

//-------------------------------------------------------------------------------------------------------------------------------------

int Llegada(HCoche hc) {
	static TIPO_FUNCION_LLEGADA Ajustes[] = { PrimerAjuste, SiguienteAjuste, MejorAjuste, PeorAjuste };
	PDATOSCOCHE Datos;
	int PosAceraAparcar;

	PosAceraAparcar = Ajustes[Funciones.GetAlgoritmo(hc)](hc);

	if (PosAceraAparcar >= 0) {
		HANDLE EventOrden;
		EXIT_IF_WRONG_VALUE(EventOrden = CreateEvent(NULL, TRUE, FALSE, NULL), NULL, IPC_CREATION_ERROR);
		EXIT_IF_WRONG_VALUE(Datos = (PDATOSCOCHE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DATOSCOCHE)), NULL, OP_FAILED);

		Datos->EventOrdenActual = EventOrden;
		Datos->hc = hc;
		Datos->EventOrdenAnterior = TurnoCoche[Funciones.GetAlgoritmo(hc)];
		TurnoCoche[Funciones.GetAlgoritmo(hc)] = Datos->EventOrdenActual;

		EXIT_IF_WRONG_VALUE(CreateThread(NULL, 0, AparcarRoutine, Datos, 0, NULL),NULL,THREAD_CREATION_ERROR);
	}

	return PosAceraAparcar;
}

int Salida(HCoche hc) {
	PDATOSCOCHE Datos;

	EXIT_IF_WRONG_VALUE(Datos = (PDATOSCOCHE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DATOSCOCHE)), NULL, OP_FAILED);
	Datos->hc = hc;

	EXIT_IF_WRONG_VALUE(CreateThread(NULL, 0, DesaparcarRoutine, Datos, FALSE, NULL), NULL, THREAD_CREATION_ERROR);
	return 0;
}

//-------------------------------------------------------------------------------------------------------------------------------------

DWORD WINAPI AparcarRoutine(LPVOID lpParam) {

	PDATOSCOCHE Datos = (PDATOSCOCHE)lpParam;

	if (Datos->EventOrdenAnterior != 0) {
		EXIT_IF_WRONG_VALUE(WaitForSingleObject(Datos->EventOrdenAnterior, INFINITE), WAIT_FAILED, OP_FAILED);
	}

	Funciones.Aparcar(Datos->hc, Datos, AparcarCommit, PermisoAvance, PermisoAvanceCommit);

	EXIT_IF_WRONG_VALUE(HeapFree(GetProcessHeap(), 0, Datos), FALSE, OP_FAILED);

	return 0;
}

DWORD WINAPI DesaparcarRoutine(LPVOID lpParam){

	PDATOSCOCHE Datos = (PDATOSCOCHE)lpParam;
	Funciones.Desaparcar(Datos->hc, Datos, PermisoAvance, PermisoAvanceCommit);

	EXIT_IF_WRONG_VALUE(HeapFree(GetProcessHeap(), 0, Datos), FALSE, OP_FAILED);

	return 0;
}

//-------------------------------------------------------------------------------------------------------------------------------------

void AparcarCommit(HCoche hc) {

	PDATOSCOCHE DatosCoche = (PDATOSCOCHE)Funciones.GetDatos(hc);
	SetEvent(DatosCoche->EventOrdenActual);

}

void PermisoAvance(HCoche hc) {
	PCARRETERA Carr = Carretera[Funciones.GetAlgoritmo(hc)];

	if (ESTA_EN_CARRETERA(hc) && Funciones.GetX(hc) > 0) {
		EXIT_IF_WRONG_VALUE(WaitForSingleObject(Carr[Funciones.GetX2(hc)], INFINITE),WAIT_FAILED,OP_FAILED);
	}
	else if (ESTA_DESAPARCANDO_AVANCE(hc)){
		for (int i = Funciones.GetX(hc) + Funciones.GetLongitud(hc) - 1; i >= Funciones.GetX(hc); i--) {
			EXIT_IF_WRONG_VALUE(WaitForSingleObject(Carr[i], INFINITE), WAIT_FAILED, OP_FAILED);
		}
	}
	else{
		return;
	}
}

void PermisoAvanceCommit(HCoche hc) {
	PCARRETERA Carr = Carretera[Funciones.GetAlgoritmo(hc)];

	if (ESTA_EN_CARRETERA(hc) && Funciones.GetX(hc) + Funciones.GetLongitud(hc) < MAX_LONG_ROAD){
		EXIT_IF_WRONG_VALUE(ReleaseMutex(Carr[Funciones.GetX(hc) + Funciones.GetLongitud(hc)]),0,OP_FAILED);
	}
	else if (ESTA_DESAPARCANDO_COMMIT(hc)){
		PACERA AceraAlg = Acera[Funciones.GetAlgoritmo(hc)];
		memset(AceraAlg + Funciones.GetX(hc), FALSE, sizeof(ACERA)*Funciones.GetLongitud(hc));
	}
	else if (ESTA_APARCANDO_COMMIT(hc)){
		for (int i = Funciones.GetX(hc); i < Funciones.GetX(hc) + Funciones.GetLongitud(hc); i++) {
			EXIT_IF_WRONG_VALUE(ReleaseMutex(Carr[i]), 0, OP_FAILED);
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------------------------

int PrimerAjuste(HCoche hc) {

	PACERA AceraAlg = Acera[PRIMER_AJUSTE];
	int Longitud;
	int PosInicial, LongLibre, i;

	i = -1;
	AceraAlg = Acera[PRIMER_AJUSTE];
	Longitud = Funciones.GetLongitud(hc);

	while (i < MAX_LONG_ROAD) {
		i++;
		LongLibre = 0;
		while (AceraAlg[i] == FALSE && i < MAX_LONG_ROAD) {
			i++;
			LongLibre++;
			if (LongLibre == Longitud) {
				PosInicial = i - Longitud;
				memset(AceraAlg + PosInicial, TRUE, sizeof(ACERA) * Longitud);
				return PosInicial;
			}
		}
	}

	return -1;
}

int SiguienteAjuste(HCoche hc){

	static int Start = -1;
	int PosInicial, LongLibre, i, Contador, Longitud;
	PACERA AceraAlg;
		
	AceraAlg = Acera[SIGUIENTE_AJUSTE];
	i = Start;
	Contador = -1;
	Longitud = Funciones.GetLongitud(hc);

	while (AceraAlg[i + 1] == FALSE && i >= 0) i--;

	while (Contador <= MAX_LONG_ROAD) {
		i = (i + 1 < MAX_LONG_ROAD) ? i + 1 : 0;
		Contador++;
		LongLibre = 0;
		while (AceraAlg[i] == FALSE && Contador <= MAX_LONG_ROAD && i < MAX_LONG_ROAD) {
			i++;
			Contador++;
			LongLibre++;
			if (LongLibre == Longitud) {
				PosInicial = i - Longitud;
				memset(AceraAlg + PosInicial, TRUE, sizeof(BOOL)*Longitud);
				Start = PosInicial - 1;

				return PosInicial;
			}
		}
	}
	return -1;
}

int MejorAjuste(HCoche hc){

	int Longitud, i, InicioActual, FinActual, InicioAnterior, FinAnterior;
	PACERA AceraAlg;
	
	AceraAlg = Acera[MEJOR_AJUSTE];
	Longitud = Funciones.GetLongitud(hc);

	i = 0;
	InicioActual = FinActual = InicioAnterior = FinAnterior = -1;

	while (i<MAX_LONG_ROAD) {
		if (AceraAlg[i] == FALSE) {
			InicioActual = i;
			while (AceraAlg[i] == FALSE && i<MAX_LONG_ROAD) { i++; }
			FinActual = i - 1;

			if (InicioAnterior == -1 && (FinActual - InicioActual + 1) >= Longitud) {
				InicioAnterior = InicioActual;
				FinAnterior = FinActual;
			}
			else if ((FinActual - InicioActual + 1) >= Longitud && (FinActual - InicioActual)<(FinAnterior - InicioAnterior)) {
				InicioAnterior = InicioActual;
				FinAnterior = FinActual;
			}
		}
		i++;
	}


	if (InicioAnterior != -1)
		memset(AceraAlg + InicioAnterior, TRUE, sizeof(BOOL)*Longitud);

	return InicioAnterior;
}

int PeorAjuste(HCoche hc){
	
	int Longitud, i, InicioActual, FinActual, InicioAnterior, FinAnterior;
	PACERA AceraAlg;

	AceraAlg = Acera[PEOR_AJUSTE];
	Longitud = Funciones.GetLongitud(hc);

	i = 0;
	InicioActual = FinActual = InicioAnterior = FinAnterior = -1;

	while (i<MAX_LONG_ROAD) {
		if (AceraAlg[i] == FALSE) {
			InicioActual = i;
			while (AceraAlg[i] == FALSE && i<MAX_LONG_ROAD) { i++; }
			FinActual = i - 1;

			if (InicioAnterior == -1 && (FinActual - InicioActual + 1) >= Longitud) {
				InicioAnterior = InicioActual;
				FinAnterior = FinActual;
			}
			else if ((FinActual - InicioActual + 1) >= Longitud && (FinActual - InicioActual)>(FinAnterior - InicioAnterior)) {
				InicioAnterior = InicioActual;
				FinAnterior = FinActual;
			}
		}
		i++;
	}


	if (InicioAnterior != -1)
		memset(AceraAlg + InicioAnterior, TRUE, sizeof(ACERA)*Longitud);

	return InicioAnterior;
}