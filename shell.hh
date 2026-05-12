#ifndef shell_hh
#define shell_hh

#include "command.hh"

struct Shell {
  static void prompt();
  static Command _currentCommand;

  static int _lastExitCode;  // ${?}
  static int _lastBackgroundPID;  // ${!}
  static std::string _lastArgument;  // ${_}
  static std::string _shellPath;  // ${SHELL}
};

#endif
