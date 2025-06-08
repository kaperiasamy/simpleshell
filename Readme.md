# SimpleShell

A simple Unix-like shell implemented in C.

## Features

- Command execution with arguments
- Built-in commands: `cd`, `history`, `exit`
- Command history (last 10 commands)
- Input/output redirection (`>`, `>>`, `<`)
- Pipelining (`|`)
- Background execution (`&`)
- Signal handling for Ctrl+C (SIGINT)
- Displays prompt with username and current directory

## Build

```sh
gcc -Wall -O2 -o shell shell.c
```

## Usage

Run the shell:

```sh
./shell
```

### Examples

- Run a command:
  ```
  ls -l
  ```
- Change directory:
  ```
  cd /path/to/dir
  ```
- Show command history:
  ```
  history
  ```
- Redirect output:
  ```
  ls > out.txt
  ```
- Append output:
  ```
  echo hello >> out.txt
  ```
- Redirect input:
  ```
  wc -l < out.txt
  ```
- Use pipes:
  ```
  cat file.txt | grep hello | sort
  ```
- Run in background:
  ```
  sleep 10 &
  ```
- Exit the shell:
  ```
  exit
  ```

## Notes

- Only the last 10 commands are kept in history.
- If a command is not found, an error message is shown.
- The shell ignores SIGQUIT and SIGTSTP signals.

---

Original repo: https://github.com/Hussein-Hazimeh/Custum-Shell/tree/main