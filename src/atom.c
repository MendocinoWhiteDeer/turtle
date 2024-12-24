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

Cons** closure(void* argList, void* body, void* env)
{
  Cons* c = assocCons(argList, body, objEqual(env, topLevel) ? nil : env);
  Cons** x = obj(TAG_CLSR, sizeof(Cons*));
  memcpy(x, &c, sizeof(Cons*));
  return x;
}

Cons** macro(void* argList, void* body)
{
  Cons* c = cons(argList, body);
  Cons** x = obj(TAG_MACRO, sizeof(Cons*));
  memcpy(x, &c, sizeof(Cons*));
  return x;
}

static void* fnCons(void* argList, void* env)
{
  if (consCount(argList) != 2)
    return symbol("ERROR: cons FAILED; MUST BE OF THE FORM (cons expr-1 expr-2)");
  void* l = evalList(argList, env);
  return cons(car(l), car(cdr(l)));
}

static void* fnCar(void* argList, void* env)
{
  if (consCount(argList) != 1)
    return symbol("ERROR: car FAILED; MUST BE OF THE FORM (car pair)");
  return car(eval(car(argList), env));
}

static void* fnCdr(void* argList, void* env)
{
  if (consCount(argList) != 1)
    return symbol("ERROR: cdr FAILED; MUST BE OF THE FORM (cdr pair)");
  return cdr(eval(car(argList), env));
}

static void* fnEval(void* argList, void* env)
{
  if (consCount(argList) != 1)
    return symbol("ERROR: eval FAILED; MUST BE OF THE FORM (eval expr)");
  return eval(eval(car(argList), env), env);
}

static void* fnQuote(void* argList, void* env)
{
  if (consCount(argList) != 1)
    return symbol("ERROR: quote FAILED; MUST BE OF THE FORM (quote expr)");
  return car(argList);
}

static void* fnAll(void* argList, void* env)
{
  if (!consCount(argList)) return symbol("ERROR: all FAILED; MUST BE OF THE FORM (all expr ...)");
  void* x = nil;
  for (void* l = evalList(argList, env); getObjTag(l) != TAG_NIL; l = cdr(l)) x = car(l);
  return x;
}

static void* fnLambda(void* argList, void* env) { return closure(car(argList), cdr(argList), env); }

static void* fnMacro(void* argList, void* env) { return macro(car(argList), cdr(argList)); }

static void* fnGlobal(void* argList, void* env)
{
  if (consCount(argList) != 2)
    return symbol("ERROR: global FAILED; MUST BE OF THE FORM (global variable expr)");
  void* x = car(argList);
  topLevel = assocCons(x, eval(car(cdr(argList)), env), topLevel);
  return x;
}

static void* fnAnd(void* argList, void* env)
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

static void* fnOr(void* argList, void* env)
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

static void* fnNot(void* argList, void* env)
{
  if (consCount(argList) != 1) return symbol("ERROR: not? FAILED; MUST BE OF THE FORM (not? expr)");
  return getObjTag(car(evalList(argList, env))) == TAG_NIL ? truth : nil;
}

static void* fnEq(void* argList, void* env)
{
  if (consCount(argList) != 2) return symbol("ERROR: eq? FAILED; MUST BE OF THE FORM (eq? expr-1 expr-2)");
  void* l = evalList(argList, env);
  return objEqual(car(l), car(cdr(l))) ? truth : nil;
}

static void* fnIf(void* argList, void* env)
{
  if (consCount(argList) != 3) return symbol("ERROR: if FAILED; MUST BE OF THE FORM (if test-expr then-expr else-expr);");
  const uint8_t test = getObjTag(eval(car(argList), env)) != TAG_NIL;
  argList = cdr(argList);
  return eval(car(test ? argList : cdr(argList)), env);
}

static void* fnWhen(void* argList, void* env)
{
  if (consCount(argList) < 2) return symbol("ERROR: when FAILED; MUST BE OF THE FORM (when test-expr then-expr ...);");
  const uint8_t test = getObjTag(eval(car(argList), env)) != TAG_NIL;
  return test ? fnAll(cdr(argList), env) : nil;
}

static void* fnUnless(void* argList, void* env)
{
  if (consCount(argList) < 2) return symbol("ERROR: unless FAILED; MUST BE OF THE FORM (unless test-expr then-expr ...);");
  const uint8_t test = getObjTag(eval(car(argList), env)) != TAG_NIL;
  return test ? nil : fnAll(cdr(argList), env);
}

static void* fnCond(void* argList, void* env)
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

static void* fnAdd(void* argList, void* env)
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

static void* fnSub(void* argList, void* env)
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

static void* fnMul(void* argList, void* env)
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

static void* fnDiv(void* argList, void* env)
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

static void* fnPrintf(void* argList, void* env)
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

static void* fnStringToCharList(void* argList, void* env)
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

static const Primitive primitives[] =
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

PrimitiveFn getPrimitiveFn(uint8_t index) { return primitives[index].fn; }

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
