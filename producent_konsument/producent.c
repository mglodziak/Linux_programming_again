#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

void get_args(int* argc, char** argv[], float *p, int *subst_p, int *port, char** adress_raw) //procedura pobierająca argumenty
{
	int opt;
	while ((opt=getopt(*argc, *argv, "p:")) != -1)
	{
		switch (opt)
		{
			case 'p':
			*p=strtof(optarg,NULL);
			*subst_p=1;
			break;
			default:
			printf("Wrong args!\n");
			exit(EXIT_FAILURE);
		}
	}

	//*port=strtol((*argv)[optind], NULL, 10);
	sprintf(*adress_raw,"%s\n",(*argv)[optind]);
}



void check_count_args(int *argc) // procedura sprawdzająca ilość podanych parametrów
{
	if ((*argc)-optind!=3)
	{
		printf("Wrong number of arguments!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...\n");
		exit( -1);
	}
}

void check_p_flag(int p) //procedura sprawdzająca, czy użytkownik użył flagi p
{
	if (p==0)
	{
		printf("Flag '-p' is mandatory!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...\n");
		exit(-2);
	}
}

int isDigit(char* str) //funkcja sprawdzająca, czy podany przez użytkownika arg pozycyjny zawiera jedynie port, czy też adres (inne znaki niż liczby)
{
	for (int i=0;i<strlen(str)-1;i++)
	{
		if (str[i] >=58 || str[i]<=47)
		{
			return 1; //nie ma liczby, użytkownik podał adres
		}
	}
	return 0; //użytkownik podał sam port
}


void split_data(char* adress_raw,char** adress_final, int* port) //funcja parsuje i zwraca adres hosta i port
{
	int situation = isDigit(adress_raw);
	if (situation==0)
	{
		*adress_final="localhost";
		*port=strtol(adress_raw,NULL, 10);
	}
	if (situation==1)
	{
		*adress_final=strtok(adress_raw,":");
		*port=strtol(strtok(NULL,":"),NULL,10);
	}

}




//-------------------

int main(int argc, char* argv[])
{
	float p=0;
	int subst_p=0;
	int port=0;
	char* adress_raw = (char*)malloc(sizeof(char)*20);
	char* adress_final = (char*)malloc(sizeof(char)*16);

	check_count_args(&argc); //sprawdzenie liczby argumentów
	get_args(&argc, &argv, &p, &subst_p, &port, &adress_raw); //pobranie argumentów
	check_p_flag(subst_p); //sprawdzenie, czy użytkownik podał flagę -f
	split_data(adress_raw, &adress_final, &port);
	printf("Adres_final:%s, port:%d\n",adress_final, port);


	return 0;
}
