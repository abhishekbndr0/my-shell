#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <dirent.h>
#include <regex.h>
#include <algorithm>
#include "simpleCommand.hh"
#include "command.hh"

#define MAXFILENAME 1024

SimpleCommand::SimpleCommand() {
  _arguments = std::vector<std::string *>();
}

SimpleCommand::~SimpleCommand() {
  // iterate over all the arguments and delete them
  for (auto &arg: _arguments) {
    delete arg;
  }
}

void SimpleCommand::insertArgument(std::string *argument) {
  if ((strchr(argument->c_str(), '*') == NULL) && (strchr(argument->c_str(), '?') == NULL)) {
    // simply add the argument to the vector if there's no wildcards
    _arguments.push_back(argument);
    return;
  }

  std::string arg = *argument;  // save the actual string value, and delete because it's not gonna be pushed back to arguments list
  delete argument;
  if (arg[0] == '/') {  // if absolute path, suffix skips the '/' at the start
    expandWildcard("/", arg.c_str() + 1);
  }
  else if (strchr(arg.c_str(), '/') == NULL) {
    expandWildcardCurrentDir(arg.c_str());
  }
  else {  // if relative path, suffix starts at start
    expandWildcard("", arg.c_str());
  }
}

void SimpleCommand::expandWildcardCurrentDir(const char *arg) {
  if ((strchr(arg, '*') == NULL) && (strchr(arg, '?') == NULL)) {
    Command::_currentSimpleCommand->insertArgument(new std::string(arg));
    return;
  }

  // converting wildcards to regular expressions
  char *reg = (char *)malloc(2 * strlen(arg) + 10);
  const char *a = arg;
  char *r = reg;

  *r = '^';  // match beginning of the line
  r++;
  while (*a) {
    if (*a == '*') {  // "*" -> ".*"
      *r = '.';
      r++;
      *r = '*';
      r++;
    }
    else if (*a == '?') {  // "?" -> "."
      *r = '.';
      r++;
    }
    else if (*a == '.') {  // "." -> "\."
      *r = '\\';
      r++;
      *r = '.';
      r++;
    }
    else {  // adding the component itself
      *r = *a;
      r++;
    }
    a++;
  }
  *r = '$'; // match end of the line
  r++;
  *r = 0;  // match NULL character

  // compiling regex
  regex_t re;
  int result = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);
  if (result != 0) {  // if the compilation failed, insert the original unexpanded wildcard argument
    perror("compile");
    free(reg);
    return;
  }

  DIR *d = opendir("."); // listing the directory
  if (d == NULL) {
    perror("opendir");
    regfree(&re);
    free(reg);
    return;
  }

  // add entries that match the regular expression as arguments
  struct dirent *ent;
  int maxEntries = 20;
  int nEntries = 0;
  char **array = (char **) malloc(maxEntries * sizeof(char *));
  while ((ent = readdir(d)) != NULL) {
    if (regexec(&re, ent->d_name, 0, NULL, 0) == 0) {
      if (ent->d_name[0] == '.') {  // checks for hidden files and only includes them if . is at the start of the pattern
        if (arg[0] == '.') {
          array[nEntries] = strdup(ent->d_name);
          nEntries++;
        }
      }
      else {  // no hidden files, so add all entries
        array[nEntries] = strdup(ent->d_name);
        nEntries++;
      }
      if (nEntries == maxEntries) {
        maxEntries *= 2;
        array = (char **)realloc(array, maxEntries * sizeof(char *));
      }
    }
  }
  closedir(d);
  regfree(&re);
  free(reg);

  for (int i = 0; i < nEntries; i++) {  // sorting all entries alphabetically with bubble sort
    for (int j = i + 1; j < nEntries; j++) {
      if (strcmp(array[i], array[j]) > 0) {
        char *tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
      }
    }
  }

  if (nEntries == 0) {  // if there's no matches, just insert the argument as it is
    Command::_currentSimpleCommand->insertArgument(new std::string(arg));
    free(array);
    return;
  }

  for (int i = 0; i < nEntries; i++) {  // for each match, add it as an argument
    Command::_currentSimpleCommand->insertArgument(new std::string(array[i]));
    free(array[i]);
  }
  free(array);
}

void SimpleCommand::expandWildcard(const char *prefix, const char *suffix) {
  if (suffix[0] == 0) {  // suffix is empty, so put the prefix in an argument
    Command::_currentSimpleCommand->insertArgument(new std::string(prefix));
    return;
  }

  const char *s = strchr(suffix, '/');  // get the next component in the suffix, and advance it
  char component[MAXFILENAME];
  if (s != NULL) {  // copy up to the first '/'
    strncpy(component, suffix, s - suffix);
    component[s - suffix] = '\0';
    suffix = s + 1;
  }
  else {  // last part of the path, so copy the whole thing
    strcpy(component, suffix);
    suffix = suffix + strlen(suffix);
  }

  char newPrefix[MAXFILENAME];  // expanding the component
  if ((strchr(component, '*') == NULL) && (strchr(component, '?') == NULL)) {
    if (prefix[0] == '\0') {  // if the prefix is empty, there shouldn't be a '/' at the beginning
      snprintf(newPrefix, MAXFILENAME, "%s", component);  // it should be in the current directory, not the root
    }
    else if (strcmp(prefix, "/") == 0) {
      snprintf(newPrefix, MAXFILENAME, "/%s", component);
    }
    else {  // if the prefix is non-empty, it would be the 'prefix/component'
      snprintf(newPrefix, MAXFILENAME, "%s/%s", prefix, component);
    }
    expandWildcard(newPrefix, suffix);
    return;
  }

  // converting wildcards to regular expressions
  char *reg = (char *)malloc(2 * strlen(component) + 10);
  const char *a = component;
  char *r = reg;

  *r = '^';  // match beginning of the line
  r++;
  while (*a) {
    if (*a == '*') {  // "*" -> ".*"
      *r = '.';
      r++;
      *r = '*';
      r++;
    }
    else if (*a == '?') {  // "?" -> "."
      *r = '.';
      r++;
    }
    else if (*a == '.') {  // "." -> "\."
      *r = '\\';
      r++;
      *r = '.';
      r++;
    }
    else {  // adding the component itself
      *r = *a;
      r++;
    }
    a++;
  }
  *r = '$';  // match end of the line
  r++;
  *r = 0;  // match NULL character

  // compiling regex
  regex_t re;
  int result = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);
  if (result != 0) {  // if the compilation failed, insert the original unexpanded wildcard argument
    perror("compile");
    free(reg);
    return;
  }

  // list directory depending on whether a prefix exists already
  const char *dir;
  if (prefix[0] == '\0') {
    dir = (char *)".";
  }
  else {
    dir = prefix;
  }
  DIR *d = opendir(dir);
  if (d == NULL) {
    perror("opendir");
    regfree(&re);
    free(reg);
    return;
  }

  // add entires that match the regular expression as arguments
  struct dirent *ent;
  int maxEntries = 20;
  int nEntries = 0;
  char **array = (char **) malloc(maxEntries * sizeof(char *));
  while ((ent = readdir(d)) != NULL) {
    if (regexec(&re, ent->d_name, 0, NULL, 0) == 0) {
      if (ent->d_name[0] == '.') {  // checks for hidden files, and include them if . is at the start of the pattern
        if (component[0] == '.') {
          array[nEntries] = strdup(ent->d_name);
          nEntries++;
        }
      }
      else {  // not hidden files, so add all entries
        array[nEntries] = strdup(ent->d_name);
        nEntries++;
      }
      if (nEntries == maxEntries) {
        maxEntries *= 2;
        array = (char **)realloc(array, maxEntries * sizeof(char *));
      }
    }
  }
  closedir(d);
  regfree(&re);
  free(reg);

  for (int i = 0; i < nEntries; i++) {  // sorting all entries alphabetically with bubble sort
    for (int j = i + 1; j < nEntries; j++) {
      if (strcmp(array[i], array[j]) > 0) {
        char *tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
      }
    }
  }

  if (nEntries == 0) {  // if there are no matches, build the original full path and just insert it as its own argument
    char original[MAXFILENAME];
    if (prefix[0] == 0) {
        sprintf(original, "%s", component);
    }
    else if (strcmp(prefix, "/") == 0) {
        sprintf(original, "/%s", component);
    }
    else {
        sprintf(original, "%s/%s", prefix, component);
    }
    if (suffix[0] != 0) {
        strcat(original, "/");
        strcat(original, suffix);
    }
    Command::_currentSimpleCommand->insertArgument(new std::string(original));
    free(array);
    return;
  }

  for (int i = 0; i < nEntries; i++) {  // for each matching entry, find the new prefix/suffix and expand again because there could be more components
    if (prefix[0] == 0) {
        snprintf(newPrefix, MAXFILENAME, "%s", array[i]);
    }
    else if (strcmp(prefix, "/") == 0) {
        snprintf(newPrefix, MAXFILENAME, "/%s", array[i]);
    }
    else {
        snprintf(newPrefix, MAXFILENAME, "%s/%s", prefix, array[i]);
    }
    expandWildcard(newPrefix, suffix);
    free(array[i]);
  }
  free(array);
}

// print out the simple command
void SimpleCommand::print() {
  for (auto &arg: _arguments) {
    std::cout << "\"" << *arg << "\" \t";
  }
  // effectively the same as printf("\n\n");
  std::cout << std::endl;
}
