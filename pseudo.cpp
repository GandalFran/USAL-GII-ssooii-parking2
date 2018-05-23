/*
*	@autor: Gabino Luis Lazo (i1028058)
*	@autor: Francisco Pinto Santos (i0918455)
*
*	Segunda prActica SSOO II - PARKING2
*      http://avellano.usal.es/~ssooii/PARKING/parking2.htm
*/

//	Estructura principal que guarda cada coche:
//		hc: Es el manejador opaco del coche de la dll.
//		EventOrdenAnterior: es el handle del evento del coche anterior, se espera sobre el antes de salir para asegurar el orden.
//		EventOrdenActual: es el handle de nuestro evento que mantenemos no señalado 
typedef struct _DatosCoche {
	HCoche hc;
	HANDLE EventOrdenAnterior;
	HANDLE EventOrdenActual;
} DATOSCOCHE, *PDATOSCOCHE;

//-------------------------------------------------------------------------------------------------------------------------------------

//	La carretera esta representada como Handles de Mutexes.
//		- Los mutex con propietario significa que esa casilla de carretera esta ocupada/reservada por un coche.
//		- Los mutex señalados significa que estan libres
PCARRETERA Carretera[4];

//	La acera de aparcamiento esta representada como un array booleano
//		- TRUE = Acera con coche aparcado.
//		- FALSE = Acera libre.
//
//	Usado para el calculo de las posiciones de aparcamiento en la acera.
PACERA Acera[4];

//-------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char** argv){

	...

	for (int i = PRIMER_AJUSTE; i <= PEOR_AJUSTE; i++) {
		Acera[i] = getMemoriaCompartida(...);
		Carretera[i] = getMemoriaCompartida(...);
		for (int j = 0; j < MAX_LONG_ROAD; j++) 
			Carretera[i][j] = CreateMutex( Sin poseerlo );
	}

	...

}

//-------------------------------------------------------------------------------------------------------------------------------------

int Llegada(HCoche hc) {
	
	...

	if ( Se ha encontrado sitio para aparcar ) {
		EventOrden = CreateEvent( Autoreset y no señalado );
		Datos = getMemoriaCompartida(...);

		Datos->EventOrdenActual = EventOrden;
		...
		Datos->EventOrdenAnterior = TurnoCoche[ hc.getAlgoritmo() ];
		TurnoCoche[ hc.getAlgoritmo() ] = EventoOrden;

		CreateThread( ... ,AparcarRoutine, Datos, ... );
	}

	...
}

int Salida(HCoche hc) {
	...
	CreateThread( ... ,DesaparcarRoutine, Datos, ... );
	...
}

//-------------------------------------------------------------------------------------------------------------------------------------

DWORD WINAPI AparcarRoutine(LPVOID lpParam) {
	...
	if (Datos->EventOrdenAnterior != 0) 
		wait( Datos->EventOrdenAnterior );
	...
}

//-------------------------------------------------------------------------------------------------------------------------------------

void AparcarCommit(HCoche hc) {

	...
	SetEvent( DatosCoche->EventOrdenActual );

}

void PermisoAvance(HCoche hc) {
	PCARRETERA Carr = Carretera[ hc.GetAlgoritmo() ];

	if ( esta en carretera y la posicion a avanzar tambien lo esta ) {
		wait( Carr[ hc.getX2() ] );
	}
	else if ( esta desaparcando ){
		for (int i = hc.getX() + hc.getLongitud() - 1; i >= hc.getX(); i--) 
			wait( Carr[i] );
	}

}

void PermisoAvanceCommit(HCoche hc) {
	PCARRETERA Carr = Carretera[ hc.GetAlgoritmo() ];

	if ( esta en carretera y la ultima posicion tambien lo esta ){
		ReleaseMutex( Carr[ hc.getX() + hc.getLongitud() ] );
	}
	else if ( esta desaparcando ){
		liberar acera
	}
	else if ( esta aparcando ){
		for (int i = hc.getX(); i < hc.getX() + hc.getLongitud(); i++)
			ReleaseMutex( Carr[i] );
	}
}