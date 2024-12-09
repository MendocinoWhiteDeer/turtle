#include "turtle.h"
#include "bdwgc/include/gc/gc.h"

void objInit() { GC_INIT(); }

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
