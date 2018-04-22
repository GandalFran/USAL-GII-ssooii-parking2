// parking2.cpp: define el punto de entrada de la aplicación de consola.
//

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <Windows.h>

#define PARKING2_EXPORTS

#include "parking2.h"

#define USAGE_ERROR_MSG "Usage: parking.exe <velocidad> [D]"
#define MAX_LONG_ROAD 80

typedef int(*PARKING_inicio)(TIPO_FUNCION_LLEGADA *, TIPO_FUNCION_SALIDA *, long, int);
typedef int(*PARKING_aparcar)(HCoche, void *datos, TIPO_FUNCION_APARCAR_COMMIT,
												   TIPO_FUNCION_PERMISO_AVANCE,
												   TIPO_FUNCION_PERMISO_AVANCE_COMMIT);

typedef HCoche* PHCoche;
typedef HANDLE* CARRETERA;
typedef struct _DatosCoche {
	HCoche hc;
	CARRETERA Carretera[MAX_LONG_ROAD];
} DATOSCOCHE, *PDATOSCOCHE;

void AparcarCommit(HCoche hc) {}
void PermisoAvance(HCoche hc) {}
void PermisoAvanceCommit(HCoche hc) {}

HMODULE WINAPI ParkingLibrary;

DWORD WINAPI ChoferRoutine(LPVOID lpParam) {

	PDATOSCOCHE Datos = (PDATOSCOCHE)lpParam;

	PARKING_aparcar aparcar = (PARKING_aparcar)GetProcAddress(ParkingLibrary, "PARKING2_aparcar");
	if (aparcar == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		return 1;
	}

	aparcar(Datos->hc, Datos, AparcarCommit, PermisoAvance, PermisoAvanceCommit);

	return 0;
}

int PrimerAjuste(HCoche hc)
{
	static int asd = 0;
	PDATOSCOCHE Datos = (PDATOSCOCHE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DATOSCOCHE));
	Datos->hc = hc;

	HANDLE nuevoThread = CreateThread(NULL, 0, ChoferRoutine, Datos, 0, NULL);
	if (asd == 0) {
		asd++;
		return 0;
	}
	return -2;
}

int SiguienteAjuste(HCoche hc)
{
	return -2;
}

int MejorAjuste(HCoche hc)
{
	return -2;
}

int PeorAjuste(HCoche hc)
{
	return -2;
}

int Desaparcar(HCoche hc)
{
	return 0;
}

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

	ParkingLibrary = LoadLibrary(TEXT("parking2.dll"));
	if (ParkingLibrary == NULL) {
		fprintf(stderr, "La biblioteca no se ha podido cargar correctamente. Abortando... %d", GetLastError());
		return 1;
	}

	TIPO_FUNCION_LLEGADA FuncionesLlegada[] = { PrimerAjuste, SiguienteAjuste, MejorAjuste, PeorAjuste };
	TIPO_FUNCION_SALIDA FuncionesSalida[] = { Desaparcar };

	PARKING_inicio init = (PARKING_inicio)GetProcAddress(ParkingLibrary, "PARKING2_inicio");
	if (init == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		return 1;
	}
	
	init(FuncionesLlegada, FuncionesSalida, Velocidad, Debug);

	Sleep(20000);

	return 0;
}

