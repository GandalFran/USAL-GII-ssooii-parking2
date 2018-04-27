// parking2.cpp: define el punto de entrada de la aplicación de consola.
//

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <Windows.h>

#define PARKING2_IMPORTS

#include "parking2.h"

#define USAGE_ERROR_MSG "Usage: parking.exe <velocidad> [D]"
#define DLL_LOAD_ERROR "DLL couldn't be loaded."
#define FUNCTION_LOAD_ERROR "function couldn't be loaded."

#define MAX_LONG_ROAD 80

#define NUMERO 0
#define LONGITUD 1
#define ALGORITMO 2
#define X 3
#define X2 4
#define Y 5
#define Y2 6

#define PRINT_ERROR(string)	fprintf(stderr, "\n[%d:%s] ERROR: %s", __LINE__, __FUNCTION__,string);	

#define EXIT_IF_NULL(returnValue,errorMsg)      \
    do{                                         \
        if((returnValue) == NULL){              \
            PRINT_ERROR(errorMsg);				\
            exit(-1);							\
        }                                       \
    }while(0)



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
//typedef BOOL ACERA, *PACERA;
typedef struct _DatosCoche {
	HCoche hc;
	PHANDLE MutexOrden;
} DATOSCOCHE, *PDATOSCOCHE;


HCoche SiguienteCoche[4] = { 0, 0, 0, 0 };

PCARRETERA Carretera[4];
PBOOL Acera[4];

struct _Funciones{
	HMODULE WINAPI ParkingLibrary;
	PARKING_Get Get[7];
	PARKING_GetDatos GetDatos;
	PARKING_Aparcar Aparcar;
	PARKING_Desaparcar Desaparcar;
	PARKING_Inicio Inicio;
	PARKING_Fin Fin;
}Funciones;


void InitFunctions();

int Aparcar(HCoche hc);
int Desaparcar(HCoche hc);
DWORD WINAPI ChoferRoutine(LPVOID lpParam);
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
		Acera[i] = (PBOOL)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BOOL) * MAX_LONG_ROAD);
		Carretera[i] = (PCARRETERA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CARRETERA) * MAX_LONG_ROAD);
		for (int j = 0; j < MAX_LONG_ROAD; j++) {
			Carretera[i][j] = CreateMutex(NULL, FALSE, NULL);
		}
	}

	for (int i = 0; i < MAX_LONG_ROAD; i++) {
		fprintf(stderr, "AceraAlg[%d] = %d\n", i, Acera[0][i]);
	}

	TIPO_FUNCION_LLEGADA FuncionesLlegada[] = { Aparcar, Aparcar, Aparcar, Aparcar };
	TIPO_FUNCION_SALIDA FuncionesSalida[] = { Desaparcar, Desaparcar, Desaparcar, Desaparcar };
	
	Funciones.Inicio(FuncionesLlegada, FuncionesSalida, Velocidad, Debug);

	Sleep(30000);

	return 0;
}
void InitFunctions(){
	EXIT_IF_NULL(Funciones.ParkingLibrary = LoadLibrary(TEXT("parking2.dll")), DLL_LOAD_ERROR);
	
	EXIT_IF_NULL(Funciones.Get[NUMERO] = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getNUmero"), FUNCTION_LOAD_ERROR);
	EXIT_IF_NULL(Funciones.Get[LONGITUD] = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getLongitud"), FUNCTION_LOAD_ERROR);
	EXIT_IF_NULL(Funciones.Get[ALGORITMO] = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getAlgoritmo"), FUNCTION_LOAD_ERROR);
	EXIT_IF_NULL(Funciones.Get[X] = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getX"), FUNCTION_LOAD_ERROR);
	EXIT_IF_NULL(Funciones.Get[X2] = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getX2"), FUNCTION_LOAD_ERROR);
	EXIT_IF_NULL(Funciones.Get[Y] = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getY"), FUNCTION_LOAD_ERROR);
	EXIT_IF_NULL(Funciones.Get[Y2] = (PARKING_Get)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getY2"), FUNCTION_LOAD_ERROR);

	EXIT_IF_NULL(Funciones.GetDatos = (PARKING_GetDatos)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_getDatos"), FUNCTION_LOAD_ERROR);

	EXIT_IF_NULL(Funciones.Inicio = (PARKING_Inicio)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_inicio"), FUNCTION_LOAD_ERROR);
	EXIT_IF_NULL(Funciones.Fin = (PARKING_Fin)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_fin"), FUNCTION_LOAD_ERROR);

	EXIT_IF_NULL(Funciones.Aparcar = (PARKING_Aparcar)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_aparcar"), FUNCTION_LOAD_ERROR);
	EXIT_IF_NULL(Funciones.Desaparcar = (PARKING_Desaparcar)GetProcAddress(Funciones.ParkingLibrary, "PARKING2_desaparcar"), FUNCTION_LOAD_ERROR);
}

//-------------------------------------------------------------------------------------------------------------------------------------

int Aparcar(HCoche hc) {
	int PosAceraAparcar;

	TIPO_FUNCION_LLEGADA Ajuste[] = { PrimerAjuste, SiguienteAjuste, MejorAjuste, PeorAjuste };

	PosAceraAparcar = Ajuste[Funciones.Get[ALGORITMO](hc)](hc);

	if (-1 != PosAceraAparcar) {
		PHANDLE PMutexOrden = (PHANDLE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(HANDLE));
		*PMutexOrden = CreateMutex(NULL, TRUE, NULL);

		PDATOSCOCHE Datos = (PDATOSCOCHE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DATOSCOCHE));
		Datos->MutexOrden = PMutexOrden;
		Datos->hc = hc;

		HANDLE nuevoThread = CreateThread(NULL, 0, ChoferRoutine, Datos, 0, NULL);
	}

	return PosAceraAparcar;
}

int Desaparcar(HCoche hc) {
	PDATOSCOCHE Datos = (PDATOSCOCHE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DATOSCOCHE));
	Datos->hc = hc;

	HANDLE nuevoThread = CreateThread(NULL, 0, DesaparcarRoutine, Datos, 0, NULL);
	return 0;
}

//-------------------------------------------------------------------------------------------------------------------------------------

DWORD WINAPI ChoferRoutine(LPVOID lpParam) {

	PDATOSCOCHE Datos = (PDATOSCOCHE)lpParam;


	HCoche CocheAnterior = SiguienteCoche[Funciones.Get[ALGORITMO](Datos->hc)];

	if (CocheAnterior != 0) {

		PDATOSCOCHE DatosAnterior = (PDATOSCOCHE)Funciones.GetDatos(CocheAnterior);

		fprintf(stderr, "[%d] Espero por el mutex de %d que es %d", __LINE__, CocheAnterior, *DatosAnterior->MutexOrden);
		DWORD WINAPI Value = WaitForSingleObject(*DatosAnterior->MutexOrden, INFINITE);
		if (Value == WAIT_FAILED) {
			fprintf(stderr, "[%d]wait failed %d", __LINE__, GetLastError());
			return 1;
		}
		fprintf(stderr, "[%d] He entrado en el mutex de %d que es %d", __LINE__, CocheAnterior, *DatosAnterior->MutexOrden);
	}

	Funciones.Aparcar(Datos->hc, Datos, AparcarCommit, PermisoAvance, PermisoAvanceCommit);

	return 0;
}

DWORD WINAPI DesaparcarRoutine(LPVOID lpParam)
{
	PDATOSCOCHE Datos = (PDATOSCOCHE)lpParam;

	Funciones.Desaparcar(Datos->hc, Datos, PermisoAvance, PermisoAvanceCommit);
	return 0;
}

//-------------------------------------------------------------------------------------------------------------------------------------

int PrimerAjuste(HCoche hc) {

	PBOOL AceraAlg = Acera[PRIMER_AJUSTE];
	int longitud;
	int posInicial, longLibre, i = -1;

	longitud = Funciones.Get[LONGITUD](hc);
	while (i < MAX_LONG_ROAD) {
		i++;
		longLibre = 0;
		while (AceraAlg[i] == FALSE && i < MAX_LONG_ROAD) {
			i++;
			longLibre++;
			if (longLibre == longitud) {
				posInicial = i - longitud;
				memset(AceraAlg + posInicial, TRUE, sizeof(BOOL) * longitud);
				return posInicial;
			}
		}
	}

	return -1;
}

int SiguienteAjuste(HCoche hc){

	static int start = -1;

	int posInicial, longLibre;
	PBOOL AceraAlg = Acera[SIGUIENTE_AJUSTE];
	int i = start;
	int contador = -1;
	int longitud = Funciones.Get[LONGITUD](hc);

	while (AceraAlg[i + 1] == FALSE && i >= 0) i--;

	while (contador <= MAX_LONG_ROAD) {
		i = (i + 1 < MAX_LONG_ROAD) ? i + 1 : 0;
		contador++;
		longLibre = 0;
		while (AceraAlg[i] == FALSE && contador <= MAX_LONG_ROAD && i < MAX_LONG_ROAD) {
			i++;
			contador++;
			longLibre++;
			if (longLibre == longitud) {
				posInicial = i - longitud;
				memset(AceraAlg + posInicial, TRUE, sizeof(BOOL)*longitud);
				start = posInicial - 1;

				return posInicial;
			}
		}
	}
	return -1;
}

int MejorAjuste(HCoche hc){

	int longitud, i, p, f, pa, fa;
	PBOOL AceraAlg;
	
	AceraAlg = Acera[MEJOR_AJUSTE];
	longitud = Funciones.Get[LONGITUD](hc);

	i = 0;
	p = f = pa = fa = -1;

	while (i<MAX_LONG_ROAD) {
		if (AceraAlg[i] == FALSE) {
			p = i;
			while (AceraAlg[i] == FALSE && i<MAX_LONG_ROAD) { i++; }
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
		memset(AceraAlg + pa, TRUE, sizeof(BOOL)*longitud);

	return pa;
}

int PeorAjuste(HCoche hc){

	int longitud, i, p, f, pa, fa;
	PBOOL AceraAlg;

	AceraAlg = Acera[PEOR_AJUSTE];
	longitud = Funciones.Get[LONGITUD](hc);

	i = 0;
	p = f = pa = fa = -1;

	while (i<MAX_LONG_ROAD) {
		if (AceraAlg[i] == FALSE) {
			p = i;
			while (AceraAlg[i] == FALSE && i<MAX_LONG_ROAD) { i++; }
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
		memset(AceraAlg + pa, TRUE, sizeof(BOOL)*longitud);

	return pa;
}

//-------------------------------------------------------------------------------------------------------------------------------------
void AparcarCommit(HCoche hc) {
	SiguienteCoche[Funciones.Get[ALGORITMO](hc)] = hc;
	PDATOSCOCHE DatosCoche = (PDATOSCOCHE)Funciones.GetDatos(hc);
	ReleaseMutex(*DatosCoche->MutexOrden);
	fprintf(stderr, "[%d]Soy %d y escribo en la variable global y release mi mutex %d", __LINE__, hc, *DatosCoche->MutexOrden);
}
void PermisoAvance(HCoche hc) {

}
void PermisoAvanceCommit(HCoche hc) {

}