#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>


#define TIMER_SIG SIGHUP
#define CLOCK_ID CLOCK_REALTIME

int flag=0;

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

int set_capacity(int c)
{
  return c*30000;
}

int calc_used (int capacity, int free)
{
  return capacity-free;
}

int calc_free (int capacity, int used)
{
  return capacity-used;
}

int set_degradaion_tempo(float d)
{
  return 819*d;
}

void degradation_data(int* free, int* used, int degradation_tempo, int capacity)
{
  *used-=degradation_tempo;
  *free+=degradation_tempo;
  if (*used < 0)
  {
    *free=capacity;
    *used=0;
  }
}

static void handler(int signal)
{
  flag=1;
  //  printf("dupa\n");
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

  int capacity=0;
  int used=0;
  int free=0;
  int degradation_tempo=0;

  int port=0; //nr portu
  char* adress_raw = (char*)malloc(sizeof(char)*20); //adres wyjściowy, roboczy
  char* adress_final = (char*)malloc(sizeof(char)*16); //adres finalny


  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = handler;
  if (sigaction(TIMER_SIG, &sa, NULL) == -1)
  {
    perror("signal");
    exit(EXIT_FAILURE);
  }

  check_count_args(&argc); //sprawdzenie liczby argumentów
  get_args(&argc, &argv, &p, &c, &d, &subst_p, &subst_c, &subst_d, &port, &adress_raw);
  check_flags(subst_p, subst_c, subst_d); //sprawdzenie, czy użytkownik podał flagi -f
  split_data(adress_raw, &adress_final, &port); //podział danych z argumentu pozycyjnego, sprawdzenie czy użytkownik podał IP:port, czy port. Dopasowanie danych do zmiennych
  capacity=set_capacity(c);
  free=capacity;
  degradation_tempo=set_degradaion_tempo(d);
  //printf("%d\t%d\n",capacity, degradation_tempo);


  //TEST
  /*
  used=20000;
  free=capacity-used;
  printf("free: %d\t used: %d\t capacity: %d\n", free, used, capacity);
  for (int i=1; i<=20; i++)
  {
  degradation_data(&free, &used, degradation_tempo, capacity);

  printf("free: %d\t used: %d\t capacity: %d\n", free, used, capacity);
}
*/
//ENDTEST
used=20000;

//budzik!!!
timer_t timerid;
struct sigevent sev;
struct itimerspec trigger;

sev.sigev_notify = SIGEV_SIGNAL;
sev.sigev_signo = TIMER_SIG;
trigger.it_value.tv_sec = 1;
trigger.it_value.tv_nsec = 0;
trigger.it_interval.tv_sec = 1;
trigger.it_interval.tv_nsec = 0;
if (timer_create(CLOCK_ID, &sev, &timerid) < 0)
{
  perror("timer_create");
  return -11;
}

if (timer_settime(timerid, 0, &trigger, NULL))
{
  perror("timer");
  return -12;
}

int sock_fd = socket(AF_INET,SOCK_STREAM,0);
if( sock_fd == -1 )
{
  perror("socket sie zepsul...\n");
  exit(1);
}
//----------------
struct sockaddr_in A;
short Port = 12345;

A.sin_family = AF_INET;
A.sin_port = htons(Port);
// bezpieczniej:
const char * Host = "127.0.0.1";
int R = inet_aton(Host,&A.sin_addr);
if( ! R ) {
    fprintf(stderr,"niepoprawny adres: %s\n",Host);
    exit(1);
}

// 4. oczekiwanie na połączenie/akceptacja połączenia
//    (powstaje nowe gniazdo)
int proba = 11;
while( --proba ) {
    if( connect(sock_fd,(struct sockaddr *)&A,sizeof(A)) != -1 ) break;
}
if( ! proba ) {
    fprintf(stderr,"nie udało się zaakceptować połączenia\n");
    exit(2);
}
fprintf(stderr,"nawiązane połączenie z serwerem %s (port %d)\n",
    inet_ntoa(A.sin_addr),ntohs(A.sin_port));

char tmp[1024];
int dl;
dl = recv(sock_fd,tmp, 4096 ,0);
printf("dl: %d, buf: %s\n", dl,tmp);
//----------------



while (1)
{
  if (flag==1)
  {
    degradation_data(&free, &used, degradation_tempo, capacity);
    printf("free: %d\t used: %d\t capacity: %d\n", free, used, capacity);
  }
  flag=0;
}
//timer_delete(timerid);


return 0;
}
