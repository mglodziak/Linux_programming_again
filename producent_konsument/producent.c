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


#define MAX_CONN 100 //ilość maksymalnej ilości połączeń

void WriteOnStdErr (char* text)
{
fprintf(stderr, "%s\n",text);
}

//---------------

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
	for (int i=0;i<strlen(str)-1;i++)
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
		//	*adress_final="localhost";
		*adress_final="127.0.0.1"; //był localhost, ale do bindowania potrzeba adresu w formie 'cyfrowej'
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
		perror("");
	}
	else
	{
		fprintf(stderr,"nawiązane połączenie z klientem %s (port %d)\n",
		inet_ntoa(peer.sin_addr),ntohs(peer.sin_port));
	}
	return new_socket;
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
	char* buffer_tmp = (char*)malloc(sizeof(char)*4000); //bufor służący do wysłania paczki 4KB
	struct timespec t={1,0}; //struktura dla nanosleepa
	struct epoll_event ep_ev; //struktura dla epolla
	struct epoll_event events[MAX_CONN]; //tablica struktur dla epolla
	int fd_epoll; //file descriptor używany przez epolla
	int code_char=97; //ustawienie kodu ascii na 'a'
	int pipefd[2]; //file descriptor dla pipa
	pid_t child; //pid dziecka

	check_count_args(&argc); //sprawdzenie liczby argumentów
	get_args(&argc, &argv, &p, &subst_p, &port, &adress_raw); //pobranie argumentów
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
			if (close(pipefd[1]) == -1)
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

			rejestracja(sock_fd,"127.0.0.1",12345);
			//rejestracja(sock_fd,adress_final,port);
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
				events_count=epoll_wait(fd_epoll, events, MAX_CONN, -1);
				if (events_count == -1)
				{
					perror("Epol wait error.\n");
					exit(EXIT_FAILURE);
				}

				for (int i=0; i<events_count; i++)
				{
					if (events[i].data.fd==sock_fd)
					{
						new_socket = polaczenie(sock_fd);
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

				if (read(pipefd[0], buffer_tmp, 4000)==-1) //wczytanie danych z pipe
				{
					WriteOnStdErr(strerror(errno));
				}

				if (write(new_socket, buffer_tmp, strlen(buffer_tmp))==-1)
				{
					perror("Write socket");
					exit(EXIT_FAILURE);
				}
			//WriteOnStdErr("buffer_tmp");

				close(new_socket); //zamknięcie socketu po wysłaniu danych
				nanosleep(&t, NULL); // niepotrzebny
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
