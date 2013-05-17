/*******
 * ep2.c
 ******/
/*******************************************************************************
 * Alunos: André Meneghelli Vale,      Núm. USP: 4898948
 *         Marcello Souza de Oliveira, Núm. USP: 6432692
 * Curso: Bacharelado em Ciências da Computação
 * EP 02 -- Calculo do valor aproximado de PI.
 * MAC0438 -- 17/04/2013 -- IME/USP, -- Prof. Daniel Macêdo Batista
 * Compilador: gcc linux 4.6.3
 * Editor: Sublime Text 2;
 * Sistema Operacional: Linux
 ******************************************************************************/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h> /* Para trabalhar com threads */
#include <semaphore.h>
#include <string.h>
#include <unistd.h>

#include "precisao.h"

  /****************************************/
 /* Inicialização das variáveis globais. */
/****************************************/

char param;

/*
sem_t sem_estrada[CITAM];
*/
  /*************************************************/
 /* Termino da declaração das variáveis globais.  */
/*************************************************/

/* Imprime a classificação dos atletas.
 **************************************/

/* Função principal.
 *******************/
int main(int argc, char *argv[]){
	int
		i,
		j,
		n,
		numCPU = sysconf( _SC_NPROCESSORS_ONLN );
	double d, s=0, d2;
	SuperLong precisao;
	unsigned char c = 37;



	for (i = 0; i < 32; ++i){
		d  = 1000000000 * ( 4.0/(8*i+1) - 2.0/(8*i+4) - 1.0/(8*i+5) - 1.0/(8*i+6) )/pow(16, i);
		
		d2  = ( 4.0/(16*i+1) - 2.0/(16*i+4)  - 1.0/(16*i+5)  - 1.0/(16*i+6 ) );
		d2 += ( 4.0/(16*i+9) - 2.0/(16*i+12) - 1.0/(16*i+13) - 1.0/(16*i+14) )/16;
		
		d2 *= 256;

		/*
		d2 = 63/d2;
		d2 *= pow(256, i+1);
		*/
		c = (char) d2;

		printf("\n\t%d\t||\t %.15lf\n", c, d2);

		for(j=0; j<8; j++) {
			if(c%2)
				printf("1");
			else
				printf("0");
			c = c>>1;
		}
		printf("\t%.15lf\n", d2);


		s += d;
		/*
		printf("%.160lf\n", d);

		printf("%d:\t %.160lf\n", i, d);
		*/
	}
	printf("FIM:\t %.160lf\n\n\n", i, s);

	return 0;

	for(i=0; i<8; i++) {
		if(c%2)
			printf("1");
		else
			printf("0");
		c = c>>1;
	}

	printf("\n");
	return 0;

	/* Inicializando variaveis globais. */
	param = 0;

	if(argc<2) {
		printf(
			"Modo de uso:\n"
			"%s DEBUG SEQUENCIAL <f>\n"
			"DEBUG: para rodar em modo de depuracao.\n"
			"SEQUENCIAL: rodar sem o uso de threads.\n"
			"<f>: precisao do calculo.\n", argv[0]
		);
	}
	else {
		for(i=1; i<argc; i++) {
			if( strcmp(argv[i], "DEBUG")==0 )
				param = 1;
			else if (strcmp(argv[i], "SEQUENCIAL")==0 )
				param = 2;
			readSuperLong(&precisao, argv[i], 10);
		}
		printf("\n");
		printSuperLong(precisao);
		printf("\n");
	}

	return 0;
}