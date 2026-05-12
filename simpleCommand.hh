#ifndef simplecommand_hh
#define simplecommand_hh

#include <string>
#include <vector>

struct SimpleCommand {  // simpleCommand is simply a vector of strings
  std::vector<std::string *> _arguments;

  SimpleCommand();
  ~SimpleCommand();
  void insertArgument( std::string * argument );
  void expandWildcardCurrentDir(const char *arg);
  void expandWildcard(const char *prefix, const char *suffix);
  void print();
};

#endif
