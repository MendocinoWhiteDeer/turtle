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
#include "bdwgc/include/gc/gc.h"

typedef struct Cons { void* car, * cdr; } Cons;
typedef struct Primitive
{
  char* name;
  void* (*fn)(void*, void*);
} Primitive;

void* nil;
char* truth;
char* falsity;
char* carErr;
char* cdrErr;
char* failedAssocRefErr;
char* failedApplyErr;
void* topLevel;

// Tagged objects
enum { TAG_SYM, TAG_NUM, TAG_NIL, TAG_CONS, TAG_PRIM};
void* obj(const uint8_t type, const uint64_t size)
{
  void* mem = GC_MALLOC(sizeof(uint8_t) + size);
  if (!mem)
  {
    fprintf(stderr, "obj: GC_MALLOC failed\n");
    exit(1);
  }
  memcpy(mem, &type, sizeof(uint8_t));
  return (void*)((uint64_t)mem + sizeof(uint8_t));
}
uint8_t getObjTag(const void* const x) { return *((uint8_t*)((uint64_t)x - sizeof(uint8_t))); }
uint8_t objEqual(void* x, void* y)
{
  const uint8_t tag = getObjTag(x);
  if (tag != getObjTag(y)) return 0;

  switch(tag)
  {
    case TAG_SYM: return !strcmp(x, y);
    case TAG_NUM: return *((double*)x) == *((double*)y);
    case TAG_NIL: return 1;
    case TAG_CONS:
    {
      Cons* cx = (Cons*)x;
      Cons* cy = (Cons*)y;
      return objEqual(cx->car, cy->car) && objEqual(cx->cdr, cy->cdr);
    }
    case TAG_PRIM: return *((uint8_t*)x) == *((uint8_t*)y);
    default: return 0;
  }
}

// Atoms
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

// Cons cells
Cons* cons(void* const car, void* const cdr)
{
  Cons* x = (Cons*)obj(TAG_CONS, sizeof(Cons));
  x->car = car;
  x->cdr = cdr;
  return x;
}
void* car(Cons* const x) {return getObjTag(x) == TAG_CONS ? x->car : carErr; }
void* cdr(Cons* const x) {return getObjTag(x) == TAG_CONS ? x->cdr : cdrErr; }

// Alist
void* assocRef(void* const x, void* alist)
{
  while (getObjTag(alist) == TAG_CONS && !objEqual(x, car(car(alist))))
    alist = cdr(alist);
  return getObjTag(alist) == TAG_CONS ? cdr(car(alist)) : failedAssocRefErr;
}

// Eval
void* apply(void* fn, void* arg, void* env);
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

// Primitives
void* fnCons(void* arg, void* env)
{
  void* list = evalList(arg, env);
  return cons(car(list), car(cdr(list)));
}
void* fnCar(void* arg, void* env) { return car(car(evalList(arg, env))); }
void* fnCdr(void* arg, void* env) { return cdr(car(evalList(arg, env))); }
void* fnEval(void* arg, void* env) { return eval(car(evalList(arg, env)), env); }
void* fnQuote(void* arg, void* env) { return car(arg); }
void* fnAnd(void* arg, void* env)
{
  void* x = nil;
  while (getObjTag(arg) != TAG_NIL)
  {
    x = eval(car(arg), env);
    if (getObjTag(x) == TAG_NIL) break;
    arg = cdr(arg);
  }
  return x;
}
void* fnOr(void* arg, void* env)
{
  void* x = nil;
  while (getObjTag(arg) != TAG_NIL)
  {
    x = eval(car(arg), env);
    if (getObjTag(x) != TAG_NIL) break;
    arg = cdr(arg);
  }
  return x;
}
void* fnNot(void* arg, void* env) { return getObjTag(car(evalList(arg, env))) == TAG_NIL ? truth : nil; }
void* fnAdd(void* arg, void* env)
{
  void* list = evalList(arg, env);
  double n = *((double*)car(list));
  while (getObjTag(list = cdr(list)) != TAG_NIL)
    n += *((double*)car(list));
  return number(n);
}
void* fnSub(void* arg, void* env)
{
  void* list = evalList(arg, env);
  double n = *((double*)car(list));
  uint8_t count = 0;
  while (getObjTag(list = cdr(list)) != TAG_NIL)
  {
    n -= *((double*)car(list));
    count = 1;
  }
  if (!count) n = -n;
  return number(n);
}
void* fnMul(void* arg, void* env)
{
  void* list = evalList(arg, env);
  double n = *((double*)car(list));
  while (getObjTag(list = cdr(list)) != TAG_NIL)
    n *= *((double*)car(list));
  return number(n);
}
void* fnDiv(void* arg, void* env)
{
  void* list = evalList(arg, env);
  double n = *((double*)car(list));
  while (getObjTag(list = cdr(list)) != TAG_NIL)
    n /= *((double*)car(list));
  return number(n);
}
enum { PRIM_CONS, PRIM_CAR, PRIM_CDR, PRIM_EVAL, PRIM_QUOTE,
       PRIM_AND, PRIM_OR, PRIM_NOT,
       PRIM_ADD, PRIM_SUB, PRIM_MUL, PRIM_DIV, PRIM_TOT };
Primitive primatives[PRIM_TOT] =
{
  {"cons",  fnCons},
  {"car",   fnCar},
  {"cdr",   fnCdr},
  {"eval",  fnEval},
  {"quote", fnQuote},
  {"and",   fnAnd},
  {"or",    fnOr},
  {"not",   fnNot},
  {"+",     fnAdd},
  {"-",     fnSub},
  {"*",     fnMul},
  {"/",     fnDiv}
};
void* setPrimitives(void* env)
{
  for (uint8_t i = 0; i < PRIM_TOT; i++)
  {
    uint8_t* id = obj(TAG_PRIM, sizeof(uint8_t));
    *id = i;
    env = cons(cons(symbol(primatives[i].name), id), env);
  }
  return env;
}

void* apply(void* fn, void* arg, void* env)
{
  const uint8_t tag = getObjTag(fn);
  switch (tag)
  {
    case TAG_PRIM: return primatives[*((uint8_t*)fn)].fn(arg, env);
    default: return failedApplyErr; 
  }
}

// Print
void printList(const Cons* x);
void printObj(const void* x)
{
  const uint8_t tag = getObjTag(x);
  switch(tag)
  {
    case TAG_SYM: printf("%s", (char*)x); return;
    case TAG_NUM: printf("%lf", *((double*)x)); return;
    case TAG_NIL: printf("()"); return; 
    case TAG_CONS: printList(x); return;
    case TAG_PRIM: printf("$primitive$%u", *((uint8_t*)x)); return;
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
  if (lookAt == EOF) exit(0);
}
void nextToken()
{
  uint8_t i = 0;
  while (lookAt <= ' ') peek();
  if ((lookAt == '\'') || (lookAt == '(') || (lookAt == ')'))
    { buffer[i++] = lookAt; peek(); }
  else
    do { buffer[i++] = lookAt; peek(); }
    while (i < BUFFER_SIZE - 1 && (lookAt > ' ') && (lookAt != '(') && (lookAt != ')'));
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
void* parse()
{
  switch (buffer[0])
  {
    case '\'': return cons(symbol("quote"), cons(readInput(), nil));
    case '(': return parseList();
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
  carErr = symbol("ERROR: CAR FAILED"); 
  cdrErr = symbol("ERROR: CDR FAILED");
  failedAssocRefErr = symbol("ERROR: ASSOC REF FAILED");
  failedApplyErr = symbol("ERROR: APPLY ONLY WORKS ON PRIMITIVES");
  topLevel = cons(cons(falsity, nil), cons(cons(truth, truth), nil)); // ((#f) (#t . #t))
  topLevel = setPrimitives(topLevel);
  printf("Top-Level Environment: \n");
  printObj(topLevel);
  printf("\n");

  // REPL
  while(1)
  {
    printf("\n>");
    printObj(eval(readInput(), topLevel));
  }
}
