// parking2.cpp: define el punto de entrada de la aplicación de consola.
//

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <Windows.h>

#define PARKING2_IMPORTS

#include "parking2.h"

#define USAGE_ERROR_MSG "Usage: parking.exe <velocidad> [D]"
#define MAX_LONG_ROAD 80

typedef int(*PARKING_Inicio)(TIPO_FUNCION_LLEGADA *, TIPO_FUNCION_SALIDA *, long, int);
typedef int(*PARKING_Aparcar)(HCoche, void *datos, TIPO_FUNCION_APARCAR_COMMIT,
												   TIPO_FUNCION_PERMISO_AVANCE,
												   TIPO_FUNCION_PERMISO_AVANCE_COMMIT);
typedef int(*PARKING_Get)(HCoche);
typedef void*(*PARKING_GetDatos)(HCoche);

typedef HCoche* PHCoche;
typedef HANDLE CARRETERA, *PCARRETERA;
typedef BOOL ACERA, *PACERA;
typedef struct _DatosCoche {
	HCoche hc;
	PHANDLE MutexOrden;
} DATOSCOCHE, *PDATOSCOCHE;

HMODULE WINAPI ParkingLibrary;
HCoche SiguienteCoche[4] = { 0, 0, 0, 0 };
PCARRETERA Carretera[4];
PACERA Acera[4];
PARKING_Get FuncionesGet[8];
PARKING_Aparcar FuncionAparcar;
PARKING_Inicio FuncionInicio;
PARKING_GetDatos FuncionGetDatos;

void AparcarCommit(HCoche hc) {
	PARKING_Get GetAlgoritmo = (PARKING_Get)GetProcAddress(ParkingLibrary, "PARKING2_getAlgoritmo");
	if (GetAlgoritmo == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		exit(1);
	}
	PARKING_GetDatos GetDatos = (PARKING_GetDatos)GetProcAddress(ParkingLibrary, "PARKING2_getDatos");
	if (GetDatos == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		exit(1);
	}

	SiguienteCoche[GetAlgoritmo(hc)] = hc;
	PDATOSCOCHE DatosCoche = (PDATOSCOCHE)GetDatos(hc);
	ReleaseMutex(*DatosCoche->MutexOrden);
}
void PermisoAvance(HCoche hc) {}
void PermisoAvanceCommit(HCoche hc) {}

DWORD WINAPI ChoferRoutine(LPVOID lpParam) {

	PDATOSCOCHE Datos = (PDATOSCOCHE)lpParam;

	PARKING_Get GetNumero = (PARKING_Get)GetProcAddress(ParkingLibrary, "PARKING2_getNUmero");
	if (GetNumero == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		return 1;
	}
	PARKING_Get GetAlgoritmo = (PARKING_Get)GetProcAddress(ParkingLibrary, "PARKING2_getAlgoritmo");
	if (GetAlgoritmo == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		return 1;
	}

	PARKING_GetDatos GetDatos = (PARKING_GetDatos)GetProcAddress(ParkingLibrary, "PARKING2_getDatos");
	if (GetDatos == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		return 1;
	}

	HCoche CocheAnterior = SiguienteCoche[GetAlgoritmo(Datos->hc)];
	if (CocheAnterior != 0) {

		PDATOSCOCHE DatosAnterior = (PDATOSCOCHE)GetDatos(CocheAnterior);

		DWORD WINAPI Value = WaitForSingleObject(*DatosAnterior->MutexOrden, INFINITE);
		if (Value == WAIT_FAILED) {
			fprintf(stderr, "Error al hacer wait sobre el mutex del coche anterior. Abortando... %d", GetLastError());
			return 1;
		}
	}

	PARKING_Aparcar Aparcar = (PARKING_Aparcar)GetProcAddress(ParkingLibrary, "PARKING2_aparcar");
	if (Aparcar == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		return 1;
	}

	Aparcar(Datos->hc, Datos, AparcarCommit, PermisoAvance, PermisoAvanceCommit);

	return 0;
}

int PrimerAjuste(HCoche hc)
{
	PARKING_Get GetLongitud = (PARKING_Get)GetProcAddress(ParkingLibrary, "PARKING2_getLongitud");
	if (GetLongitud == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		exit(1);
	}

	PARKING_Get GetAlgoritmo = (PARKING_Get)GetProcAddress(ParkingLibrary, "PARKING2_getAlgoritmo");
	if (GetAlgoritmo == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		exit(1);
	}

	PACERA AceraAlg = Acera[GetAlgoritmo(hc)];
	int longitud;
	int posInicial, longLibre, i = -1;

	longitud = GetLongitud(hc);
	while (i < MAX_LONG_ROAD) {
		i++;
		longLibre = 0;
		fprintf(stderr, "\nAceraAlg[%d] = %d",i, AceraAlg[i]);
		while (AceraAlg[i] == FALSE && i < MAX_LONG_ROAD) {
			i++;
			longLibre++;
			if (longLibre == longitud) {
				posInicial = i - longitud;
				memset(AceraAlg + posInicial, TRUE, sizeof(ACERA) * longitud);
				i = INFINITE;
			}
		}
	}
	fprintf(stderr, "\n\nLong = %d\n\n", longitud);
	fprintf(stderr, "\n\ni = %d\n\n", i);
	if (i == INFINITE) {
		PHANDLE PMutexOrden = (PHANDLE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(HANDLE));
		*PMutexOrden = CreateMutex(NULL, TRUE, NULL);

		PDATOSCOCHE Datos = (PDATOSCOCHE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DATOSCOCHE));
		Datos->MutexOrden = PMutexOrden;
		Datos->hc = hc;

		HANDLE nuevoThread = CreateThread(NULL, 0, ChoferRoutine, Datos, 0, NULL);

		return posInicial;
	}
	else {
		return -1;
	}
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

	for (int i = PRIMER_AJUSTE; i <= PEOR_AJUSTE; i++) {
		Acera[i] = (PACERA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ACERA) * MAX_LONG_ROAD);
		Carretera[i] = (PCARRETERA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CARRETERA) * MAX_LONG_ROAD);
		for (int j = 0; j < MAX_LONG_ROAD; j++) {
			Carretera[i][j] = CreateMutex(NULL, FALSE, NULL);
		}
	}

	TIPO_FUNCION_LLEGADA FuncionesLlegada[] = { PrimerAjuste, SiguienteAjuste, MejorAjuste, PeorAjuste };
	TIPO_FUNCION_SALIDA FuncionesSalida[] = { Desaparcar, Desaparcar, Desaparcar, Desaparcar };

	PARKING_Inicio init = (PARKING_Inicio)GetProcAddress(ParkingLibrary, "PARKING2_inicio");
	if (init == NULL) {
		fprintf(stderr, "No se ha cargado la funcion correctamente. Abortando... %d", GetLastError());
		return 1;
	}
	
	init(FuncionesLlegada, FuncionesSalida, Velocidad, Debug);

	Sleep(30000);

	return 0;
}

