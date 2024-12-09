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

void* nil;
char* truth, * falsity;
void* topLevel;

void panic(char* str)
{
  fprintf(stderr, "%s\n", str);
  exit(EXIT_FAILURE);
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
  if (consCount(argList) != 1) return symbol("ERROR: not? FAILED; MUST BE OF THE FORM (not? expr)");
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
    char* fmt = malloc(sizeof(char) * len);
    if (!fmt) panic("fnPrintf(): malloc failed");
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
    free(fmt);
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
  {"not?",              fnNot},
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
  for (uint8_t i = 0; i < sizeof(primitives) / sizeof(Primitive); i++)
  {
    uint8_t* id = obj(TAG_PRIM, sizeof(uint8_t));
    *id = i;
    topLevel = assocCons(symbol(primitives[i].name), id, topLevel);
  }
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
