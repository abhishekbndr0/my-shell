#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <dirent.h>
#include <algorithm>

#define MAX_BUFFER_LINE 2048

extern "C" void tty_raw_mode(void);
int line_length;
int cursor;
char line_buffer[MAX_BUFFER_LINE];  // buffer where line is stored

int history_index = 0;
std::vector<std::string> history;

void read_line_print_usage() {
  const char *usage = "\n"
    " ctrl-?       Print usage\n"
    " ctrl-D       Delete character at cursor\n"
    " ctrl-H       Backspace\n"
    " ctrl-A       Move to beginning of line\n"
    " ctrl-E       Move to end of line\n"
    " up arrow     See last command in the history\n"
    " left arrow   Move cursor left\n"
    " right arrow  Move cursor right\n";

  write(1, usage, strlen(usage));
}

char *read_line() {  // input a line with some basic editing
  tty_raw_mode();  // sets terminal in raw mode
  line_length = 0;
  cursor = 0;

  // reads one line until enter is typed
  while (1) {
    // read one character in raw mode
    char ch;
    read(0, &ch, 1);

    if (ch >= 32 && ch != 127) {  // if it's a printable character...
      if (line_length == MAX_BUFFER_LINE - 2) break;

      int i;
      for (i = line_length; i > cursor; i--) {  // for everything to the right of the cursor, shift to the right by 1 to make room for the new character
        line_buffer[i] = line_buffer[i - 1];
      }
      // add char to buffer
      line_buffer[cursor] = ch;
      line_length++;

      // do echo
      write(1, &line_buffer[cursor], line_length - cursor);  // write from the start (where the new character is), from the cursor to the end of the line
      cursor++;  // but cursor is moved to the end of the line

      for (i = cursor; i < line_length; i++) {  // writes a backspace repeatedly until the cursor is back at right after the added character
        ch = 8;
        write(1, &ch, 1);
      }
    }
    else if (ch == 10) {  // if it's an <Enter>, return line...
      line_buffer[line_length] = 0;
      history.push_back(std::string(line_buffer));
      history_index = history.size();

      write(1, &ch, 1);  // print newline

      break;
    }
    else if (ch == 31) {  // if it's a <ctrl+?>...
      read_line_print_usage();
      line_buffer[0] = 0;
      break;
    }
    else if (ch == 8 || ch == 127) {  // if it's a <backspace>, removes the previous character read...
      if (cursor == 0) {  // can't backspace at the beginning
        continue;
      }
      int i;
      for (i = cursor - 1; i < line_length - 1; i++) {  // shifts everything from the cursor - 1 to the end, left by 1
        line_buffer[i] = line_buffer[i + 1];
      }
      // remove one character from buffer
      line_length--;
      cursor--;

      // go back one character
      ch = 8;
      write(1, &ch, 1);

      write(1, &line_buffer[cursor], line_length - cursor);  // write from the start (where the new character is), from the cursor to the end of the line

      // write a space to erase the last character read
      ch = ' ';
      write(1, &ch, 1);

      // move cursor repeatedly back to the right spot after the backspace
      for (i = cursor; i < line_length + 1; i++) {
        ch = 8;
        write(1, &ch, 1);
      }
    }
    else if (ch == 4) {  // if it's a <ctrl-D>, delete at cursor...
      if (cursor == line_length) {  // can't delete if you're at the end of the line
        continue;
      }

      int i;
      for (i = cursor; i < line_length - 1; i++) {  // shifts everything from the cursor to the end, left by 1
        line_buffer[i] = line_buffer[i + 1];
      }
      line_length--;
      line_buffer[line_length] = '\0';

      int cursor_to_end = line_length - cursor;
      if (cursor_to_end < 0) {
        cursor_to_end = 0;
      }
      if (cursor_to_end > 0) {
        write(1, &line_buffer[cursor], cursor_to_end);  // write from the start (where the new character is), from the cursor to the end of the line
      }

      // write a space to erase the last character read
      ch = ' ';
      write(1, &ch, 1);

      // move cursor repeatedly back to the right spot after the backspace
      for (i = 0; i < cursor_to_end + 1; i++) {
        ch = 8;
        write(1, &ch, 1);
      }
    }
    else if (ch == 1) {  // if it's a <ctrl-A>, moves cursor to the start...
      while (cursor > 0) {  // repeatedly move cursor until it's at the beginning of the line
        ch = 8;
        write(1, &ch, 1);
        cursor--;
      }
    }
    else if (ch == 5) {  // if it's a <ctrl-E>, moves cursor to the end...
      write(1, &line_buffer[cursor], line_length - cursor);  // write from the cursor to the end, moving the cursor to the end
      cursor = line_length;
    }
    else if (ch == 27) {  // if it's an <esc>, reads two or more characters...
      char ch1;
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1 == 91 && ch2 == 65) {  // if it's an up arrow, prints next line in history
        if (history.empty()) {
          continue;
        }
        if (history_index > (int)history.size()) {
          history_index = history.size();
        }
        if (history_index > 0) {
          history_index--;
        }

        // erases old line + prints backspaces
        int i = 0;
        for (i = 0; i < cursor; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        // prints spaces on top
        for (i = 0; i < line_length; i++) {
          ch = ' ';
          write(1, &ch, 1);
        }

        // prints backspaces
        for (i = 0; i < line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        if (history_index == (int)history.size()) {  // just show an empty prompt line
          line_length = 0;
          cursor = 0;
          line_buffer[0] = '\0';
        }
        else {  // if there still is a previous line in history, copy it and echo
          strcpy(line_buffer, history[history_index].c_str());
          line_length = strlen(line_buffer);
          cursor = line_length;

          write(1, line_buffer, line_length);
        }
      }
      else if (ch1 == 91 && ch2 == 66) {  // if it's a down arrow, prints increasingly recent line from history
        if (history.empty()) {
          continue;
        }

        // erase old line
        int i = 0;
        for (i = 0; i < cursor; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        for (i = 0; i < line_length; i++) {
          ch = ' ';
          write(1, &ch, 1);
        }

        for (i = 0; i < line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }

        // move forward in history (for up, we went back in history)
        if (history_index < (int)history.size()) {
          history_index++;
        }

        // if we're past the newest prompt...
        if (history_index == (int)history.size()) {  // just show an empty prompt line
          line_length = 0;
          cursor = 0;
          line_buffer[0] = '\0';
        }
        else {  // if there's still a more recent line from history, copy it and echo
          strcpy(line_buffer, history[history_index].c_str());
          line_length = strlen(line_buffer);
          cursor = line_length;

          write(1, line_buffer, line_length);
        }
      }
      else if (ch1 == 91 && ch2 == 68) {  // if it's a left arrow (27 91 68), moves cursor to the left...
        if (cursor == 0) continue;  // can't move left if already at beginning
        cursor--;  // moves cursor back by 1 and writes a backspace
        ch = 8;
        write(1, &ch, 1);
      }
      else if (ch1 == 91 && ch2 == 67) {  // if it's a right arrow (27 91 67), moves cursor to the right...
        if (cursor == line_length) continue;  // can't move right if already at end
        write(1, &line_buffer[cursor], 1);
        cursor++;
      }
    }
    else if (ch == 9) {  // if it's a <tab> key, autofill if one + display options if two
      static int last_was_tab = 0;

      // getting what needs to be expanded and saving it in a prefix variable
      int beginning = cursor;
      while ((beginning > 0) && (line_buffer[beginning - 1] != ' ')) {
        beginning--;
      }
      std::string prefix(line_buffer + beginning, cursor - beginning);  // from the beginning of the word (length of where the cursor is minus the location of the word start)

      // getting all of the matches
      std::vector<std::string> matches;
      DIR *d = opendir(".");
      if (d != NULL) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
          std::string name = ent->d_name;
          if (name[0] == '.') {  // skip hidden files
            if (prefix.empty() || (prefix[0] != '.')) {
              continue;
            }
          }
          if (name.substr(0, prefix.size()) == prefix) {  // if the prefix of the match matches the word typed, add it to the list of matches
            matches.push_back(name);
          }
        }
        closedir(d);
        std::sort(matches.begin(), matches.end());  // sorting
      }
      if (matches.empty()) {  // if there aren't any matches, don't do anything
        last_was_tab = 0;
        continue;
      }

      // if there's only one match, just expand it
      if (matches.size() == 1) {
        std::string completion = matches[0].substr(prefix.size());  // gets the rest of the match that wasn't typed
        for (char c: completion) {  // for each character, shift the buffer to the right by 1 and insert the character where the space was made
          for (int i = line_length; i > cursor; i--) {
            line_buffer[i] = line_buffer[i - 1];
          }
          line_buffer[cursor] = c;
          line_length++;
          cursor++;
        }
        line_buffer[line_length] = '\0';

        int new_beginning = cursor - completion.size();  // prints the updated text
        write(1, &line_buffer[new_beginning], line_length - new_beginning);
        for (int i = cursor; i < line_length; i++) {  // moves the cursor back to where it should be
          ch = 8;
          write(1, &ch, 1);
        }
        last_was_tab = 0;
      }
      // if there's multiple matches...
      else {
        std::string longest_common_prefix = matches[0];
        for (std::string &m: matches) {  // builds the longest common prefix that needs to be autofilled for one tab
          int j = 0;
          while ((j != (int)longest_common_prefix.size()) && (j != (int)m.size()) && (longest_common_prefix[j] == m[j])) {
            j++;
          }
          longest_common_prefix = longest_common_prefix.substr(0, j);
        }

        if (longest_common_prefix.size() > prefix.size()) {
          std::string completion = longest_common_prefix.substr(prefix.size());  // gets the rest of the match that wasn't typed
          for (char c: completion) {  // for each character, shift the buffer to the right by 1 and insert the character where the space was made
            for (int i = line_length; i > cursor; i--) {
              line_buffer[i] = line_buffer[i - 1];
            }
            line_buffer[cursor] = c;
            line_length++;
            cursor++;
          }
          line_buffer[line_length] = '\0';

          write(1, &line_buffer[cursor - completion.size()], line_length - (cursor - completion.size()));  // prints the updated text
          for (int i = cursor; i < line_length; i++) {  // moves the cursor back to where it should be
            ch = 8;
            write(1, &ch, 1);
          }
          last_was_tab = 0;
        }
        else if (last_was_tab) {  // if the last character was a tab, implement the double tab logic
          write(1, "\n", 1);  // writes a newline, prints all the matches with two spaces between them, and another new line
          for (std::string &m: matches) {
            write(1, m.c_str(), m.size());
            write(1, "  ", 2);
          }
          write(1, "\n", 1);

          const char *prompt = getenv("PROMPT");  // reprints the prompt and what the current line had (if it didn't have anything, print just the prompt)
          if (prompt) {
            write(1, prompt, strlen(prompt));
          }
          else {
            write(1, "myshell>", 8);
          }
          write(1, line_buffer, line_length);  // prints the updated text
          for (int i = cursor; i < line_length; i++) {  // moves the cursor back to where it should be
            ch = 8;
            write(1, &ch, 1);
          }
          last_was_tab = 0;
        }
        else {  // resets the tab tracking so the next tab is a "first" tab press
          last_was_tab = 1;
        }
      }
    }
  }

  // adds EOL + null character at the end of string
  line_buffer[line_length] = 10;
  line_length++;
  line_buffer[line_length] = 0;

  return line_buffer;
}
