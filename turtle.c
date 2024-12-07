/*

This file is part of turtle.
Copyright (C) 2024 Taylor Wampler

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "bdwgc/include/gc/gc.h"

void* nil;
char* truth;
char* falsity;
void* topLevel;

void panic(char* str)
{
  fprintf(stderr, "%s\n", str);
  exit(EXIT_FAILURE);
}

pid_t frk()
{
  pid_t pid = fork();
  if (pid == -1) panic("frk() failed");
  return pid;
}

// Tagged objects
enum { TAG_SYM, TAG_STR, TAG_NUM, TAG_PRIM, TAG_CLSR, TAG_MACRO, TAG_NIL, TAG_CONS};
typedef struct Cons { void* car, * cdr; } Cons;
typedef struct Primitive { char* name; void* (*fn)(void*, void*); } Primitive;
void* obj(const uint8_t type, const uint64_t size)
{  
  void* mem = GC_MALLOC(sizeof(uint8_t) + size);
  if (!mem) panic("obj(): GC_MALLOC failed");
  memcpy(mem, &type, sizeof(uint8_t));
  return (void*)((uint64_t)mem + sizeof(uint8_t));
}
uint8_t getObjTag(const void* const x) { return *((uint8_t*)((uint64_t)x - sizeof(uint8_t))); }
uint8_t objEqual(const void* const x, const void* const y)
{
  const uint8_t tag = getObjTag(x);
  if (tag != getObjTag(y)) return 0;

  switch(tag)
  {
    case TAG_SYM: case TAG_STR: return !strcmp(x, y);
    case TAG_NUM: return *((double*)x) == *((double*)y); 
    case TAG_PRIM: return *((uint8_t*)x) == *((uint8_t*)y);
    case TAG_CLSR: case TAG_MACRO:
    {
      Cons* cx = *((Cons**)x);
      Cons* cy = *((Cons**)y);
      return objEqual(cx->car, cy->car) && objEqual(cx->cdr, cy->cdr);
    }
    case TAG_NIL: return 1;
    case TAG_CONS:
    {
      Cons* cx = (Cons*)x;
      Cons* cy = (Cons*)y;
      return objEqual(cx->car, cy->car) && objEqual(cx->cdr, cy->cdr);
    }
    default: return 0;
  }
}

char* symbol(char* str)
{
  const size_t len = strlen(str) + 1;
  char* x = (char*)obj(TAG_SYM, len);
  memcpy(x, str, len);
  return x;
}

double* number(double n)
{
  double* x = (double*)obj(TAG_NUM, sizeof(double));
  *x = n;
  return x;
}

char* string(char* str)
{
  const size_t len = strlen(str) + 1;
  char* x = (char*)obj(TAG_STR, len);
  memcpy(x, str, len);
  return x;
}

// Cons pair
Cons* cons(void* const car, void* const cdr)
{
  Cons* x = (Cons*)obj(TAG_CONS, sizeof(Cons));
  x->car = car;
  x->cdr = cdr;
  return x;
}
void* car(Cons* const x) {return getObjTag(x) == TAG_CONS ? x->car : symbol("ERROR: car FAILED"); }
void* cdr(Cons* const x) {return getObjTag(x) == TAG_CONS ? x->cdr : symbol("ERROR: cdr FAILED"); }
uint64_t consCount(Cons* x)
{
  uint64_t count = 0;
  while (getObjTag(x) == TAG_CONS)
  {
    count++;
    x = ((Cons*)x)->cdr;
  }
  return count;
}

// Alist
Cons* assocCons(void* const key, void* const v, void* const alist) { return cons(cons(key, v), alist); }
void* assocRef(void* const key, void* alist)
{
  while (getObjTag(alist) == TAG_CONS && !objEqual(key, car(car(alist))))
    alist = cdr(alist);
  return getObjTag(alist) == TAG_CONS ? cdr(car(alist)) : symbol("ERROR: ASSOC REF FAILED");
}
void* assocList(void* const keyList, void* const vList, void* const alist)
{
  switch (getObjTag(keyList))
  {
    case TAG_NIL: return alist;
    case TAG_CONS: return assocList(cdr(keyList), cdr(vList), assocCons(car(keyList), car(vList), alist));
    default: return assocCons(keyList, vList, alist);
  }
}
  
void* apply(void* fn, void* argList, void* env);
void* eval(void* x, void* env)
{
  switch (getObjTag(x))
  {
    case TAG_SYM: return assocRef(x, env);
    case TAG_CONS: return apply(eval(car(x), env), cdr(x), env);
    default: return x;
  }
}
void* evalList(void* x, void* env)
{
  switch (getObjTag(x))
  {
    case TAG_SYM: return assocRef(x, env);
    case TAG_CONS: return cons(eval(car(x), env), evalList(cdr(x), env));
    default: return nil;
  }
}

// Closures
Cons** closure(void* argList, void* body, void* env)
{
  Cons* c = assocCons(argList, body, objEqual(env, topLevel) ? nil : env);
  Cons** x = obj(TAG_CLSR, sizeof(Cons*));
  memcpy(x, &c, sizeof(Cons*));
  return x;
}

// Macros
Cons** macro(void* argList, void* body)
{
  Cons* c = cons(argList, body);
  Cons** x = obj(TAG_MACRO, sizeof(Cons*));
  memcpy(x, &c, sizeof(Cons*));
  return x;
}

// Primitives
void* fnCons(void* argList, void* env)
{
  if (consCount(argList) != 2)
    return symbol("ERROR: cons FAILED; MUST BE OF THE FORM (cons expr-1 expr-2)");
  void* l = evalList(argList, env);
  return cons(car(l), car(cdr(l)));
}
void* fnCar(void* argList, void* env)
{
  if (consCount(argList) != 1)
    return symbol("ERROR: car FAILED; MUST BE OF THE FORM (car pair)");
  return car(eval(car(argList), env));
}
void* fnCdr(void* argList, void* env)
{
  if (consCount(argList) != 1)
    return symbol("ERROR: cdr FAILED; MUST BE OF THE FORM (cdr pair)");
  return cdr(eval(car(argList), env));
}
void* fnEval(void* argList, void* env)
{
  if (consCount(argList) != 1)
    return symbol("ERROR: eval FAILED; MUST BE OF THE FORM (eval expr)");
  return eval(eval(car(argList), env), env);
}
void* fnQuote(void* argList, void* env)
{
  if (consCount(argList) != 1)
    return symbol("ERROR: quote FAILED; MUST BE OF THE FORM (quote expr)");
  return car(argList);
}
void* fnAll(void* argList, void* env)
{
  if (!consCount(argList)) return symbol("ERROR: all FAILED; MUST BE OF THE FORM (all expr ...)");
  void* x = nil;
  for (void* l = evalList(argList, env); getObjTag(l) != TAG_NIL; l = cdr(l)) x = car(l);
  return x;
}
void* fnLambda(void* argList, void* env) { return closure(car(argList), cdr(argList), env); }
void* fnMacro(void* argList, void* env) { return macro(car(argList), cdr(argList)); }
void* fnGlobal(void* argList, void* env)
{
  if (consCount(argList) != 2)
    return symbol("ERROR: global FAILED; MUST BE OF THE FORM (global variable expr)");
  void* x = car(argList);
  topLevel = assocCons(x, eval(car(cdr(argList)), env), topLevel);
  return x;
}
void* fnAnd(void* argList, void* env)
{
  if (!consCount(argList)) return symbol("ERROR: and FAILED; MUST BE OF THE FORM (and expr ...)");
  void* x = nil;
  for (; getObjTag(argList) != TAG_NIL; argList = cdr(argList))
  {
    x = eval(car(argList), env);
    if (getObjTag(x) == TAG_NIL) break;
  }
  return x;
}
void* fnOr(void* argList, void* env)
{
  if (!consCount(argList)) return symbol("ERROR: or FAILED; MUST BE OF THE FORM (or expr ...)");
  void* x = nil;
  for (; getObjTag(argList) != TAG_NIL; argList = cdr(argList))
  {
    x = eval(car(argList), env);
    if (getObjTag(x) != TAG_NIL) break;
  }
  return x;
}
void* fnNot(void* argList, void* env)
{
  if (consCount(argList) != 1) return symbol("ERROR: not FAILED; MUST BE OF THE FORM (not expr)");
  return getObjTag(car(evalList(argList, env))) == TAG_NIL ? truth : nil;
}
void* fnEq(void* argList, void* env)
{
  if (consCount(argList) != 2) return symbol("ERROR: eq? FAILED; MUST BE OF THE FORM (eq? expr-1 expr-2)");
  void* l = evalList(argList, env);
  return objEqual(car(l), car(cdr(l))) ? truth : nil;
}
void* fnIf(void* argList, void* env)
{
  if (consCount(argList) != 3) return symbol("ERROR: if FAILED; MUST BE OF THE FORM (if test-expr then-expr else-expr);");
  const uint8_t test = getObjTag(eval(car(argList), env)) != TAG_NIL;
  argList = cdr(argList);
  return eval(car(test ? argList : cdr(argList)), env);
}
void* fnWhen(void* argList, void* env)
{
  if (consCount(argList) < 2) return symbol("ERROR: when FAILED; MUST BE OF THE FORM (when test-expr then-expr ...);");
  const uint8_t test = getObjTag(eval(car(argList), env)) != TAG_NIL;
  return test ? fnAll(cdr(argList), env) : nil;
}
void* fnUnless(void* argList, void* env)
{
  if (consCount(argList) < 2) return symbol("ERROR: unless FAILED; MUST BE OF THE FORM (unless test-expr then-expr ...);");
  const uint8_t test = getObjTag(eval(car(argList), env)) != TAG_NIL;
  return test ? nil : fnAll(cdr(argList), env);
}
void* fnCond(void* argList, void* env)
{
  char* err = "ERROR: cond FAILED; MUST BE OF THE FORM (cond clause ...) WHERE clause is of the form (test-expr then-expr ...)";
  if (!consCount(argList)) return symbol(err);
  for (void* l = argList; getObjTag(l) != TAG_NIL; l = cdr(l))
    if (consCount(car(l)) < 2) return symbol(err);
  for (; getObjTag(argList) != TAG_NIL; argList = cdr(argList))
  {
    void * test_expr = car(car(argList));
    if (getObjTag(eval(test_expr, env)) != TAG_NIL) break;
  }
  void* then_list = cdr(car(argList));
  return fnAll(then_list, env);
}
void* fnAdd(void* argList, void* env)
{
  char* err = "ERROR: + FAILED; MUST BE OF THE FORM (+ number ...)";
  if (!consCount(argList)) return symbol(err);
  void* l = evalList(argList, env);
  double n = *((double*)car(l));
  while (getObjTag(l = cdr(l)) != TAG_NIL)
  {
    void* x = car(l);
    if (getObjTag(x) != TAG_NUM) return symbol(err);
    n += *((double*)x);
  }
  return number(n);
}
void* fnSub(void* argList, void* env)
{
  char* err = "ERROR: - FAILED; MUST BE OF THE FORM (- number ...)";
  if (!consCount(argList)) return symbol(err);
  void* l = evalList(argList, env);
  double n = *((double*)car(l));
  uint8_t count = 0;
  while (getObjTag(l = cdr(l)) != TAG_NIL)
  {
    void* x = car(l);
    if (getObjTag(x) != TAG_NUM) return symbol(err);
    n -= *((double*)x);
    count = 1;
  }
  if (!count) n = -n;
  return number(n);
}
void* fnMul(void* argList, void* env)
{
  char* err =  "ERROR: * FAILED; MUST BE OF THE FORM (* number ...)";
  if (!consCount(argList)) return symbol(err);
  void* l = evalList(argList, env);
  double n = *((double*)car(l));
  while (getObjTag(l = cdr(l)) != TAG_NIL)
  {
    void* x = car(l);
    if (getObjTag(x) != TAG_NUM) return symbol(err);
    n *= *((double*)x);
  }
  return number(n);
}
void* fnDiv(void* argList, void* env)
{
  char* err =  "ERROR: / FAILED; MUST BE OF THE FORM (/ number ...)";
  if (!consCount(argList)) return symbol(err);
  void* l = evalList(argList, env);
  double n = *((double*)car(l));
  while (getObjTag(l = cdr(l)) != TAG_NIL)
  {
    void* x = car(l);
    if (getObjTag(x) != TAG_NUM) return symbol(err);
    n /= *((double*)x);
  }
  return number(n);
}
void* fnPrintf(void* argList, void* env)
{
  char* err = "ERROR: printf FAILED; MUST BE OF THE FORM (printf string)";
  if (!consCount(argList)) return symbol(err);
  char* x = (char*)nil;
  for (void* l = evalList(argList, env); getObjTag(l) != TAG_NIL; l = cdr(l))
  {
    x = (char*)car(l);
    if (getObjTag(x) != TAG_STR) return symbol(err);

    uint64_t i = 0, j = 0, len = strlen(x) + 1;
    char* fmt = GC_MALLOC(sizeof(char) * len);
    for (; x[i] != '\0'; i++)
    {
      if (x[i] == '\\' && i < len - 1)
      {
	switch(x[i + 1])
	{
	  case 'n': fmt[j++]= '\n'; i++; break;
	  case 't': fmt[j++] = '\t'; i++; break;
	  default: fmt[j++] = x[i++]; break;
	}
      }
      else fmt[j++] = x[i];
    }
    fmt[j] = '\0';
    printf("%s", fmt);
    GC_FREE(fmt);
  }
  return x;
}
void* fnStringToCharList(void* argList, void* env)
{
  char* err =  "ERROR: string->char-list FAILED; MUST BE OF THE FORM (string->char-list string)";
  if (consCount(argList) != 1) return symbol(err);
  char* str = eval(car(argList), env);
  if (getObjTag(str) != TAG_STR) return symbol(err);
  void* x = nil;
  for (uint64_t i = 0; str[i] != '\0'; i++)
    x = cons(number(str[i]), x);
  return x;
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
const Primitive primitives[] =
{
  // fundamental
  {"cons",              fnCons},
  {"car",               fnCar},
  {"cdr",               fnCdr},
  {"eval",              fnEval},
  {"quote",             fnQuote},
  {"all",               fnAll},
  {"lambda",            fnLambda},
  {"macro",             fnMacro},
  {"global",            fnGlobal},
  
  // logical operators
  {"and",               fnAnd},
  {"or",                fnOr},
  {"not",               fnNot},
  {"eq?",               fnEq},

  // control flow
  {"if",                fnIf},
  {"when",              fnWhen},
  {"unless",            fnUnless},
  {"cond",              fnCond},

  // arithmetic
  {"+",                 fnAdd},
  {"-",                 fnSub},
  {"*",                 fnMul},
  {"/",                 fnDiv},

  // string
  {"printf",            fnPrintf},
  {"string->char-list", fnStringToCharList},

  // system
  {"cd",                fnCd},
  {"cwd",               fnCwd},
  {"run",               fnRun},
  {"daemon",            fnDaemon},
  {"pipe",              fnPipe}
};
void* setPrimitives(void* env)
{
  for (uint8_t i = 0; i < sizeof(primitives) / sizeof(Primitive); i++)
  {
    uint8_t* id = obj(TAG_PRIM, sizeof(uint8_t));
    *id = i;
    env = assocCons(symbol(primitives[i].name), id, env);
  }
  return env;
}

void* apply(void* fn, void* argList, void* env)
{
  switch (getObjTag(fn))
  {
    case TAG_PRIM: return primitives[*((uint8_t*)fn)].fn(argList, env);
    case TAG_CLSR:
    {
      Cons* c = *((Cons**)fn);
      void* cc = car(c), * clsrArgList = car(cc), * clsrBody = cdr(cc), * e = (getObjTag(cdr(c)) == TAG_NIL) ? env : nil;
      e = assocList(clsrArgList, evalList(argList, env), e);
      void* x = nil;
      for (void* l = evalList(clsrBody, e); getObjTag(l) != TAG_NIL; l = cdr(l)) x = car(l);
      return x;
    }
    case TAG_MACRO:
    {
      Cons* c = *((Cons**)fn);
      void* macroArgList = car(c), * macroBody = cdr(c), * e = assocList(macroArgList, argList, env);
      void* x = nil;
      for (void* l = evalList(evalList(macroBody, e), env); getObjTag(l) != TAG_NIL; l = cdr(l)) x = car(l);
      return x;
    }
    default: return symbol("ERROR: APPLY FAILED; APPLY ONLY ACCEPTS OBJECTS WITH TAG_PRIM, TAG_CLSR, or TAG_MACRO"); 
  }
}

// Print
void printList(const Cons* x);
void printObj(const void* x)
{
  switch(getObjTag(x))
  {
    case TAG_SYM: printf("%s", (char*)x); return;
    case TAG_NUM: printf("%lf", *((double*)x)); return;
    case TAG_STR: printf("\"%s\"", (char*)x); return;
    case TAG_NIL: printf("()"); return; 
    case TAG_CONS: printList(x); return;
    case TAG_PRIM: printf("<primitive>%u", *((uint8_t*)x)); return;
    case TAG_CLSR: printf("<closure>%p", *((Cons**)x)); return;
    case TAG_MACRO: printf("<macro>%p", *((Cons**)x)); return; 
    default: printf("Object has invalid type"); return;
  }
}
void printList(const Cons* x)
{
  printf("(");
  while (1)
  {
    printObj(x->car);
    x = x->cdr;
    const uint8_t tag = getObjTag(x); 
    if (tag == TAG_NIL) break;
    if (tag != TAG_CONS)
    {
      printf(" . ");
      printObj(x);
      break;
    }
    printf(" ");
  }
  printf(")");
}

// Read
#define BUFFER_SIZE 64
char buffer[BUFFER_SIZE];
int lookAt = ' ';
void peek()
{
  lookAt = getchar();
  if (lookAt == ';')
    while (lookAt != '\n' && lookAt != EOF) lookAt = getchar();
  if (lookAt == EOF) exit(EXIT_SUCCESS);
}
uint8_t lookingAtBracket() { return  (lookAt == '(') || (lookAt == ')') || (lookAt == '[') || (lookAt == ']'); }
void nextToken()
{
  uint8_t i = 0;
  while (lookAt <= ' ') peek();
  if ((lookAt == '\'') || lookingAtBracket())
    { buffer[i++] = lookAt; peek(); }
  else if (lookAt == '"') // copy [" t o k e n \0] to buffer; lead with " for parsing
  {
    do { buffer[i++] = lookAt; peek(); } while (i < BUFFER_SIZE - 1 && (lookAt != '"') && (lookAt != '\n'));
    if (lookAt != '"') fprintf(stderr, "nextToken: missing closing double quote\n");
    peek();
  }
  else
    do { buffer[i++] = lookAt; peek(); } while (i < BUFFER_SIZE - 1 && (lookAt > ' ') && !lookingAtBracket());
  buffer[i] = '\0';
}
void* parse();
void* readInput() { nextToken(); return parse(); }
void* parseList()
{
  nextToken();
  if (buffer[0] == ')') return nil;
  if(!strcmp(buffer, "."))
  {
    void* x = readInput();
    nextToken();
    return x;
  }
  
  void* x = parse();
  return cons(x, parseList());
}
void* parseListSquare()
{
  nextToken();
  if (buffer[0] == ']') return nil;
  if(!strcmp(buffer, "."))
  {
    void* x = readInput();
    nextToken();
    return x;
  }
  
  void* x = parse();
  return cons(x, parseListSquare());
}
void* parse()
{
  switch (buffer[0])
  {
    case '\'': return cons(symbol("quote"), cons(readInput(), nil));
    case '"': return string(buffer + 1);
    case '(': return parseList();
    case '[': return parseListSquare();
    default:
    {
      double n;
      int i;
      if (sscanf(buffer, "%lf%n", &n, &i) > 0 && buffer[i] == '\0')
	return number(n);
      else
	return symbol(buffer);
    }
  }
} 

int main()
{
  GC_INIT();

  // top-level environment
  nil = obj(TAG_NIL, 0);
  truth = symbol("#t");
  falsity = symbol("#f");
  topLevel = cons(cons(falsity, nil), cons(cons(truth, truth), nil)); // ((#f) (#t . #t))
  topLevel = setPrimitives(topLevel);
  printf("Top-Level Environment:\n");
  printObj(topLevel);
  printf("\n");

  // REPL
  while(1)
  {
    printf(">");
    printObj(eval(readInput(), topLevel));
    printf("\n");
  }
}
