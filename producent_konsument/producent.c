#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

void get_args(int* argc, char** argv[], float *p)
{
	int opt;
	while ((opt=getopt(*argc, *argv, "p:")) != -1)
	{
		switch (opt)
		{
			case 'p':
				*p=strtof(optarg,NULL);
				break;
			default:
				printf("Wrong args!\n");
				exit(EXIT_FAILURE);
		}
	}
}




int main(int argc, char* argv[])
{
	float p=0;

	get_args(&argc, &argv, &p);
	printf("P=%.3f\n",p);
	return 0;
}

