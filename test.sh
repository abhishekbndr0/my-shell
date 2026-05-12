#!/bin/bash

# test.sh - basic tests for my-shell

SHELL_PATH="./shell"
PASS=0
FAIL=0
SKIP=0

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

# check the shell binary exists
if [ ! -f "$SHELL_PATH" ]; then
  echo "Error: shell binary not found at $SHELL_PATH. Run 'make' first."
  exit 1
fi

# on macOS, /tmp is a symlink to /private/tmp — resolve it
TMP=$(cd /tmp && pwd -P)

run_test() {
  local name="$1"
  local input="$2"
  local expected="$3"

  # strip "Good bye!!" and the prompt from output
  actual=$(printf "%s\nexit\n" "$input" | $SHELL_PATH 2>/dev/null \
    | grep -v "^Good bye" \
    | grep -v "^myshell>")

  if [ "$actual" = "$expected" ]; then
    echo -e "${GREEN}PASS${NC} $name"
    PASS=$((PASS + 1))
  else
    echo -e "${RED}FAIL${NC} $name"
    echo "  expected: |$(echo "$expected" | head -3)|"
    echo "  actual:   |$(echo "$actual"   | head -3)|"
    FAIL=$((FAIL + 1))
  fi
}

skip_test() {
  local name="$1"
  local reason="$2"
  echo -e "${YELLOW}SKIP${NC} $name ($reason)"
  SKIP=$((SKIP + 1))
}

echo ""
echo "=== Basic Command Execution ==="
run_test "echo" \
  "echo hello" \
  "hello"

run_test "echo multiple args" \
  "echo one two three" \
  "one two three"

echo ""
echo "=== I/O Redirection ==="

# output redirection — check the file, not stdout
printf "echo redirected > $TMP/test_redir_out\nexit\n" | $SHELL_PATH > /dev/null 2>/dev/null
if [ "$(cat $TMP/test_redir_out 2>/dev/null)" = "redirected" ]; then
  echo -e "${GREEN}PASS${NC} output redirection (>)"
  PASS=$((PASS + 1))
else
  echo -e "${RED}FAIL${NC} output redirection (>)"
  FAIL=$((FAIL + 1))
fi

# input redirection
echo "hello from file" > $TMP/test_input_file
run_test "input redirection (<)" \
  "cat < $TMP/test_input_file" \
  "hello from file"

# append redirection
rm -f $TMP/test_append
printf "echo line1 >> $TMP/test_append\necho line2 >> $TMP/test_append\nexit\n" \
  | $SHELL_PATH > /dev/null 2>&1
expected_append=$(printf "line1\nline2")
actual_append=$(cat $TMP/test_append 2>/dev/null)
if [ "$actual_append" = "$expected_append" ]; then
  echo -e "${GREEN}PASS${NC} append redirection (>>)"
  PASS=$((PASS + 1))
else
  echo -e "${RED}FAIL${NC} append redirection (>>)"
  FAIL=$((FAIL + 1))
fi

echo ""
echo "=== Pipes ==="
# compute expected from bash so whitespace matches whatever the system's wc produces
run_test "single pipe" \
  "echo hello world | wc -w" \
  "$(echo hello world | wc -w)"

run_test "multi pipe" \
  "echo one two three | wc -w" \
  "$(echo one two three | wc -w)"

echo ""
echo "=== Built-in Commands ==="

# use echo ${VAR} instead of printenv VAR — our printenv dumps all env vars
run_test "setenv and echo var" \
  "setenv TESTVAR hello
echo \${TESTVAR}" \
  "hello"

run_test "unsetenv clears var" \
  "setenv TESTVAR hello
unsetenv TESTVAR
echo \${TESTVAR}" \
  ""

run_test "cd and pwd" \
  "cd $TMP
pwd" \
  "$TMP"

run_test "cd with no args goes home" \
  "cd
pwd" \
  "$HOME"

echo ""
echo "=== Environment Variable Expansion ==="
run_test "basic env var" \
  "setenv MYVAR world
echo hello \${MYVAR}" \
  "hello world"

run_test "\${?} last exit code (success)" \
  "echo hello
echo \${?}" \
  "hello
0"

run_test "\${_} last argument" \
  "echo foo bar
echo \${_}" \
  "foo bar
bar"

run_test "\${SHELL} is set" \
  "echo \${SHELL}" \
  "$(cd "$(dirname "$SHELL_PATH")" && pwd)/$(basename "$SHELL_PATH")"

echo ""
echo "=== Quotes and Escape Characters ==="
run_test "double quotes preserve spaces" \
  'echo "hello   world"' \
  "hello   world"

run_test "escape space" \
  'echo hello\ world' \
  "hello world"

run_test "escape ampersand" \
  'echo hello \& world' \
  "hello & world"

echo ""
echo "=== Tilde Expansion ==="
run_test "tilde expands to HOME" \
  "echo ~" \
  "$HOME"

echo ""
echo "=== Wildcarding ==="
mkdir -p $TMP/test_wildcard
touch $TMP/test_wildcard/file1.txt $TMP/test_wildcard/file2.txt $TMP/test_wildcard/other.log

run_test "wildcard *.txt" \
  "cd $TMP/test_wildcard
echo *.txt" \
  "file1.txt file2.txt"

run_test "wildcard ?" \
  "cd $TMP/test_wildcard
echo file?.txt" \
  "file1.txt file2.txt"

echo ""
echo "=== Subshell ==="
if [ "$(uname)" = "Darwin" ]; then
  skip_test "subshell basic" "uses /proc/self/exe which is Linux-only"
  skip_test "subshell captures output" "uses /proc/self/exe which is Linux-only"
else
  run_test "subshell basic" \
    'echo $(echo hello)' \
    "hello"

  run_test "subshell captures output" \
    'echo $(echo one two three | wc -w | tr -d " ")' \
    "3"
fi

echo ""
echo "=== Source Built-in ==="
echo "echo sourced" > $TMP/test_source.sh
run_test "source executes file" \
  "source $TMP/test_source.sh" \
  "sourced"

echo ""
echo "=== Background Processes ==="
# perl alarm is available on both Mac and Linux as a cross-platform timeout
perl -e 'alarm 3; exec @ARGV' \
  bash -c "printf 'sleep 1 &\nexit\n' | $SHELL_PATH > /dev/null 2>&1" 2>/dev/null
if [ $? -eq 0 ]; then
  echo -e "${GREEN}PASS${NC} background process doesn't hang"
  PASS=$((PASS + 1))
else
  echo -e "${RED}FAIL${NC} background process hung or crashed"
  FAIL=$((FAIL + 1))
fi

# cleanup
rm -f $TMP/test_redir_out $TMP/test_input_file $TMP/test_append $TMP/test_source.sh
rm -rf $TMP/test_wildcard

echo ""
echo "--------------------------------------------------"
echo "Results: $PASS passed, $FAIL failed, $SKIP skipped (out of $((PASS + FAIL + SKIP)) tests)"
echo "--------------------------------------------------"
echo ""

[ $FAIL -eq 0 ] && exit 0 || exit 1
