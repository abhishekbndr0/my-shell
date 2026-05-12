%{

#include <cstring>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include "shell.hh"
#include "y.tab.hh"

extern char *read_line();
int shellrc_found = 0;

static void yyunput (int c, char *buf_ptr);
void myunputc(int c) {
  unput(c);
}

int mygetc(FILE *f) {
  static char *p;
  char ch;

  if (!isatty(0)) { // stdin is not a tty -> call real getc
    return getc(f);
  }

  static FILE *shellrc = NULL;  // shellrc check
  static int shellrc_checked = 0;
  if (shellrc_checked == 0) {
    shellrc_checked = 1;
    if (shellrc_found) {
      shellrc = fopen(".shellrc", "r");
    }
  }
  if (shellrc != NULL) {
    int c = fgetc(shellrc);
    if (c != EOF) {
      return c;
    }
    fclose(shellrc);
    shellrc = NULL;
  }

  // stdin is a tty -> call read_line
  if (p == NULL || *p == 0) {
    char *s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {  // discard spaces and tabs

}

"<(" {  // collecting everything inside the <() as the process substitution command
  std::string command = "";
  int character = yyinput();
  while ((character != ')') && (character != EOF)) {
    command += character;
    character = yyinput();
  }
  command += "\n";

  char temp_directory[] = "/tmp/shell_ps_XXXXXX";  // directory has to end in XXXXXX so they can be replaced by the unique characters
  if (mkdtemp(temp_directory) == NULL) {  // mkdtemp error checking
    perror("mkdtemp");
    yylval.cpp_string = new std::string("");
    return WORD;
  }
  char fifopath[128];  // making the pipe name size 128 just to be safe
  snprintf(fifopath, sizeof(fifopath), "%s/fifo", temp_directory);
  if (mkfifo(fifopath, 0600) < 0) {  // mkfifo error checking
    perror("mkfifo");
    rmdir(temp_directory);
    yylval.cpp_string = new std::string("");
    return WORD;
  }

  int ret = fork();
  if (ret == 0) {  // the child process runs the inner command
    int pipe_in[2];  // pipe to send command to a subshell
    pipe(pipe_in);

    int ret2 = fork();
    if (ret2 == 0) {  // the grandchild runs the subshell
      close(pipe_in[1]);
      dup2(pipe_in[0], 0);  // redirects stdin to the pipe's read end
      close(pipe_in[0]);

      int fd = open(fifopath, O_WRONLY);
      dup2(fd, 1);  // redirects stdout to the fifo
      close(fd);

      char *args[2];
      args[0] = (char *)"/proc/self/exe";
      args[1] = NULL;
      execvp(args[0], args);
      perror("execvp procsub");
      exit(1);
    }

    close(pipe_in[0]);  // child writes command to grandchild's stdin
    write(pipe_in[1], command.c_str(), command.size());
    close(pipe_in[1]);
    waitpid(ret2, NULL, 0);

    unlink(fifopath);  // clean up resources
    rmdir(temp_directory);
    exit(0);
  }

  yylval.cpp_string = new std::string(fifopath);
  return WORD;
}

"$(" {  // collecting everything inside the $() as the subshell command
  std::string command = "";
  int character = yyinput();
  while ((character != ')') && (character != EOF)) {
    command += character;
    character = yyinput();
  }
  command += "\n";

  int pipe_in[2];   // parent writes command and child reads it as stdin
  int pipe_out[2];  // child writes output and parent reads it back
  pipe(pipe_in);
  pipe(pipe_out);

  int ret = fork();
  if (ret == 0) {  // child process, new shell
    dup2(pipe_in[0], 0);  // stdin comes from pipe_in read end instead of the keyboard
    dup2(pipe_out[1], 1);  // stdout goes to pipe_out write end instead of the screen
    close(pipe_in[1]);
    close(pipe_out[0]);  // only parent writes to child, so no need for pipe_in[1], and only parent reads from child, so no need for pipe_out[0]

    char *args[2];
    args[0] = (char *)"/proc/self/exe";  // this path is always to the current running executable, so the child executes the shell itself
    args[1] = NULL;
    execvp(args[0], args);
    perror("execvp subshell");
    exit(1);
  }
  close(pipe_in[0]);
  close(pipe_out[1]);  // only child reads from parent, so no need for pipe_in[0], and only child writes to parent, so no need for pipe_out[1]
  write(pipe_in[1], command.c_str(), command.size());  // parents writes to child through pipe_in[1], and then closes for EOF
  close(pipe_in[1]);

  std::string output = "";
  char buffer[1];
  while (read(pipe_out[0], buffer, 1) > 0) {  // parent reads all output from child through pipe_out[0] (read returns 0 on child's exit)
    if (buffer[0] == '\n') {
      buffer[0] = ' ';  // replacing newlines with spaces
    }
    output += buffer[0];
  }
  close(pipe_out[0]);  // done reading
  waitpid(ret, NULL, 0);  // waits for child shell to fully finish

  for (int i = output.size() - 1; i >= 0; i--) {  // putting characters of output back into the buffer in reverse order
    myunputc(output[i]);
  }
}

\"[^\"]*\" {  // matches any character that isn't a quote
  yylval.cpp_string = new std::string(yytext + 1, yyleng - 2);  // skips first character and removes both quotes from length
  return WORD;
}

">" {
  return GREAT;
}

"<" {
  return LESS;
}

">>" {
  return GREATGREAT;
}

">&" {
  return GREATAMPERSAND;
}

">>&" {
  return GREATGREATAMPERSAND;
}

"2>" {
  return GREAT2;
}

"|" {
  return PIPE;
}

"&" {
  return AMPERSAND;
}

([^ \t\n\"\\|><&$]|\\.|\$\{[^}]*\})+ {  // 1+ characters, where each char is a non-special char, or a backslash with any character, or an environment variable within a ${ and }
  std::string *s = new std::string();

  if (yytext[0] == '~') {  // tilde check
    std::string username = "";  // start after the '~' character
    int i = 1;
    while((i < yyleng) && (yytext[i] != '/')) {  // go until the end or '/' to get the username
      username += yytext[i];
      i++;
    }

    std::string home_directory = "";
    if (username.empty()) {  // handles the case of just ~
      home_directory = getenv("HOME");
    }
    else {  // handles the case of ~/username
      struct passwd *pw = getpwnam(username.c_str());  // getpwnam gets all the information about the user, to specify its home directory
      home_directory = pw->pw_dir;
    }
    *s = home_directory + std::string(yytext + i);  // handles the case of ~/username/dir
    yylval.cpp_string = s;
    return WORD;
  }

  for (int i = 0; i < yyleng; i++) {
    if (yytext[i] == '\\') {  // explicitly "consuming/moving over" escaping characters when they appear in a word
      i++;
      *s += yytext[i];  // skips backslash and take the next character literally
    }
    else if ((yytext[i] == '$') && (yytext[i+1] == '{')) {
      i += 2;
      std::string environment_variable = "";
      while (yytext[i] != '}') {
        environment_variable += yytext[i++];
      }

      std::string expanded;
      if (environment_variable == "$") {  // pid of the shell process
        expanded = std::to_string(getpid());
      }
      else if(environment_variable == "?") {  // exit code of last simple command
        expanded = std::to_string(Shell::_lastExitCode);
      }
      else if (environment_variable == "!") {  // pid of last background process
        expanded = std::to_string(Shell::_lastBackgroundPID);
      }
      else if(environment_variable == "_") {  // last argument of last command
        expanded = Shell::_lastArgument;
      }
      else {  // expanded path if it exists
        const char *env = getenv(environment_variable.c_str());
        expanded = env;
      }
      *s += expanded;
    }
    else {
      *s += yytext[i];
    }
  }
  yylval.cpp_string = s;
  return WORD;
}
