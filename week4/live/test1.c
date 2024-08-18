#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809l
#endif


#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void handle_sigint(int sig) {
	write(2, "Recieved SIGINT signal\n", 24);
}

void handle_sigterm(int sig) {
	write(2, "Recieved SIGTERM signal\n", 25);
}

void handle_sigsegv(int sig) {
	write(2, "Recieved SIGSEGV signal\n", 25);
}


int main(int argc, char **argv) {
	struct sigaction sint, sterm, ssegv;

	sigemptyset(&sint.sa_mask);
	sint.sa_flags = 0;
	sint.sa_handler = handle_sigint;
	sigemptyset(&sterm.sa_mask);
	sterm.sa_flags = 0;
	sterm.sa_handler = handle_sigterm;
	sigemptyset(&ssegv.sa_mask);
	ssegv.sa_flags = 0;
	ssegv.sa_handler = handle_sigsegv;

	sigaction(SIGTERM, &sterm, NULL);
	sigaction(SIGINT, &sint, NULL);
	sigaction(SIGSEGV, &ssegv, NULL);

	// int *a = 0x9999999999999;
	// *a = -4;

	for (;;) sleep(1);

	return 0;
}
