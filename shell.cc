#include <cstdio>
#include <climits>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string>
#include "shell.hh"

int yyparse(void);

int Shell::_lastExitCode = 0;
int Shell::_lastBackgroundPID = 0;
std::string Shell::_lastArgument = "";
std::string Shell::_shellPath = "";

extern int line_length;
extern char line_buffer[];
extern int cursor;

extern int shellrc_found;

void Shell::prompt() {
  if (isatty(0)) {
    const char *prompt = getenv("PROMPT");
    if (prompt != NULL) {  // if there's an environment variable set for prompt, use that instead
      printf("%s", prompt);
    }
    else {
      printf("myshell>");
    }
    fflush(stdout);
  }
}

void sigIntHandler(int sig) {
  line_length = 0;
  line_buffer[0] = '\0';
  cursor = 0;
  fprintf(stderr, "\n");
  Shell::prompt();  // new prompt when there's a Ctrl-C
}

void sigChldHandler(int sig) {
  int pid;
  int wstatus;
  // while loop because multiple children could finish at once with only one SIGCHLD
  // changed this to > 0 because != -1 was giivng an infinite loop of [0] exited
  while ((pid = (waitpid(-1, &wstatus, WNOHANG))) > 0) {  // -1 = wait for any child, WNOHANG = don't block, just check and immediately return if nothing's done yet
    // nothing
  }
}

int main(int argc, char **argv) {
  // resolving the shell path for ${SHELL}
  char fullPath[PATH_MAX];
  if (realpath(argv[0], fullPath)) {
    Shell::_shellPath = std::string(fullPath);
  }
  else {
    Shell::_shellPath = std::string(argv[0]);
  }
  setenv("SHELL", Shell::_shellPath.c_str(), 1);

  struct sigaction signalAction;
  signalAction.sa_handler  = sigIntHandler;
  sigemptyset(&signalAction.sa_mask);
  signalAction.sa_flags = SA_RESTART;  // stops getc() from returning EOF on interrupt
  int error = sigaction(SIGINT, &signalAction, NULL);
  if (error) {
    perror("sigaction");
    exit(-1);
  }

  signalAction.sa_handler = sigChldHandler;  // zombie elimination handler (similar to ctrl+c implementation)
  sigemptyset(&signalAction.sa_mask);
  signalAction.sa_flags = SA_RESTART;
  error = sigaction(SIGCHLD, &signalAction, NULL);
  if (error) {
    perror("sigaction");
    exit(-1);
  }

  // fixing the double prompt print when there's a shellrc
  FILE *f = fopen(".shellrc", "r");
  if (f != NULL) {
    shellrc_found = 1;
    fclose(f);
  }
  if (!shellrc_found) {
    Shell::prompt();
  }
  yyparse();
}

Command Shell::_currentCommand;
