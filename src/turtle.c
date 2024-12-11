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

#include "turtle.h"

void panic(char* str)
{
  fprintf(stderr, "%s\n", str);
  exit(EXIT_FAILURE);
}

void* nil;
char* truth, * falsity;
void* topLevel;

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

void* apply(void* fn, void* argList, void* env)
{
  switch (getObjTag(fn))
  {
    case TAG_PRIM: return getPrimitiveFn(*((uint8_t*)fn))(argList, env);
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
uint8_t lookingAtBracket() { return (lookAt == '(') || (lookAt == ')') || (lookAt == '[') || (lookAt == ']'); }
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
  objInit();

  nil = obj(TAG_NIL, 0);
  truth = symbol("#t");
  falsity = symbol("#f");
  
  // initial top-level environment
  topLevel = nil;
  topLevel = assocCons(truth, truth, topLevel);
  topLevel = assocCons(falsity, nil, topLevel);
  topLevel = setPrimitives(topLevel);
  /*
  printf("Top-Level Environment:\n");
  printObj(topLevel);
  printf("\n");
  */
  
  // REPL
  while(1)
  {
    printf(">");
    printObj(eval(readInput(), topLevel));
    printf("\n");
  }
}
