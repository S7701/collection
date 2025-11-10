/* micro lisp (C) A. Carl Douglas */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define debug(m,e) printf("%s:%d: %s:",__FILE__,__LINE__,m); print_obj(e,1); puts("");

typedef struct Object {
  enum { _List, _Symbol, _Primitive, _Closure } tag;
  union {
    struct List {
      struct Object * head;
      struct Object * tail;
    } list;
    char * string;
    struct Object * (*pfn) (struct Object *);
    struct Closure {
      struct Object * params;
      struct Object * body;
      struct Object * env;
    } closure;
  } value;
} Object;

static Object *symbols = 0, *sym_quote, *sym_if, *sym_lambda, *sym_apply, *exp_true, *exp_false = 0;

#define SYMBOL_MAX 256

static int look; /* look ahead character */
static char token[SYMBOL_MAX+1]; /* token */

int is_space(char x)  { return x == ' ' || x == '\t' || x == '\n'; }
int is_parens(char x) { return x == '(' || x == ')'; }

Object *get_head(Object *x) { return x->value.list.head; }
Object *get_tail(Object *x) { return x->value.list.tail; }

static void read_token() {
  int index = 0;
  while(is_space(look)) { look = getchar(); }
  if (is_parens(look)) {
    token[index++] = look;  look = getchar();
  } else {
    while( && look != EOF && !is_space(look) && !is_parens(look)) {
      if (index < SYMBOL_MAX)
        token[index++] = look;
      look = getchar();
    }
  }
  token[index] = '\0';
}

Object *new_list(Object *_car, Object *_cdr) {
  Object *obj = calloc( 1, sizeof (Object) );
  obj->tag = _List;
  obj->value.list.head = _car;
  obj->value.list.tail = _cdr;
  return obj;
}

Object *new_symbol(char *str) {
  Object *obj = calloc( 1, sizeof (Object) );
  obj->tag = _Symbol;
  obj->value.string = strdup(str);
  return obj;
}

Object *new_primitive(Object *(*fn) (Object *) ) {
  Object *obj = calloc(1, sizeof (Object) );
  obj->tag = _Primitive;
  obj->value.pfn = fn;
  return obj;
}

Object *new_closure(Object *params, Object *body, Object *env ) {
  Object *obj = calloc(1, sizeof (Object) );
  obj->tag = _Closure;
  obj->value.closure.params = params;
  obj->value.closure.body = body;
  obj->value.closure.env = env;
  return obj;
}

Object *get_symbol(char *str) {
  Object *list = symbols;
  for (; list; list = get_tail(list))
    if (strncmp(str, get_head(list)->value.string, SYMBOL_MAX)==0)
      return get_head(list);
  symbols = new_list(new_symbol(str), symbols);
  return get_head(symbols);
}

Object *read_list();

Object *read_obj() {
  if (token[0] == '(') return read_list();
  return get_symbol(token);
}

Object *read_list() {
  Object *tmp;
  read_token();
  if (token[0] == ')') return (Object *) NULL;
  tmp = read_obj();
  return new_list(tmp, read_list());
}

void print_obj(Object *ob, int head_of_list) {
  if (ob == 0) {
    printf("null");
  } else if (ob->tag == _Symbol) {
    printf("%s", ob->value.string);
  } else if (ob->tag == _List) {
    if (head_of_list) printf("(");
    print_obj(get_head(ob), 1);
    if (get_tail(ob) == 0) {
      printf(")");
    }
    else if(get_tail(ob)->tag == _List) {
      printf(" ");
      print_obj(get_tail(ob), 0);
    }
    else {
      printf(" . ");
      print_obj(get_tail(ob), 0);
      printf(")");
    }
  } else if (ob->tag == _Closure) {
    printf("<CLOSURE>");
  }
}

Object *fn_list(Object *a)      { return new_list(get_head(a), get_head(get_tail(a))); }
Object *fn_head(Object *a)      { return get_head(get_head(a)); }
Object *fn_tail(Object *a)      { return get_tail(get_head(a)); }
Object *fn_is_eq(Object *a)     { return get_head(a) == get_head(get_tail(a)) ? (Object *) exp_true : exp_false; }
Object *fn_is_list(Object *a)   { return get_head(a)->tag == _List ? exp_true : exp_false; }
Object *fn_is_symbol(Object *a) { return get_head(a)->tag == _Symbol ? exp_true : exp_false; }
Object *fn_is_null(Object *a)   { return get_head(a) == 0 ? exp_true : exp_false; }
Object *fn_read(Object *a)      { look = getchar(); read_token(); return read_obj(); }
Object *fn_write(Object *a)     { print_obj(get_head(a), 1); puts(""); return exp_true; }

Object *eval(Object *exp, Object *env);

Object *eval_list(Object *list, Object *env) {
  /* http://cslibrary.stanford.edu/105/LinkedListProblems.pdf */
  Object *head = 0, **args = &head;
  for (; list; list = get_tail(list)) {
    *args = new_list(eval(get_head(list), env) , 0);
    args = &((Object *) *args)->value.list.tail;
  }
  return head;
}

Object *eval_list_bind_append(Object *list, Object *names, Object *env, Object *tail) {
  Object *head = tail, **args = &head;
  for (; list; list = get_tail(list), names = get_tail(names)) {
    *args = new_list(new_list(get_head(names), new_list(eval(get_head(list), env), 0)), tail);
    args = &( (Object *) *args )->value.list.tail;
  }
  return head;
}

Object *eval_cond(Object *exp, Object *env) {
  if (exp == 0) {
    return 0;
  } else if (eval (get_head(get_head(exp)), env)) {
    return eval(get_head(get_tail(get_head(exp))), env);
  } else {
    return eval_cond(get_tail(exp), env);
  }
}

Object *apply_primitive(Object *func, Object *args) {
  return func->value.pfn(args);
}

Object *eval(Object *exp, Object *env) {
  if (exp->tag == _Symbol ) {
    for ( ; env != 0; env = get_tail(env) ) {
      if (exp == get_head(get_head(env))) return get_head(get_tail(get_head(env)));
    }
    puts("unbound variable");
    return 0;
  } else if (exp->tag == _List) {
    Object *func;
    if (get_head(exp)->tag == _Symbol) { /* special forms */
      if (get_head(exp) == sym_quote) {
        return get_head(get_tail(exp));
      } else if (get_head(exp) == sym_if) {
        return eval_cond(get_tail(exp), env);
      } else if (get_head(exp) == sym_lambda) {
        return new_closure(get_head(get_tail(exp)), get_head(get_tail(get_tail(exp))), env);
      } else if (get_head(exp) == sym_apply) { /* apply function to list */
        Object *args = eval_list (get_tail(get_tail(exp)), env);
        args = get_head(args); /* assumes one argument and that it is a list */
        return apply_primitive(eval(get_head(get_tail(exp)), env), args);
      }
    }
    /* function call */
    func = eval(get_head(exp), env);
    if (func) { 
      if (func->tag == _Closure) {
        env = eval_list_bind_append(get_tail(exp), func->value.closure.params, env, func->value.closure.env);
        return eval(func->value.closure.body, env );
      } else if (func->tag == _Primitive) { /* built-in primitive */
        return apply_primitive(func, eval_list(get_tail(exp), env));
      } else {
        puts("invalid function");
        return 0;
      }
    } else {
      puts("invalid application");
      return 0;
    }
  }
  puts("cannot evaluate expression");
  return 0;
}

int main(int argc, char *argv[]) {
  Object *env = new_list(new_list(get_symbol("head"), new_list(new_primitive(fn_head), 0)),
                new_list(new_list(get_symbol("tail"), new_list(new_primitive(fn_tail), 0)),
                new_list(new_list(get_symbol("list"), new_list(new_primitive(fn_list), 0)),
                new_list(new_list(get_symbol("eq?"), new_list(new_primitive(fn_is_eq), 0)),
                new_list(new_list(get_symbol("list?"), new_list(new_primitive(fn_is_list), 0)),
                new_list(new_list(get_symbol("symbol?"), new_list(new_primitive(fn_is_symbol), 0)),
                new_list(new_list(get_symbol("null?"), new_list(new_primitive(fn_is_null), 0)),
                new_list(new_list(get_symbol("read"), new_list(new_primitive(fn_read), 0)),
                new_list(new_list(get_symbol("write"), new_list(new_primitive(fn_write), 0)),
                new_list(new_list(get_symbol("null"), new_list(0,0)), 0))))))))));
  sym_quote = get_symbol("quote"); sym_if = get_symbol("if"); sym_lambda = get_symbol("lambda"); sym_apply = get_symbol("apply");
  exp_true = new_list(get_symbol("quote"), new_list(get_symbol("t"), 0));
  look = getchar();
  read_token();
  print_obj(eval(read_obj(), env), 1);
  printf("\n");
  return 0;
}
