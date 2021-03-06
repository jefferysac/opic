#ifndef OP_TRAIT_H
#define OP_TRAIT_H 1

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "../common/op_assert.h"
#include "../common/op_macros.h"
#include "op_object_def.h"

OP_BEGIN_DECLS

Class* LPTypeMap_get(char* key);
void LPTypeMap_put(char* key, Class* value);
bool tc_isa_instance_of(Class* klass, char* trait);
bool tc_instance_of(OPObject* obj, char* trait);
uint64_t op_murmur_hash(void* ptr);

OP_END_DECLS

#define OP_METHOD_TYPE(METHOD) METHOD ## _type

#define OP_TYPECLASS_METHODS(OP_TYPE) OP_TYPE ## _OP_METHODS

#define OP_DECLARE_METHOD(METHOD, RET, ...)             \
  typedef RET OP_METHOD_TYPE(METHOD)(__VA_ARGS__);      \
  OP_METHOD_TYPE(METHOD) METHOD;


#define _OP_METHOD_DECLARE_FIELD(METHOD,...)    \
  OP_METHOD_TYPE(METHOD)* METHOD

#define OP_DECLARE_TYPECLASS(OP_TYPE)                                   \
  typedef struct OP_TYPE {                                              \
    struct TypeClass base;                                              \
    OP_MAP_SC_S0(_OP_METHOD_DECLARE_FIELD, OP_TYPECLASS_METHODS(OP_TYPE)); \
  } OP_TYPE;

// Simple fast universal hasing
// possible way to avoid collision: linear probing + simple tabular hasing
// size_t idx = ((size_t)ISA * 31) >> (sizeof(size_t)*8 - 16);
/*
  In future we should use debug flag to enable these
  printf("ISA matched. ISA: %p, fn: %p\n", method.isa, method.fn); \
  printf("ISA mismatch.\n"); \
*/
#define OP_TYPECLASS_METHOD_FACTORY(OP_TYPE, METHOD, ISA,...)           \
  do {                                                                  \
    Class* isa = (Class*)(((size_t)(ISA)) & (size_t)(~(0x0FL)));        \
    static AtomicClassMethod method_cache[16];                          \
    size_t idx = ((size_t)isa >> 3) & 0x0F;                             \
    op_assert(isa, "Class ISA is null\n");                              \
    ClassMethod method;                                                 \
    method = atomic_load(&method_cache[idx]);                           \
    OP_METHOD_TYPE(METHOD)* fn = NULL;                                  \
    if (method.isa == isa) {                                            \
      fn = (OP_METHOD_TYPE(METHOD)*) method.fn;                         \
    } else {                                                            \
      for (TypeClass** trait_it = isa->traits; *trait_it; trait_it++)   \
        {                                                               \
          if(!strcmp((*trait_it)->name, #OP_TYPE))                      \
            {                                                           \
              OP_TYPE* tc = *(OP_TYPE**) trait_it;                      \
              fn = tc->METHOD;                                          \
              break;                                                    \
            }                                                           \
        }                                                               \
      op_assert(fn,"Class %s should implement %s.%s\n",                 \
                isa->classname,#OP_TYPE,#METHOD);                       \
      method = (ClassMethod){.isa = isa, .fn = (void*) fn};             \
      atomic_store(&method_cache[idx], method);                         \
    }                                                                   \
    return fn(__VA_ARGS__);                                             \
  } while (0);

/*
#define OP_CLASS_DECLARE_OP_METHODS(KLASS) \
  void KLASS ## _init(KLASS*); \
  OP_MAP_SC_S1(_OP_CLASS_DECLARE_OP_METHOD,KLASS,

#define _OP_CLASS_DECLARE_OP_METHOD(METHOD,I,KLASS,...) \
  OP_METHOD_TYPE(METHOD) KLASS ## METHOD;
 */ 

#define OP_CLASS_OBJ(KLASS) KLASS ## _klass_

#define OP_DECLARE_ISA(KLASS)                   \
  extern Class OP_CLASS_OBJ(KLASS);

/* OPIC-32 Sadly, even if we specify the constructor functions as used
   and externally_visible, it still get striped out when link
   statically. For now we'll just keep it as-is and work with compiler
   team to fix it in future.  Note: A possible hack to use is
   __LIBGCC_CTORS_SECTION_ASM_OP__. To do so we might need to link
   with libgcc. But this is a big rework.
 */
#define OP_DEFINE_ISA(KLASS)                                            \
  Class OP_CLASS_OBJ(KLASS) __attribute__((used))                       \
    = {.classname = #KLASS, .size=sizeof(KLASS) };                      \
  __attribute__((constructor,used,externally_visible))                  \
  void define_##KLASS##_ISA() {                                         \
    LPTypeMap_put(#KLASS, &OP_CLASS_OBJ(KLASS));                        \
    OP_CLASS_OBJ(KLASS).hash = op_murmur_hash(&OP_CLASS_OBJ(KLASS));    \
  }

#define OP_DEFINE_ISA_WITH_TYPECLASSES(KLASS,...)                       \
  Class OP_CLASS_OBJ(KLASS) __attribute__((used))                       \
    = {.classname = #KLASS, .size=sizeof(KLASS) };                      \
  __attribute__((constructor,used,externally_visible))                  \
  void define_##KLASS##_ISA() {                                         \
    LPTypeMap_put(#KLASS, &OP_CLASS_OBJ(KLASS));                        \
    OP_CLASS_OBJ(KLASS).hash = op_murmur_hash(&OP_CLASS_OBJ(KLASS));    \
    OP_CLASS_OBJ(KLASS).traits =                                        \
      calloc(sizeof(void*), OP_LENGTH(__VA_ARGS__) + 1);                \
    OP_MAP_SC_S1(OP_CLASS_ADD_TYPECLASS,KLASS,__VA_ARGS__);             \
  }


#define OP_CLASS_ADD_TYPECLASS(OP_TRAIT_TYPE, SLOT, KLASS_TYPE,...)     \
  do {                                                                  \
    OP_TRAIT_TYPE* OP_TRAIT_TYPE##_var = malloc(sizeof(OP_TRAIT_TYPE)); \
    OP_TRAIT_TYPE##_var->base.name = #OP_TRAIT_TYPE;                    \
    OP_MAP_SC_S2_(_OP_METHOD_ASSIGN_IMPL,                               \
                  OP_TRAIT_TYPE, KLASS_TYPE,                            \
                  OP_TYPECLASS_METHODS(OP_TRAIT_TYPE));                 \
    OP_CLASS_OBJ(KLASS_TYPE).traits[SLOT] =                             \
      (TypeClass*) OP_TRAIT_TYPE##_var;                                 \
  } while (0);

#define _OP_METHOD_ASSIGN_IMPL(METHOD,I,OP_TRAIT_TYPE,KLASS_TYPE,...)   \
  OP_TRAIT_TYPE##_var->METHOD = &KLASS_TYPE##_##METHOD


#endif /* OP_TRAIT_H */
