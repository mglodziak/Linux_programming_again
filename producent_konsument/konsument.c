#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>


void get_args(int* argc, char** argv[], float *p, int *c, float *d, int* subst_p, int* subst_c, int* subst_d, int* port, char** adress_raw)
{
  int opt;
  while ((opt=getopt(*argc, *argv, "p:c:d:")) != -1)
  {
    switch (opt)
    {
      case 'p':
      *p=strtof(optarg,NULL);
      *subst_p=1;
      break;
      case 'c':
      *c=strtol(optarg,NULL,10);
      *subst_c=1;
      break;
      case 'd':
      *d=strtof(optarg,NULL);
      *subst_d=1;
      break;
      default:
      printf("Wrong args!\n");
      exit(EXIT_FAILURE);
    }
  }
  sprintf(*adress_raw,"%s\n",(*argv)[optind]);
}
void check_count_args(int *argc) // procedura sprawdzająca ilość podanych parametrów
{
  if ((*argc)-optind!=7)
  {
    printf("Wrong number of arguments!\nUsage: ./kons -p <float> -c <int> -d <float> [adress:]<port> - Exiting...\n");
    exit( -1);
  }
}

void check_flags(int p, int c, int d) //procedura sprawdzająca, czy użytkownik użył flagi p
{
  if (p==0)
  {
    printf("Flag '-p' is mandatory!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...\n");
    exit(-2);
  }
  if (c==0)
  {
    printf("Flag '-c' is mandatory!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...\n");
    exit(-2);
  }
  if (d==0)
  {
    printf("Flag '-d' is mandatory!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...\n");
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



//----------------------------


int main(int argc, char* argv[])
{
  float p=0; //zmienne flag
  float d=0;
  int c=0;

  int subst_p=0; //zmienne, które definiują, czy flaga wystąpiła
  int subst_d=0;
  int subst_c=0;

  int port=0; //nr portu
  char* adress_raw = (char*)malloc(sizeof(char)*20); //adres wyjściowy, roboczy
  char* adress_final = (char*)malloc(sizeof(char)*16); //adres finalny

  check_count_args(&argc); //sprawdzenie liczby argumentów
  get_args(&argc, &argv, &p, &c, &d, &subst_p, &subst_c, &subst_d, &port, &adress_raw);
  check_flags(subst_p, subst_c, subst_d); //sprawdzenie, czy użytkownik podał flagi -f
  split_data(adress_raw, &adress_final, &port); //podział danych z argumentu pozycyjnego, sprawdzenie czy użytkownik podał IP:port, czy port. Dopasowanie danych do zmiennych






  printf("p=%.3f\n",p);
  printf("c=%d\n",c);
  printf("d=%.3f\n",d);
  printf("Adres_final:%s, port:%d\n",adress_final, port);
  return 0;
}
