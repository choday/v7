/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#ifndef VM_H_INCLUDED
#define VM_H_INCLUDED

#include "internal.h"

#if defined(__cplusplus)
extern "C" {
#endif  /* __cplusplus */

/* TODO(mkm): remove ifdef once v7 has been moved here */
#ifndef V7_VALUE_DEFINED
typedef uint64_t val_t;
#endif

#define V7_TAG_OBJECT    ((uint64_t) 0xFFFF << 48)
#define V7_TAG_FOREIGN   ((uint64_t) 0xFFFE << 48)
#define V7_TAG_UNDEFINED ((uint64_t) 0xFFFD << 48)
#define V7_TAG_BOOLEAN   ((uint64_t) 0xFFFC << 48)
#define V7_TAG_NAN       ((uint64_t) 0xFFFB << 48)
#define V7_TAG_STRING_I  ((uint64_t) 0xFFFA << 48)  /* Inlined string */
#define V7_TAG_STRING_O  ((uint64_t) 0xFFF9 << 48)  /* Owned string */
#define V7_TAG_STRING_F  ((uint64_t) 0xFFF8 << 48)  /* Foreign string */
#define V7_TAG_FUNCTION  ((uint64_t) 0xFFF7 << 48)  /* JavaScript function */
#define V7_TAG_CFUNCTION ((uint64_t) 0xFFF6 << 48)  /* C function */
#define V7_TAG_MASK      ((uint64_t) 0xFFFF << 48)

#define V7_NULL V7_TAG_FOREIGN
#define V7_UNDEFINED V7_TAG_UNDEFINED

struct v7_property {
  struct v7_property *next; /* Linkage in struct v7_object::properties */
  char *name;               /* Property name is a zero-terminated string */
  val_t value;           /* Property value */

  unsigned int attributes;
#define V7_PROPERTY_READ_ONLY    1
#define V7_PROPERTY_DONT_ENUM    2
#define V7_PROPERTY_DONT_DELETE  4
#define V7_PROPERTY_HIDDEN       8
};

/*
 * An object is an unordered collection of properties.
 * A function stored in a property of an object is called a method.
 * A property has a name, a value, and set of attributes.
 * Attributes are: ReadOnly, DontEnum, DontDelete, Internal.
 *
 * A constructor is a function that creates and initializes objects.
 * Each constructor has an associated prototype object that is used for
 * inheritance and shared properties. When a constructor creates an object,
 * the new object references the constructor’s prototype.
 *
 * Objects could be a "generic objects" which is a collection of properties,
 * or a "typed object" which also hold an internal value like String or Number.
 * Those values are implicit, unnamed properties of the respective types,
 * and can be coerced into primitive types by calling a respective constructor
 * as a function:
 *    var a = new Number(123);
 *    typeof(a) == 'object';
 *    typeof(Number(a)) == 'number';
 */
struct v7_object {
  /* First HIDDEN property in a chain is an internal object value */
  struct v7_property *properties;
  struct v7_object *prototype;
};

/*
 * Variables are function-scoped and are hoisted.
 * Lexical scoping & closures: each function has a chain of scopes, defined
 * by the lexicographic order of function definitions.
 * Scope is different from the execution context.
 * Execution context carries "variable object" which is variable/value
 * mapping for all variables defined in a function, and `this` object.
 * If function is not called as a method, then `this` is a global object.
 * Otherwise, `this` is an object that contains called method.
 * New execution context is created each time a function call is performed.
 * Passing arguments through recursion is done using execution context, e.g.
 *
 *    var factorial = function(num) {
 *      return num < 2 ? 1 : num * factorial(num - 1);
 *    };
 *
 * Here, recursion calls the same function `factorial` several times. Execution
 * contexts for each call form a stack. Each context has different variable
 * object, `vars`, with different values of `num`.
 */

struct v7_function {
  /*
   * Functions are objects. This has to be the first field so that function
   * objects can be managed by the GC.
   */
  struct v7_property *properties;
  struct v7_object *scope;    /* lexical scope of the closure */
  struct ast *ast;            /* AST, used as a byte code for execution */
  unsigned int ast_off;       /* Position of the function node in the AST */
};

/* TODO(mkm): possibly replace those with macros for inlining */
enum v7_type val_type(struct v7 *v7, val_t);
int v7_is_object(val_t);
int v7_is_function(val_t);
int v7_is_cfunction(val_t);
int v7_is_string(val_t);
int v7_is_boolean(val_t);
int v7_is_double(val_t);
int v7_is_null(val_t);
int v7_is_undefined(val_t);
int v7_is_error(val_t);
V7_PRIVATE val_t v7_pointer_to_value(void *);
V7_PRIVATE void *val_to_pointer(val_t);

val_t v7_object_to_value(struct v7_object *);
val_t v7_string_to_value(struct v7 *, const char *c, size_t len, int own);
val_t v7_function_to_value(struct v7_function *);
val_t v7_foreign_to_value(void *);
val_t v7_boolean_to_value(int);
val_t v7_double_to_value(double);
val_t v7_cfunction_to_value(v7_cfunction_t);

struct v7_object *val_to_object(val_t);
struct v7_function *val_to_function(val_t);
void *val_to_foreign(val_t);
int val_to_boolean(val_t);
double val_to_double(val_t);
v7_cfunction_t val_to_cfunction(val_t);
const char *val_to_string(struct v7 *, val_t *, size_t *);

V7_PRIVATE val_t v_get_prototype(val_t);

/* TODO(lsm): NaN payload location depends on endianness, make crossplatform */
#define GET_VAL_NAN_PAYLOAD(v) ((char *) &(v))

/*
 * Create a value with the given type.
 * Last arguments to a function depend on `type`:
 * - For `V7_TYPE_UNDEFINED` and `V7_TYPE_NULL` - no argument
 * - For `V7_TYPE_NUMBER`, `v7_num_t` argument
 * - For `V7_TYPE_BOOLEAN`, `int` argument
 * - For `V7_TYPE_STRING`, `char *` followed by `v7_strlen_t` argument
 * - For everything else, `val_t` argument. Value is not copied.
 * Return NULL on failure.
 */
val_t v7_create_value(struct v7 *, enum v7_type, ...);
val_t v7_va_create_value(struct v7 *, enum v7_type, va_list);

int v7_stringify_value(struct v7 *, val_t, char *, size_t);
int v7_to_json(struct v7 *, val_t, char *, size_t);
V7_PRIVATE char* debug_json(struct v7 *, val_t);
V7_PRIVATE struct v7_property *v7_create_property(struct v7 *);

int v7_set_property_value(struct v7 *, val_t obj,
                          const char *name, size_t len,
                          unsigned int attributes, val_t val);

/*
 * Set a property for an object.
 * `obj` must be a object value. Last arguments depend on `type` (see above).
 * Return 0 on success, -1 on error.
 */
int v7_set_property(struct v7 *, val_t obj,
                    const char *name, size_t len,
                    unsigned int attributes, enum v7_type, ...);

/* If `len` is -1/MAXUINT/~0, then `name` must be 0-terminated */
V7_PRIVATE struct v7_property *v7_get_property(val_t obj,
                                               const char *name, size_t);

/* Return address of property value or NULL if the passed property is NULL */
V7_PRIVATE val_t v7_property_value(struct v7_property *);

/*
 * If `len` is -1/MAXUINT/~0, then `name` must be 0-terminated.
 * Return 0 on success, -1 on error.
 */
V7_PRIVATE int v7_del_property(val_t, const char *, size_t);

/*
 * Returns the array length, or `-1` if the object is not an array
 */
V7_PRIVATE long v7_array_length(struct v7 *v7, val_t);

/* String API */
V7_PRIVATE int s_cmp(struct v7 *, val_t a, val_t b);
V7_PRIVATE val_t s_concat(struct v7 *, val_t, val_t);
V7_PRIVATE val_t s_substr(struct v7 *, val_t, size_t, size_t);
V7_PRIVATE void embed_string(struct mbuf *m, size_t off, const char *p, size_t);

#endif  /* VM_H_INCLUDED */