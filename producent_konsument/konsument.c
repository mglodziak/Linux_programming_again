#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>


#define TIMER_SIG SIGHUP
#define CLOCK_ID CLOCK_REALTIME

int flag=0; //flaga potrzebna do budzika (degradacji danych)

void WriteOnStdErr (char* text)
{
  fprintf(stderr, "%s\n",text);
}

//---------------

void get_args(int* argc, char** argv[], float *p, int *c, float *d, int* subst_p, int* subst_c, int* subst_d, char** adress_raw)
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
      WriteOnStdErr("Wrong args!");
      exit(EXIT_FAILURE);
    }
  }
  sprintf(*adress_raw,"%s\n",(*argv)[optind]);
}

//---------------

void check_count_args(int *argc) // procedura sprawdzająca ilość podanych parametrów
{
  if ((*argc)-optind!=7)
  {
    WriteOnStdErr("Wrong number of arguments!\nUsage: ./kons -p <float> -c <int> -d <float> [adress:]<port> - Exiting...");
    exit( -1);
  }
}

//---------------

void check_flags(int p, int c, int d) //procedura sprawdzająca, czy użytkownik użył flagi p
{
  if (p==0)
  {
    WriteOnStdErr("Flag '-p' is mandatory!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...");
    exit(-2);
  }
  if (c==0)
  {
    WriteOnStdErr("Flag '-c' is mandatory!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...");
    exit(-2);
  }
  if (d==0)
  {
    WriteOnStdErr("Flag '-d' is mandatory!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...");
    exit(-2);
  }
}

//---------------

int isDigit(char* str) //funkcja sprawdzająca, czy podany przez użytkownika arg pozycyjny zawiera jedynie port, czy też adres (inne znaki niż liczby)
{
  for (int i=0;i<(int)strlen(str)-1;i++)
  {
    if (str[i] >=58 || str[i]<=47)
    {
      return 1; //nie ma liczby, użytkownik podał adres
    }
  }
  return 0; //użytkownik podał sam port
}

//---------------

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

//---------------

int set_capacity(int c)
{
  return c*30000;
}

//---------------

int calc_used (int capacity, int free)
{
  return capacity-free;
}

//---------------

int calc_free (int capacity, int used)
{
  return capacity-used;
}

//---------------

int set_degradaion_tempo(float d)
{
  return 819*d;
}

//---------------

int set_consumption_tempo(float p)
{
  return 4435*p;
}

//---------------

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

//---------------

static void handler()
{
  flag=1;
}

//---------------

void write_report_on_exit(char* adress_final, int port, int tab1[], int tab2[] )
{
  struct timespec t1; //Do wzięcia czasu TS

  clock_gettime(CLOCK_REALTIME, &t1);
  pid_t pid = getpid();
  fprintf(stderr, "Time: %ld:%ld\tPID: %d\t Adress: %s\t Port: %d\t\n",t1.tv_sec, t1.tv_nsec, pid, adress_final, port);
  for (int i=0; i<10; i++)
  {
    fprintf(stderr, "Pakiet 13KB nr %d: Opóźnienie między połączeniem a odebraniem pierwszej porcji: %d ns\n",i+1, tab1[i] );
    fprintf(stderr, "Pakiet 13KB nr %d: Opóźnienie między odebraniem pierwszej porcji a zamknięciem połączenia: %d s\n",i+1, tab2[i] );
  }
}

//---------------
//---------------
//---------------

int main(int argc, char* argv[])
{
  float p=0; //zmienne parametrów (flag)
  float d=0;
  int c=0;
  int subst_p=0; //zmienne, które definiują, czy flaga wystąpiła
  int subst_d=0;
  int subst_c=0;
  int capacity=0; //pojemność magazynu
  int used=0; //ilość użytego miejsca magazynu
  int free=0; //ilość wolnego miejsca magazynu
  int degradation_tempo=0; //tempo degradacji
  int consumpion_tempo=0; //tempo konsumpcji danych
  int count_connections=0; // licznik połączeń
  int port=0; //nr portu
  int late_recive[10]={0}; //tablica przechowująca opóźnienia mięzy połączeniem, a odebraniem pierwszej porcji produktu
  int late_closed[10]={0}; //tablica przechowująca opóźnienia między pierwszą porcją, a zamknięciem połączenia
  int ret=0; //wartość, którą zwróci recv
  int conn=0; //flaga określająca, czy mamy istniejące połączenie, czy trzeba nawiązać nowe
  int sock_fd; //file descriptor socketu
  int R;
  char* adress_raw = (char*)malloc(sizeof(char)*20); //adres wyjściowy, roboczy
  char* adress_final = (char*)malloc(sizeof(char)*16); //adres finalny

  struct timespec t2; //do opóźnień clock monotonic
  struct timespec t3;
  struct timespec t4; //do opóźnień clock monotonic
  struct timespec t5;
  struct sockaddr_in A;
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
  get_args(&argc, &argv, &p, &c, &d, &subst_p, &subst_c, &subst_d, &adress_raw); //pobranie argumentów
  check_flags(subst_p, subst_c, subst_d); //sprawdzenie, czy użytkownik podał flagi -f
  split_data(adress_raw, &adress_final, &port); //podział danych z argumentu pozycyjnego, sprawdzenie czy użytkownik podał IP:port, czy port. Dopasowanie danych do zmiennych
  capacity=set_capacity(c); //ustawienie pojemności magazynu
  free=capacity;
  degradation_tempo=set_degradaion_tempo(d); //ustawienie tempa degradacji danych
  consumpion_tempo=set_consumption_tempo(p);

  free=capacity-used;

  //budzik
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


  while (1) //główna pętla programu
  {
    if (free<13000) //zabezpieczenie przed przepełnieniem magazynu
    {
      //WriteOnStdErr("Nie mam miejsca w magazynie, kończę działanie.");
      write_report_on_exit(adress_final, port, late_recive, late_closed);
      exit(0);
    }

    if (ret > 0) //degradacja danych i jednoczesne przetwarzanie
    {
      if (flag==1) //jeśli budzik wysłał sygnał - następuje degradacja i konsumpcja danych w tempie zadanym przez parametr
      {
        if (ret<consumpion_tempo)
        {
          used+=ret;
          free-=ret;
          ret=0;
        }
        else
        {
          used+=consumpion_tempo;
          free-=consumpion_tempo;
          ret-=consumpion_tempo;
        }
        degradation_data(&free, &used, degradation_tempo, capacity);
        //  printf("free: %d\t used: %d\t capacity: %d\n", free, used, capacity);
      }
      flag=0;
      continue;
    }

    if (conn==0) //jeśli trzeba pobrać paczkę danych - stworzyć połączenie
    {
      sock_fd = socket(AF_INET,SOCK_STREAM,0);
      if( sock_fd == -1 )
      {
        perror("socket sie zepsul...\n");
        exit(1);
      }
      if (strcmp(adress_final, "localhost")==0)
      {
        adress_final="127.0.0.1";
      }

      A.sin_family = AF_INET;
      A.sin_port = htons(port);
      R = inet_aton(adress_final,&A.sin_addr);
      if( ! R )
      {
        fprintf(stderr,"niepoprawny adres: %s\n",adress_final);
        exit(1);
      }

      if( connect(sock_fd,(struct sockaddr *)&A,sizeof(A)) != -1 )
      {
        conn=1;
        if (count_connections%4==0)
        {
          clock_gettime(CLOCK_MONOTONIC, &t2); //połączenie
        }
      }
    }

    char tmp[3250]; //bufor do otrzymania danych - 4x3250=13000. Najmniejsza ilość paczek, która nie przekracza 4KB, aby przesłać 13KB
    ret = recv(sock_fd,tmp, 3250 ,0); //otrzymanie paczki danych - chcę pobrać 4KB
    if (count_connections%4==0)
    {
      clock_gettime(CLOCK_MONOTONIC, &t3); //po otrzymaniu porcji
      clock_gettime(CLOCK_MONOTONIC, &t4);
      late_recive[count_connections/4]=t3.tv_nsec-t2.tv_nsec;
    }
    if (count_connections%4==3) //na koniec połączenia
    {
      clock_gettime(CLOCK_MONOTONIC, &t5);
      late_closed[(count_connections-3)/4]=t5.tv_sec-t4.tv_sec;
    }

    count_connections++;
    if (ret==-1)
    {
      continue;
    }
    used+=ret;
    free-=ret;

    conn=0;
    if (flag==1) //jeśli budzik wysłał sygnał - następuje degradacja danych w tempie zadanym przez parametr
    {
      degradation_data(&free, &used, degradation_tempo, capacity);
    }
    flag=0;
  }
  timer_delete(timerid);

  return 0;
}
