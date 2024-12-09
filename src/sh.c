#include "turtle.h"

static pid_t frk()
{
  pid_t pid = fork();
  if (pid == -1) panic("frk() failed");
  return pid;
}

void* fnCd(void* argList, void* env)
{
  char* err =  "ERROR: cd FAILED; MUST BE OF THE FORM (cd string)";
  if (consCount(argList) != 1) return symbol(err);
  char* str = eval(car(argList), env);
  if (getObjTag(str) != TAG_STR) return symbol(err);
  return chdir(str) ? nil : str;
}

void* fnCwd(void* argList, void* env)
{
  char* err =  "ERROR: cwd FAILED; MUST BE OF THE FORM (cwd)";
  if (consCount(argList)) return symbol(err);

  // glibc extends POSIX; getcwd will allocate the memory if you give it the appropriate args
  char* buf = getcwd(NULL, 0);
  char* x = buf ? string(buf) : nil; 
  free(buf);
  return x;
}

char** parseExecArgs(char* str)
{
  uint64_t capacity = 32, size = 0;
  char** execArgs = malloc(capacity * sizeof(char*));
  if (!execArgs) panic("fnRun(): malloc failed");
  char* delim = " \t\n\r\f\v";
  uint8_t reachedEnd = 0;
  for (char* tkn = strtok(str, delim); !reachedEnd; tkn = strtok(NULL, delim))
  {
    if (size + 1 > capacity)
      {
	capacity *= 8;
	execArgs = realloc(execArgs, capacity * sizeof(char*));
	if (!execArgs) panic("fnRun(): realloc failed");
      }
    execArgs[size++] = tkn;
    if (!tkn) reachedEnd = 1; // stop when we store all the tokens and the NULL-terminator into the arg list
  }
  return execArgs;
}

void* fnRun(void* argList, void* env)
{
  char* err =  "ERROR: run FAILED; MUST BE OF THE FORM (run arg-string ...)";
  if (consCount(argList) < 1) return symbol(err);
  uint8_t allSuccess = 1;
  for (void* l = evalList(argList, env); getObjTag(l) != TAG_NIL; l = cdr(l))
  {
    char* x = car(l);
    if (getObjTag(x) != TAG_STR) return symbol(err);

    // child
    if (!frk())
    {
      char** execArgs = parseExecArgs(x);
      execvp(execArgs[0], execArgs);
      free(execArgs);
      exit(EXIT_FAILURE);
    }

    // parent
    {
      int status;
      if (wait(&status) == -1) panic("fnRun(); wait() failed");
      if (WIFEXITED(status)) { if(WEXITSTATUS(status)) allSuccess = 0; }
    }
  }
  return allSuccess ? truth : nil;
}

void* fnDaemon(void* argList, void* env)
{
  char* err =  "ERROR: daemon FAILED; MUST BE OF THE FORM (daemon arg-string)";
  if (consCount(argList) != 1) return symbol(err);
  char* x = eval(car(argList), env);
  if (getObjTag(x) != TAG_STR) return symbol(err);
  if (!frk())
  {
    char** execArgs = parseExecArgs(x);
    execvp(execArgs[0], execArgs);
    exit(EXIT_FAILURE);
  }
  return truth;
}

uint8_t pipeHelper(void* evalArgList, char** execArgsOut)
{
  int pipefd[2];
  evalArgList = cdr(evalArgList);
  const uint8_t isChild = (getObjTag(evalArgList) != TAG_NIL) ? 1 : 0;
  if (isChild)
    if (pipe(pipefd) == -1) panic("fnPipe(); pipe() failed");

  // child 1
  if (!frk())
  {
    if (isChild)
    {
      close(STDOUT_FILENO);
      dup(pipefd[1]);
      close(pipefd[0]);
      close(pipefd[1]);
    }
    execvp(execArgsOut[0], execArgsOut);
    exit(EXIT_FAILURE);
  }

  // child 2
  if (isChild && !frk())
  {
    close(STDIN_FILENO);
    dup(pipefd[0]);
    close(pipefd[0]);
    close(pipefd[1]);
    char** execArgsIn = parseExecArgs(car(evalArgList));
    pipeHelper(evalArgList, execArgsIn) ? exit(EXIT_SUCCESS) : exit(EXIT_FAILURE);
  }
    
  // parent
  uint8_t allSuccess = 1;
  {
    if (isChild)
    {
      close(pipefd[0]);
      close(pipefd[1]);
    }
    pid_t w[2];
    w[1] = 0;
    for(uint8_t i = 0; i < 1 + isChild; i++)
    {
      int status;
      w[i] = wait(&status);
      if (w[i] == -1) continue;
      if (WIFEXITED(status)) { if(WEXITSTATUS(status)) allSuccess = 0; }
    }
    if ((w[0] == -1) || (w[1] == -1)) panic("fnPipe(); C wait() failed");
  }
  return allSuccess;
}

void* fnPipe(void* argList, void* env)
{
  char* err =  "ERROR: pipe FAILED; MUST BE OF THE FORM (pipe arg-string-1 arg-string-2 ...)";
  if (consCount(argList) < 2) return symbol(err);
  void* l = evalList(argList, env);
  for (void* ll = l; getObjTag(ll) != TAG_NIL; ll = cdr(ll))
    if (getObjTag(car(ll)) != TAG_STR) return symbol(err);
  char** execArgsOut = parseExecArgs(car(l));
  const uint8_t allSuccess = pipeHelper(l, execArgsOut);
  free(execArgsOut);
  return allSuccess ? truth : nil;
}
