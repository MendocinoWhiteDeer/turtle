#include "turtle.h"

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
