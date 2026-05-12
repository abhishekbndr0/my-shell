#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>

// sets terminal into raw mode -> characters available immediately instead of waiting for a newline + no automatic echo
static struct termios orig_termios;
static int tty_saved = 0;

void tty_restore_mode(void) {  // function that actually gets out of raw mode
  if (tty_saved) {
    tcsetattr(0, TCSANOW, &orig_termios);
  }
}

void tty_raw_mode(void) {
  struct termios new_termios;

  if (!tty_saved) {  // saves the current mode if not saved already
    tcgetattr(0, &orig_termios);
    tty_saved = 1;
    atexit(tty_restore_mode);
  }

  tcgetattr(0, &new_termios);  // if current mode is already saved, gets new mode and sets it
  // sets raw mode
  new_termios.c_lflag &= ~(ICANON | ECHO);
  new_termios.c_cc[VTIME] = 0;
  new_termios.c_cc[VMIN] = 1;

  tcsetattr(0, TCSANOW, &new_termios);
}
