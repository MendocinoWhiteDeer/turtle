#pragma once
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

// turtle.c ////////////////////////////////////////////////////////////////////////////////////////
void panic(char* str);

enum { TAG_SYM, TAG_STR, TAG_NUM, TAG_PRIM, TAG_CLSR, TAG_MACRO, TAG_NIL, TAG_CONS};
typedef struct Cons { void* car, * cdr; } Cons;
typedef void* (*PrimitiveFn)(void*, void*);
typedef struct Primitive { char* name; PrimitiveFn fn; } Primitive;

extern void* nil;
extern char* truth;
extern char* falsity;
extern void* topLevel;

void* eval(void* x, void* env);
void* evalList(void* x, void* env);
void* apply(void* fn, void* argList, void* env);
////////////////////////////////////////////////////////////////////////////////////////////////////

// obj.c ///////////////////////////////////////////////////////////////////////////////////////////
void objInit();
void* obj(const uint8_t type, const uint64_t size);
uint8_t getObjTag(const void* const x);
uint8_t objEqual(const void* const x, const void* const y);
////////////////////////////////////////////////////////////////////////////////////////////////////

// atom.c //////////////////////////////////////////////////////////////////////////////////////////
char* symbol(char* str);
double* number(double n);
char* string(char* str);
Cons** closure(void* argList, void* body, void* env);
Cons** macro(void* argList, void* body);
PrimitiveFn getPrimitiveFn(uint8_t index);
void* setPrimitives(void* env);
////////////////////////////////////////////////////////////////////////////////////////////////////

// cons.c //////////////////////////////////////////////////////////////////////////////////////////
Cons* cons(void* const car, void* const cdr);
void* car(Cons* const x);
void* cdr(Cons* const x);
uint64_t consCount(Cons* x);

Cons* assocCons(void* const key, void* const v, void* const alist);
void* assocRef(void* const key, void* alist);
void* assocList(void* const keyList, void* const vList, void* const alist);
////////////////////////////////////////////////////////////////////////////////////////////////////

// sys.c ///////////////////////////////////////////////////////////////////////////////////////////
void* fnCd(void* argList, void* env);
void* fnCwd(void* argList, void* env);
void* fnRun(void* argList, void* env);
void* fnDaemon(void* argList, void* env);
void* fnPipe(void* argList, void* env);
////////////////////////////////////////////////////////////////////////////////////////////////////
