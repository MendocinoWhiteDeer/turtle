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
