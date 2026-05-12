#ifndef command_hh
#define command_hh

#include <string>
#include <vector>
#include "simpleCommand.hh"

struct Command {
  std::vector<SimpleCommand *> _simpleCommands;
  std::string *_outFile;
  std::string *_inFile;
  std::string *_errFile;
  bool _background;
  bool _append;
  bool _ambiguousOutput;

  Command();
  void insertSimpleCommand( SimpleCommand *simpleCommand );

  void clear();
  void print();
  void execute();

  static SimpleCommand *_currentSimpleCommand;
};

#endif
