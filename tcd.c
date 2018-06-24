#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

typedef struct collector_s {
	int id;						 /* identification of the collector */
	double balance;				 /* how much money this collector has */
	unsigned long inCounter;	 /* how many times this collector 'collected' */
	unsigned long outCounter;	 /* how many times this collector 'got collected' */
	pthread_mutex_t balanceLock; /* mutex for this collector */
} collector;

static double globalBalance;			/* how much money globally exists */
static unsigned long globalInCounter;	/* how many times collectors collected */
static unsigned long globalOutCounter;  /* how many times collectors got collected */
static pthread_mutex_t globalLock;		/* mutex for those global values */
static int collectorCount;			/* how many collectors exist */
static collector* collectors;		/* array of collectors */
static bool cancelPhase = false;	/* indicates if transactions are allowed or not */

/* -------------------------------------------------------------------- */
/* copied from the exercise slides------------------------------------- */
/* -------------------------------------------------------------------- */
static unsigned int hash(const void* key, size_t size)
{
	const char* ptr = key;
	unsigned int hval;
	
	for (hval = 0x811c9dc5u; size --> 0; ++ptr)
	{
		hval ^= *ptr;
		hval *= 0x1000193u;
	}
	
	return hval;
}

static unsigned long roll(unsigned int* seed, unsigned long sides)
{
	return rand_r(seed) / (RAND_MAX + 1.0) * sides;
}
/* -------------------------------------------------------------------- */

static void* collect(void* data)
{
	/* feed the random generator with the time */
	time_t now = time(NULL);
	unsigned int seed = hash(&now, sizeof(now));

	int myID = *(int*)data;

	while(1)
	{
		/* randomly choose a partner */
		int partnerID;
		while((partnerID = (roll(&seed, collectorCount))) == myID) ;

		/* check if we should end */
		pthread_testcancel();

		/* make this transaction uncancelable */
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		/* avoid deadlocks by ordering the resources */
		if(partnerID < myID)
		{
			pthread_mutex_lock(&collectors[partnerID].balanceLock);
			pthread_mutex_lock(&collectors[myID].balanceLock);
		}
		else
		{
			pthread_mutex_lock(&collectors[myID].balanceLock);
			pthread_mutex_lock(&collectors[partnerID].balanceLock);
		}

		/* check if the transaction is possible */
		if(collectors[partnerID].balance / 2 >= 100 && !cancelPhase)
		{
			/* collect the money and upgrade both blances */
			collectors[myID].balance += collectors[partnerID].balance / 2;
			collectors[partnerID].balance /= 2;

			/* save the transaction */
			collectors[partnerID].outCounter++;
			collectors[myID].inCounter++;

			/* adjust the global data */
			pthread_mutex_lock(&globalLock);

			globalInCounter++;
			globalOutCounter++;
		
			pthread_mutex_unlock(&globalLock);
		}
		
		/* unlock mutex */
		pthread_mutex_unlock(&collectors[myID].balanceLock);
		pthread_mutex_unlock(&collectors[partnerID].balanceLock);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		sched_yield();
	}

	return NULL;
}

int main(int argc, const char* argv[])
{
	double duration = 2; // default duration in seconds
	collectorCount = 5;  // default number of tax collectors
	int funds = 300;     // default funding per collector in Euro
	
	// allow overriding the defaults by the command line arguments
	switch (argc)
	{
	case 4:
		duration = atof(argv[3]);
		/* fall through */
	case 3:
		funds = atoi(argv[2]);
		/* fall through */
	case 2:
		collectorCount = atoi(argv[1]);
		/* fall through */
	case 1:
		printf(
			"Tax Collectors:  %d\n"
			"Initial funding: %d EUR\n"
			"Duration:        %g s\n",
			collectorCount, funds, duration
		);
		break;
		
	default:
		printf("Usage: %s [collectors [funds [duration]]]\n", argv[0]);
		return -1;
	}

	/* get memory for all the data */
	collectors = (collector*)malloc(sizeof(collector) * collectorCount);
	pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * collectorCount);

	/* initalize the global values */
	globalBalance = funds * collectorCount;
	globalInCounter = 0;
	globalOutCounter = 0;
	pthread_mutex_init(&globalLock, NULL);
	cancelPhase = true;

	/* set up all the collectors */
	for(int i = 0; i < collectorCount; i++)
	{
		collectors[i].id = i;
		collectors[i].balance = funds;
		collectors[i].inCounter = 0;
		collectors[i].outCounter = 0;

		pthread_mutex_init(&(collectors[i].balanceLock), NULL);
	}

	/* create all the threads */
	for(int i = 0; i < collectorCount; i++)
	{
		pthread_create(&threads[i], NULL, collect, &(collectors[i].id));
	}

	cancelPhase = false;

	/* wait for the given amount of time */
	sleep(duration);

	double balanceSum = 0;
	unsigned long inCountSum = 0, 
		      outCountSum = 0;
	cancelPhase = true;
	/* abort all the threads */
	for(int i = 0; i < collectorCount; i++)
	{
		pthread_cancel(threads[i]);
		pthread_join(threads[i], NULL);
	}
	
	/* build some stats */
	for(int i = 0; i < collectorCount; i++)
	{
		printf("Steuereintreiber %d hatte %lu-mal Geldeingang und %lu-mal Geldausgang und hat nun %f Geld.\n\n", 
			i, collectors[i].inCounter, collectors[i].outCounter, collectors[i].balance);

		/* count the data of all the collectors */
		balanceSum += collectors[i].balance;
		inCountSum += collectors[i].inCounter;
		outCountSum += collectors[i].outCounter;
	}

	/* and compare it to the global values to make sure nothing went missing due to race conditions :)) */
	printf("Systemweite Geldeingaenge: %lu (global), %lu (summiert)\n", globalInCounter, inCountSum);
	printf("Systemweite Geldausgaenge: %lu (global), %lu (summiert)\n", globalOutCounter, outCountSum);
	printf("Geld im System: %f (erwartet), %f (real)\n", globalBalance, balanceSum);

	/* free all the resources */
	for(int i = 0; i < collectorCount; i++)
	{
		pthread_mutex_destroy(&(collectors[i].balanceLock));
	}

	free(threads);
	free(collectors);
	pthread_mutex_destroy(&globalLock);

	return 0;
}
