#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

void get_args(int* argc, char** argv[], float *p, int *subst_p, int *port, char** adress)
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
sprintf(*adress,"%s\n",(*argv)[optind]);
}


void check_count_args(int* argc)
{
	if ((*argc)-optind!=3)
	{
		printf("Usage: ./prod -p <float> [adress:]<port> - Exiting...\n");
		exit( -1);
	}
}



int main(int argc, char* argv[])
{
	float p=0;
	int subst_p=0;
	int port=0;

	char* adress = (char*)malloc(sizeof(char)*20);


  check_count_args(&argc);
	get_args(&argc, &argv, &p, &subst_p, &port, &adress);
	printf("P=%.3f\n",p);
  printf("subst_p=%d\n",subst_p);
  printf("port=%d\n",port);
	printf("address=%s\n",adress);

	return 0;
}
