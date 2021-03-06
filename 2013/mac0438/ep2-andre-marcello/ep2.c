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
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

  /****************************************/
 /* Inicialização das variáveis globais. */
/****************************************/

short int param;                                      /* Variável que guarda qual dos modos de cálculo será utilizado.                        */
int numCPU;                                           /* Número de núcleos do computador onde o processo esta rodando.                        */
long int impreciso;                                   /* Variável usada para indicar que a precisão foi ou não atingida.                      */
long int iteracoes;                                   /* Número de vezes que as threads se encontraram na barreira.                           */

long double *termos;                                  /* Vetor para guardar o resultado das parcelas de cada termo do algoritmo de bellard.   */
long double pi;                                       /* Variável para guardar o valor de pi para a máxima precisão da linguagem.             */
long double precisao;                                 /* Pecisão do cálculo.                                                                  */


/* Variáveis para calculos acumulados. */
long double p2;                                       /* Variável que acumula parte do cálculo que contém potências de 2.                     */
long double m4;                                       /* Variável que acumula parte do cálculo que contém multiplos de 4.                     */
long double m10;                                      /* Variável que acumula parte do cálculo que contém multiplos de 10.                    */
unsigned long int n;                                  /* Número do termo atual a ser calculado.                                               */

/* Variaveis auxiliares de controle de fluxo. */
int *chegada;                                         /* Vetor para guardar a posição de chegada das threads na barreira.                     */
int Flag = 0;                                         /* Barreira de sincronização das threads.                                               */
int processos;                                        /* Numero de processos concluidos.                                                      */

/* Semaforos. */
sem_t sem_writeread;                                  /* Semaforo para proteger a leitura e a escrita dos termos précalculados.               */

  /*************************************************/
 /* Termino da declaração das variáveis globais.  */
/*************************************************/


/*pthread_mutex_t meu_mutex = PTHREAD_MUTEX_INITIALIZER;*/

void *mallocX (unsigned int nbytes) {
   void *ptr;
   ptr = malloc (nbytes);
   if (ptr == NULL) {
      printf ("Socorro! malloc devolveu NULL! (%d)\n", nbytes);
      exit (EXIT_FAILURE);
   }
   return ptr;
}

long double bellard(
	long int ln,
	long double lm4,
	long double lm10,
	long double lp2
) {
	long double
		termo = 0;

	termo += (long double) -32 / (lm4+1);
	termo += (long double)  -1 / (lm4+3);
	termo += (long double) 256 / (lm10+1);
	termo += (long double) -64 / (lm10+3);
	termo += (long double)  -4 / (lm10+5);
	termo += (long double)  -4 / (lm10+7);
	termo += (long double)   1 / (lm10+9);
	termo /= lp2;

	return (ln%2) ? -termo : termo;
}

/* Calcula o enésimo termo acumulando os resultados diretamente
 * em pi e sem a necessidade de uso de barreuras.
 ******************************************************************************************************/
void *calculaTermo_nl( void *t) {
	long int ln;
	long double lm4, lm10, lp2;
	long double termo = 0;

	/* Continua calculando enquanto a precisão não tenha sido atingida anteriormente
	 * por nenhuma thread de um termo menor que a atual.
	 **************************************************************************************************/
	do {
		sem_wait( &sem_writeread);
		ln = n++;
		lm4 = m4; m4 += 4;
		lm10 = m10; m10 += 10;
		lp2 = p2; p2 *= 1024;
		sem_post( &sem_writeread);

		if( impreciso!=-1 && ln>impreciso)
			break;

		termo = bellard( ln, lm4, lm10, lp2);
		pi += termo;
	} while ( fabs(termo)>precisao && impreciso==-1 );
	
	if(impreciso==-1)
		impreciso = ln;

	return NULL;
}


void *gerencia(void *p) {
	int i;

	while ( impreciso ) {
		iteracoes++;
		
		while(processos<numCPU) printf("\r%d - %ld            ", processos, impreciso);
		
		if (param==1)
			printf("\nSincronizando threads!\nChegada:\n");

		for( i=0; i<numCPU; i++) {
			if (param==1)
				printf("%d ", chegada[i]);
			pi += termos[i];
			
			if ( fabs(termos[i])<=precisao )
				impreciso = 0;
			termos[i] = 0;
		}

		processos = 0;
		Flag = (Flag+1)%2;
	}

	return NULL;
}

/* Calcula o enésimo termo acumulando os resultados em um vetor
 * depois que todos os processos terminan o valor de pi é atualizado.
 ******************************************************************************************************/
void *calculaTermo( int t) {
	int p = t;
	long int ln = 0;
	long double lm4, lm10, lp2;
	int flag = 0;

	/* Continua calculando enquanto a precisão não tenha sido atingida anteriormente
	 * por nenhuma thread de um termo menor que a atual.
	 ***********************************************************************/
	do {
		flag = (flag+1)%2;
		/* Acessando a sessão crítica. */
		sem_wait( &sem_writeread );
		ln = n++;
		lm4 = m4; m4 += 4;
		lm10 = m10; m10 += 10;
		lp2 = p2; p2 *= 1024;
		sem_post( &sem_writeread);

		/* Gardando o termo calculado em seu devido lugar.
		 ************************************************************************/
		termos[p] = bellard( ln, lm4, lm10, lp2);
		chegada[processos++] = p;

		/* Todos os processos ficam travados até que o gerenciador faça a sincronização adequada.
		 ********************************************************************************************/
		while( flag!=Flag && impreciso ) printf("\r");
	} while ( impreciso );

	return NULL;
}

/* Função principal.
 *******************/
int main(int argc, char *argv[]){
	int
		i,
		t;
	
	struct timespec tps, tpe;

	if ( (clock_gettime(1, &tps) != 0) ) {
	    perror("clock_gettime");
	    return -1;
	  }

	pthread_t
		*threads = (pthread_t *) mallocX( numCPU * sizeof (pthread_t)),
		gerente;


	/* Inicializando variaveis globais.
	 *********************************/
	numCPU = sysconf( _SC_NPROCESSORS_ONLN );
	param = 0;
	iteracoes = 0;
	impreciso = -1;
	processos = 0;
	pi = 0;
	n = 0;
	m4 = 0;
	m10 = 0;
	p2 = 64;

	printf( "Utilizando %d nucleos.\n\n", numCPU);

	/* Lendo os parametros de entrada.
	 *********************************/
	if(argc<2) {
		printf(
			"Modo de uso:\n"
			"%s [DEBUG] [SEQUENCIAL] [UNLIMITED] <f>\n"
			"\tDEBUG: para rodar em modo de depuracao;\n"
			"\tSEQUENCIAL: rodar sem o uso de threads;\n"
			"\tUNLIMITED: rodar sem ter que parar na barreira;\n"
			"\t<f>: precisao do calculo.\n", argv[0]
		);
	}
	else {
		for(i=1; i<argc; i++) {
			if( strcmp(argv[i], "DEBUG")==0 )
				param = 1;
			else if (strcmp(argv[i], "SEQUENCIAL")==0 )
				param = 2;
			else if (strcmp(argv[i], "UNLIMITED")==0 )
				param = 3;
			sscanf(argv[i], "%Lf", &precisao);
		}
	}

	switch(param){
		case 2:
			/* SEQUENCIAL */
			termos = (long double*) mallocX(sizeof(long double));
			n = 0;
			do{
				*termos = bellard( n, m4, m10, p2 );
				pi += *termos;
				printf("%.40Lf\t[%g]\n", pi, (double) *termos);
				n++;
				m4 += 4;
				m10 += 10;
				p2 *= 1024;
			} while( fabs(*termos)>precisao && p2 != 0 );

			printf("\nO resultado de pi obtido com %ld termos calculados de maneira sequencial foi:\n%.40Lf\n", n, pi);
			break;
		case 3:
			/* UNLIMITED */
			sem_init( &sem_writeread, 0, 1);
			for (t = 0; t < numCPU; ++t)
				pthread_create( &threads[t], NULL, calculaTermo_nl, (void *) &t);
			for (t = 0; t < numCPU; ++t)
				pthread_join( threads[t], NULL);
			sem_destroy( &sem_writeread);
			printf("\nO resultado de pi obtido usando %d processos foi:\n%.40Lf\n", numCPU, pi);
			printf("Foram calculados %ld termos.\n", n);
			break;
		default:
			/* DEBUG */
			/* NORMAL */

			termos = (long double*) mallocX( numCPU*sizeof(long double) );
			chegada = (int*) mallocX( numCPU*sizeof(int) );

			pthread_create( &gerente, NULL, gerencia, NULL);

			sem_init( &sem_writeread, 0, 1);
			for (t = 0; t < numCPU; t++) {
				termos[t] = 0;
				pthread_create( &threads[t], NULL, calculaTermo, t);
			}

			for (t = 0; t < numCPU; ++t)
				pthread_join( threads[t], NULL);
			pthread_join( gerente, NULL);
			
			sem_destroy( &sem_writeread);
			printf("O resultado de pi obtido usando %d processos foi:\n%.40Lf\n", numCPU, pi);
			printf("As threads se encontraram %ld vezes na sincronizacao\n", iteracoes);
			break;
	}

	if ( (clock_gettime(1, &tpe) != 0)) {
		perror("clock_gettime");
		return -1;
	}
	printf("\ntempo: %lu s, %lu ns\n", tpe.tv_sec-tps.tv_sec, tpe.tv_nsec-tps.tv_nsec);

	return 0;
}