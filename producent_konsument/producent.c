#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <signal.h>

#define TIMER_SIG SIGUSR1
#define CLOCK_ID CLOCK_REALTIME
#define MAX_CONN 100 //ilość maksymalnej ilości połączeń

int flag=0; //flaga potrzebna do budzika (degradacji danych)

void WriteOnStdErr (char* text)
{
	fprintf(stderr, "%s\n",text);
}

//---------------

void get_args(int* argc, char** argv[], float *p, int *subst_p, char** adress_raw) //procedura pobierająca argumenty
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
			WriteOnStdErr("Wrong args!");
			exit(EXIT_FAILURE);
		}
	}
	sprintf(*adress_raw,"%s\n",(*argv)[optind]);
}

//---------------

void check_count_args(int *argc) // procedura sprawdzająca ilość podanych parametrów
{
	if ((*argc)-optind!=3)
	{
		WriteOnStdErr("Wrong number of arguments!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...");
		exit( -1);
	}
}

//---------------

void check_p_flag(int p) //procedura sprawdzająca, czy użytkownik użył flagi p
{
	if (p==0)
	{
		WriteOnStdErr("Flag '-p' is mandatory!\nUsage: ./prod -p <float> [adress:]<port> - Exiting...");
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

void one_block_production(char** buffer, char c) //produkcja jednego bloku danych
{
	for (int i=0; i<640; i++)
	{
		(*buffer)[i]=c;
	}
}

//---------------

int calc_count_of_block(float p) //wylicza ilość bloków, które będą produkowane
{
	int sum_bytes=2662*p;
	return (sum_bytes/640);
}

//---------------

void check_port (int port) //bezpiecznik
{
	if (port < 1 || port > 65535)
	{
		WriteOnStdErr ("Port must be from range 1:65535. Exiting.");
		exit(EXIT_FAILURE);
	}
}

//---------------

void rejestracja( int sockfd, char * Host, in_port_t Port ) //przygotowanie socketu - zapożyczone z materiałów z zajęć
{
	// struktura przechowująca adres w formacie dla domeny INET
	struct sockaddr_in A;

	A.sin_family = AF_INET;
	A.sin_port = htons(Port);
	A.sin_addr.s_addr = inet_addr(Host);
	int R = inet_aton(Host,&A.sin_addr);
	if( ! R )
	{
		fprintf(stderr,"niepoprawny adres: %s\n",Host);
		exit(EXIT_FAILURE);
	}

	if( bind(sockfd,(struct sockaddr *)&A,sizeof(A)) ) // ostatecznie, wywołanie funkcji
	{
		perror("bind sie zepsul\n");
		exit(EXIT_FAILURE);
	}
}

//---------------

int polaczenie( int sockfd ) //stworzenie połączenia z socketem - zapożyczone z materiałów z zajęć
{
	struct sockaddr_in peer;
	socklen_t addr_len = sizeof(peer);

	int new_socket = accept(sockfd,(struct sockaddr *)&peer,&addr_len);
	if( new_socket == -1 ) {
		perror("new socket");
	}
	return new_socket;
}

//---------------

void write_report_after_disconnect(char* adress_final, int port, int wasted_bytes)
{
	struct timespec t1;
	clock_gettime(CLOCK_REALTIME, &t1);
	fprintf(stderr, "Time: %ld:%ld\tAdress: %s\t Port: %d\t Wasted bytes: %d\n",t1.tv_sec, t1.tv_nsec, adress_final, port, wasted_bytes );
}

//---------------

static void handler()
{
	flag=1;
}

//---------------
//---------------
//---------------

int main(int argc, char* argv[])
{
	float p=0; //zmienna przechowująca wartość flagi -p
	int subst_p=0; //zmienna przechowująca wystąpienie flagi -p
	int port=0; //zmienna przechowująca nr portu
	char* adress_raw = (char*)malloc(sizeof(char)*20); //adres roboczy
	char* adress_final = (char*)malloc(sizeof(char)*16); //adres właściwy
	char* buffer_prod = (char*)malloc(sizeof(char)*640); //bufor z blokiem danych
	char* buffer_tmp = (char*)malloc(sizeof(char)*3250); //bufor służący do wysłania paczki 3250B 4*3250=13000
	struct timespec t={1,0}; //struktura dla nanosleepa
	struct epoll_event ep_ev; //struktura dla epolla
	struct epoll_event events[MAX_CONN]; //tablica struktur dla epolla
	int fd_epoll; //file descriptor używany przez epolla
	int code_char=97; //ustawienie kodu ascii na 'a'
	int pipefd[2]; //file descriptor dla pipa
	int wasted_bytes=0; //ilość zmarnowanych bajtów.
	pid_t child; //pid dziecka
	int count_bytes_in_pipe=0; //ilość bajtów w pipie
	int count_bytes_in_pipe_5s_ago=0; //ilość bajtów 5s temu
	int count_connected_clients=0;
	int max_bytes_in_magazine=65535;


	check_count_args(&argc); //sprawdzenie liczby argumentów
	get_args(&argc, &argv, &p, &subst_p, &adress_raw); //pobranie argumentów
	check_p_flag(subst_p); //sprawdzenie, czy użytkownik podał flagę -f
	split_data(adress_raw, &adress_final, &port); //podział danych z argumentu pozycyjnego, sprawdzenie czy użytkownik podał IP:port, czy port. Dopasowanie danych do zmiennych
	check_port(port); //sprawdzenie zakresów portu

	if (pipe(pipefd) == -1)
	{
		perror("pipe");
		_exit(EXIT_FAILURE);
	}

	switch(child = fork()) //podział programu na rodzica i dziecko
	{
		case -1: //ale mogło coś przecież pójść nie tak
		{
			perror("fork");
			exit(EXIT_FAILURE);
		}

		case 0: //dziecko - odbiera dane z pipa, obsługuje sockety
		{
			//stworzenie i ustawienie budzika do raportów
			struct sigaction sa;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
			sa.sa_handler = handler;
			if (sigaction(TIMER_SIG, &sa, NULL) == -1)
			{
				perror("signal");
				exit(EXIT_FAILURE);
			}
			timer_t timerid;
			struct sigevent sev;
			struct itimerspec trigger;
			sev.sigev_notify = SIGEV_SIGNAL;
			sev.sigev_signo = TIMER_SIG;
			trigger.it_value.tv_sec = 5;
			trigger.it_value.tv_nsec = 0;
			trigger.it_interval.tv_sec = 5;
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


			if (close(pipefd[1]) == -1) //zamknięcie końcówki do pisania
			{
				perror("close");
				_exit(EXIT_FAILURE);
			}

			int sock_fd = socket(AF_INET,SOCK_STREAM,0);
			if( sock_fd == -1 )
			{
				perror("creating socket");
				exit(1);
			}
			if (strcmp(adress_final, "localhost")==0)
			{
				adress_final="127.0.0.1";
			}

			rejestracja(sock_fd,"127.0.0.1",12345);
			if( listen(sock_fd,5) )
			{
				perror("zmiana trybu na pasywny sie zepsula");
				exit(1);
			}

			fd_epoll = epoll_create(MAX_CONN); //tworzenie epolla
			if (fd_epoll == -1)
			{
				perror("Creating epoll");
				exit(EXIT_FAILURE);
			}

			ep_ev.events=EPOLLIN; //ustawienie struktury dla epolla
			ep_ev.data.fd=sock_fd;
			if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, sock_fd, &ep_ev ) == -1)
			{
				perror("Epoll_ctl");
				exit (EXIT_FAILURE);
			}

			int new_socket; //zmienne potrzebne do działania epolla
			int events_count;

			while (1) //pętla główna dziecka
			{
				if (flag==1)
				{
					struct timespec t1;
					clock_gettime(CLOCK_REALTIME, &t1);
					int change=count_bytes_in_pipe-count_bytes_in_pipe_5s_ago;
					float percent=((float)count_bytes_in_pipe/(float)max_bytes_in_magazine)*100; //procent zajętych danych
					fprintf(stderr, "\n------------\nREPORT EVERY 5 SECONDS\n");
					fprintf(stderr, "Time: %ld:%ld\t\n",t1.tv_sec, t1.tv_nsec);
					fprintf(stderr, "Count of clients: %d\n",count_connected_clients);
					fprintf(stderr, "Bytes in pipe: %d B\t%.2f%%\n",count_bytes_in_pipe, percent);
					fprintf(stderr, "Change every 5 seconds: %d B\n",change );
					fprintf(stderr, "------------\n\n");
					count_bytes_in_pipe_5s_ago=count_bytes_in_pipe;
					//printf("%d\n",count_connected_clients);
					flag=0;
				}
				events_count=epoll_wait(fd_epoll, events, MAX_CONN, -1);
				if (events_count == -1) //obsługa konkretnego błędu epolla (interrupted system call zjawisko kontrolowane - w związku z budzikiem)
				{
					continue;
				}

				for (int i=0; i<events_count; i++)
				{
					if (events[i].data.fd==sock_fd)
					{
						new_socket = polaczenie(sock_fd);
						count_connected_clients++;
						if (new_socket == -1)
						{
							perror("New socket error.\n");
							exit(EXIT_FAILURE);
						}
						ep_ev.events=EPOLLOUT;
						ep_ev.data.fd=new_socket;
						if(epoll_ctl(fd_epoll, EPOLL_CTL_ADD, new_socket, &ep_ev)==-1)
						{
							perror("epoll_ctl: listen_sock");
							exit(EXIT_FAILURE);
						}
					}
				}

				ioctl(pipefd[0],FIONREAD,&count_bytes_in_pipe); //ściągnięcie informacji ile bajtów jest w pipie
				if (count_bytes_in_pipe < 13000) //jeśli w pipie jest mniej niż 13KB nie wysyłaj danych
				{
					continue;
				}

				if (read(pipefd[0], buffer_tmp, 3250)==-1) //wczytanie danych z pipe
				{
					WriteOnStdErr(strerror(errno));
				}

				if (write(new_socket, buffer_tmp, strlen(buffer_tmp))==-1)
				{
					perror("Write socket");
					exit(EXIT_FAILURE);
				}

				close(new_socket); //zamknięcie socketu po wysłaniu danych
				count_connected_clients--;
				write_report_after_disconnect(adress_final, port, wasted_bytes);
			}
			break;
		}

		default: //rodzic
		{
			if (close(pipefd[0]) == -1)
			{
				perror("close");
				exit(EXIT_FAILURE);
			}

			while (1) //pętla główna rodzica
			{
				//produkcja danych (bajtów)
				for (int i=1;i<=calc_count_of_block(p);i++)
				{
					if (code_char==123)
					{
						code_char=65;
					}
					if (code_char == 91)
					{
						code_char=97;
					}
					one_block_production(&buffer_prod, code_char);
					code_char++;
					if(	write(pipefd[1], buffer_prod, strlen(buffer_prod)) == -1)
					{
						fprintf(stderr, "%d\n", errno );
					}
				}
				nanosleep(&t,NULL); //sekunda przerwy
			}
			break;
		}
	}

	return 0;
}
