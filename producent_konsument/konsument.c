#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

void get_args(int* argc, char** argv[], float *p, int *c, float *d)
{
        int opt;
        while ((opt=getopt(*argc, *argv, "p:c:d:")) != -1)
        {
                switch (opt)
                {
                        case 'p':
                                *p=strtof(optarg,NULL);
                                break;
			case 'c':
                                *c=strtol(optarg,NULL,10);
                                break;
			case 'd':
                                *d=strtof(optarg,NULL);
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
	int c=0;
	float d=0;

        get_args(&argc, &argv, &p, &c, &d);
        printf("p=%.3f\n",p);
	printf("c=%d\n",c);
	printf("d=%.3f\n",d);
        return 0;
}

