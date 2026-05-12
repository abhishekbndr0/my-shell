#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

#include "command.hh"
#include "shell.hh"

extern char **environ;
extern void myunputc(int c);
extern "C" void tty_restore_mode(void);  // a C function that gets out of the line editor's raw mode

Command::Command() {
    _simpleCommands = std::vector<SimpleCommand *>();  // initializing a new vector of simpleCommand's

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
    _ambiguousOutput = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;
    _append = false;
    _ambiguousOutput = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::execute() {
    // don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    if ( _ambiguousOutput ) {
      fprintf(stderr, "Ambiguous output redirect.\n");
      clear();
      Shell::prompt();
      return;
    }

    if (*_simpleCommands[0]->_arguments[0] == "exit") {  // exit check
        fprintf(stdout, "Good bye!!\n");
        tty_restore_mode();  // get out of raw mode on exiting myshell, so normal shell works as intended
        exit(0);
    }

    if (*_simpleCommands[0]->_arguments[0] == "source") {  // source built-in check outside of the file redirection logic
      SimpleCommand *s = _simpleCommands[0];
      if (s->_arguments.size() >= 2) {
        FILE *f = fopen(s->_arguments[1]->c_str(), "r");  // opens the file as read-only
        if (f == NULL) {
          perror("source");
        }
        else {
          std::string contents = "";
          int c;
          while ((c = fgetc(f)) != EOF) {
              contents += (char)c;
          }
          fclose(f);

          for (int i = contents.size() - 1; i >= 0; i--) {  // feeding characters of file contents back into the buffer in reverse order
            myunputc(contents[i]);
          }
        }
      }
      clear();
      Shell::prompt();
      return;
    }

    // for every simpleCommand, forks a new process, sets up i/o redirection, and calls exec
    int tmpin  = dup(0);  // copy of current stdin (terminal) - another file descriptor that points to the same thing as fd 0, and returns the new number
    int tmpout = dup(1);  // copy of current stdout (terminal)
    int tmperr = dup(2);

    int fdin;
    if (_inFile) {
      fdin = open(_inFile->c_str(), O_RDONLY);  // input redirection is read-only
    }
    else {
      fdin = dup(tmpin);  // use default input
    }

    int ret;
    int fdout;
    for (int i = 0; i < (int)_simpleCommands.size(); i++) {
      // redirect input
      dup2(fdin, 0);  // makes fd 0 point to whatever fdin points to, closes whatever fd 0 was pointing to before - also makes a command's stdin come from the pipe's read end
      close(fdin);  // now, the child process points to a file instead of the keyboard, and reads fd 0 normally

      if (i == (int)_simpleCommands.size() - 1){  // if it's the last simpleCommand, check for output
        if (_outFile){
          if (_append) {
            fdout = open(_outFile->c_str(), O_CREAT | O_WRONLY | O_APPEND, 0664);  // output redirection is write-only, creates the file if it's not there already, and overwrites anything already there
          }
          else {
            fdout = open(_outFile->c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0664);  // append redirection is the same as before, but instead of overwriting, it appends
          }
        }
        else {
          fdout = dup(tmpout);  // use default output
        }
      }
      else {  // if it's not the last simpleCommand, handle logic for creating the pipe
        int fdpipe[2];
        pipe(fdpipe);
        fdout = fdpipe[1];  // the write end where data goes into
        fdin = fdpipe[0];  // the read end where data comes out of
      }

      // redirect output
      dup2(fdout, 1);  // makes a command's stdout go into the pipe's write end
      close(fdout);  // close is important because the read (in) needs to know when to stop reading from the data going out of write

      // redirect stderr if needed
      if (_errFile) {
        int fderr;
        if (_append) {
          fderr = open(_errFile->c_str(), O_CREAT | O_WRONLY | O_APPEND, 0664);
        }
        else {
          fderr = open(_errFile->c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0664);
        }
        dup2(fderr, 2);
        close(fderr);
      }

      SimpleCommand *s = _simpleCommands[i];
      std::string built_in = *s->_arguments[0];  // checking for the built-in commands
      if ((built_in == "printenv") || (built_in == "setenv") || (built_in == "unsetenv") || (built_in == "cd")) {  // run in parent (no forking needed yet)
        if (built_in == "printenv") { // going through array of environment variable strings and printing them all on a new line
          for (int i = 0; environ[i] != NULL; i++) {
            printf("%s\n", environ[i]);
          }
          fflush(stdout);  // forces output to print immediately instead of waiting in the buffer
        }
        else if (built_in == "setenv") {
          if (s->_arguments.size() >= 3) {  // args vector would be something like "setenv, A, B"
            setenv(s->_arguments[1]->c_str(), s->_arguments[2]->c_str(), 1);  // 1 means overwrite if it already exists
          }
        }
        else if (built_in == "unsetenv") {
          if (s->_arguments.size() >= 2) {  // args vector would be something like "unsetenv, A"
            unsetenv(s->_arguments[1]->c_str());
          }
        }
        else if (built_in == "cd") {
          if (s->_arguments.size() >= 2) {
            if (chdir(s->_arguments[1]->c_str()) < 0) {
              fprintf(stderr, "cd: can't cd to %s\n", s->_arguments[1]->c_str());
            }
          }
          else {
            chdir(getenv("HOME"));
          }
        }
      }
      else {
        ret = fork();
        if (ret == 0) {  // child process
          if (_background) {
            setpgid(0, 0);  // puts this child in its own process group
          }

          const char **args = (const char **) malloc((s->_arguments.size() + 1) * sizeof(char*));  // setting up the args array to call execvp and get the first argument from the current simple command
          for (unsigned long j = 0; j < s->_arguments.size(); j++) {
            args[j] = s->_arguments[j]->c_str();
          }
          args[s->_arguments.size()] = NULL;

          execvp(args[0], (char* const*)args);
          perror("execvp");
          exit(1);
        }
        else if (ret < 0) {
          perror("fork");
          return;
        }
        else if (_background) {
          Shell::_lastBackgroundPID = ret;
        }
      }
      // parent shell continues
    }

    // restore in/out defaults
    dup2(tmpin, 0);  // keyboard back on fd 0
    dup2(tmpout, 1);  // screen back on fd 1
    dup2(tmperr, 2);
    close(tmpin);  // the saved copies aren't needed anymore
    close(tmpout);
    close(tmperr);

    if (!_background) {
      int wstatus;
      waitpid(ret, &wstatus, 0);  // wait for the last process
      Shell::_lastExitCode = WEXITSTATUS(wstatus);

      if (Shell::_lastExitCode != 0) {  // after getting the last exit code, prints its ON_ERROR value if it's not 0
        const char *on_error = getenv("ON_ERROR");
        if (on_error != NULL) {
          fprintf(stderr, "%s\n", on_error);
        }
      }
    }

    if (!_simpleCommands.empty()) {  // checking for the last argument right before clear() happens
      SimpleCommand *last = _simpleCommands.back();  // getting the last simple command
      if (!last->_arguments.empty()) {
        Shell::_lastArgument = *last->_arguments.back();  // getting the last argument
      }
    }

    // clear to prepare for the next command
    clear();

    // prints a new prompt
    Shell::prompt();
}

SimpleCommand *Command::_currentSimpleCommand;
