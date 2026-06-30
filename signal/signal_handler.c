#include <stdio.h>
#include <stdlib.h>
#include "signal_handler.h"

// Global variables
volatile sig_atomic_t sigint_received = 0;

static void err_output(char *msg){
	FILE *log_file = fopen("signal_exit_log.txt", "w");
	fputs(msg, stderr);
	if (log_file != NULL) {
		fputs(msg, log_file);
		fclose(log_file);
	}
}

int signal_close(void) {
	
	switch (sigint_received) {
	case 1:
		err_output("Ended by Ctrl+C\n");
		return EXIT_FAILURE;
	case 2:
		err_output("Ended by Ctrl+\\\n");
		return EXIT_FAILURE;
	case 3:
		err_output("Program Killed\n");
		return EXIT_FAILURE;
	case 4:
		err_output("Terminal Hangup\n");
		return EXIT_FAILURE;
	case 5:
		err_output("Unknown Cause of Exit\n");
		return EXIT_FAILURE;
	default:
		return EXIT_SUCCESS;
	}
}

void signal_handler(int signum) {
	if (sigint_received == 0) {
		if (signum == SIGINT)
			sigint_received = 1;
		else if (signum == SIGQUIT)
			sigint_received = 2;
		else if (signum == SIGTERM)
			sigint_received = 3;
		else if (signum == SIGHUP)
			sigint_received = 4;
		else
			sigint_received = 5;	// unknown signal
	}
}

void setup_signals(void) {
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = signal_handler;
//	sa.sa_flags = SA_RESTART;
//	changed from SA_RESTART to zero to avoid delay from get_ch() loop
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
}
