%code requires 
{
#include <string>

#if __cplusplus > 199711L
#define register  // deprecated in C++11, so remove the keyword
#endif
}

%union
{
  char *string_val;
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE LESS GREATGREAT GREATAMPERSAND GREATGREATAMPERSAND GREAT2 PIPE AMPERSAND

%{
#include <cstdio>
#include "shell.hh"

void yyerror(const char * s);
int yylex();
%}

%%

goal: commands ;

commands: command | commands command ;

command: simple_command ;

simple_command:
  command_and_args iomodifier_list background_optional NEWLINE {
    Shell::_currentCommand.execute();
  }
  | NEWLINE {
    Shell::prompt();
  }
  | error NEWLINE { yyerrok; } ;

command_and_args:
  command_word argument_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  | command_and_args PIPE command_word argument_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  } ;

argument_list: argument_list argument | ;

argument:
  WORD {
    Command::_currentSimpleCommand->insertArgument( $1 );
  } ;

command_word:
  WORD {
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  } ;

iomodifier_list: iomodifier_list iomodifier | ;

iomodifier:
  GREAT WORD {
    if ( Shell::_currentCommand._outFile ) {
      Shell::_currentCommand._ambiguousOutput = true;
    }
    Shell::_currentCommand._outFile = $2;
  }
  | GREATGREAT WORD {
    if ( Shell::_currentCommand._outFile ) {
        Shell::_currentCommand._ambiguousOutput = true;
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._append = true;
  }
  | GREATAMPERSAND WORD {
    if ( Shell::_currentCommand._outFile ) {
        Shell::_currentCommand._ambiguousOutput = true;
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = new std::string( $2->c_str() );
  }
  | GREATGREATAMPERSAND WORD {
    if ( Shell::_currentCommand._outFile ) {
        Shell::_currentCommand._ambiguousOutput = true;
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = new std::string( $2->c_str() );
    Shell::_currentCommand._append = true;
  }
  | LESS WORD {
    Shell::_currentCommand._inFile = $2;
  }
  | GREAT2 WORD {
    Shell::_currentCommand._errFile = $2;
  } ;

background_optional:
  AMPERSAND {
    Shell::_currentCommand._background = true;
  }
  | ;

%%

void yyerror(const char *s) {
  fprintf(stderr,"%s", s);
}
