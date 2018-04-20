/* micro lisp (C) A. Carl Douglas */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define debug(m,e) printf("%s:%d: %s:",__FILE__,__LINE__,m); print_obj(e,1); puts("");
typedef struct Object {
  enum { _Symbol, _Pair, _Primitive, _Closure } tag;
  union {
    char * string;
    struct Object * (*pfn) (struct Object *);
    struct List {
      struct Object * next;
      struct Object * data;
    } pair;
    struct Closure {
      struct Object * params;
      struct Object * body;
      struct Object * env;
    } closure;
  } value;
} Object;
Object * symbols = 0;
static int look; /* look ahead character */
#define SYMBOL_MAX  32
static char token[SYMBOL_MAX]; /* token */
int is_space(char x)  { return x == ' ' || x == '\n'; }
int is_parens(char x) { return x == '(' || x == ')'; }
Object *car(Object *x) { return x->value.pair.data; }
Object *cdr(Object *x) { return x->value.pair.next; }
static void gettoken() {
  int index = 0;
  while(is_space(look)) { look = getchar(); }
  if (is_parens(look)) {
    token[index++] = look;  look = getchar();
  } else {
    while(index < SYMBOL_MAX - 1 && look != EOF && !is_space(look) && !is_parens(look)) {
      token[index++] = look;  look = getchar();
    }
  }
  token[index] = '\0';
}

#define e_true     cons(intern("quote"), cons(intern("t"), 0))
#define e_false    0

Object * cons(Object * _car, Object * _cdr) {
  Object *_pair = calloc( 1, sizeof (Object) );
  _pair->tag = _Pair;
  _pair->value.pair.data = _car;
  _pair->value.pair.next = _cdr;
  return _pair;
}

Object * newsymbol(char *str) {
  Object *obj = calloc( 1, sizeof (Object) );
  obj->tag = _Symbol;
  obj->value.string = strdup(str);
  return obj;
}

Object * newprimop( Object * (*fn) (Object *) ) {
  Object * obj = calloc(1, sizeof (Object) );
  obj->tag = _Primitive;
  obj->value.pfn = fn;
  return obj;
}

Object * newclosure( Object *params, Object *body, Object *env ) {
  Object * obj = calloc(1, sizeof (Object) );
  obj->tag = _Closure;
  obj->value.closure.params = params;
  obj->value.closure.body = body;
  obj->value.closure.env = env;
  return obj;
}

Object * intern(char *sym) {
  Object *_pair = symbols;
  for ( ; _pair ; _pair = cdr(_pair))
    if (strncmp(sym, car(_pair)->value.string, SYMBOL_MAX)==0)
      return car(_pair);
  symbols = cons(newsymbol(sym), symbols);
  return car(symbols);
}

Object * getlist();

Object * getobj() {
  if (token[0] == '(') return getlist();
  return intern(token);
}

Object * getlist() {
  Object *tmp;
  gettoken();
  if (token[0] == ')') return (Object *) NULL;
  tmp = getobj();
  return cons(tmp, getlist());
}

void print_obj(Object *ob, int head_of_list) {
  if (ob == 0) {
    printf("null");
  } else if (ob->tag == _Symbol) {
    printf("%s", ob->value.string);
  } else if (ob->tag == _Pair) {
    if (head_of_list) printf("(");
    print_obj(car(ob), 1);
    if (cdr(ob) == 0) {
      printf(")");
    }
    else if(cdr(ob)->tag == _Pair) {
      printf(" ");
      print_obj(cdr(ob), 0);
    }
    else {
      printf(" . ");
      print_obj(cdr(ob), 0);
      printf(")");
    }
  } else if (ob->tag == _Closure) {
    printf("<CLOSURE>");
  }
}

Object *fcons(Object *a)    {  return cons(car(a), car(cdr(a)));  }
Object *fcar(Object *a)     {  return car(car(a));  }
Object *fcdr(Object *a)     {  return cdr(car(a));  }
Object *feq(Object *a)      {  return car(a) == car(cdr(a)) ? (Object *) e_true : e_false;  }
Object *fpair(Object *a)    {  return car(a)->tag == _Pair  ? e_true : e_false;  }
Object *fatom(Object *a)    {  return car(a)->tag == _Symbol  ? e_true : e_false;  }
Object *fnull(Object *a)    {  return car(a) == 0           ? e_true : e_false; }
Object *freadobj(Object *a) {  look = getchar(); gettoken(); return getobj();  }
Object *fwriteobj(Object *a){  print_obj(car(a), 1); puts(""); return e_true;  }

Object * eval(Object *exp, Object *env);

Object * evlist(Object *list, Object *env) {
  /* http://cslibrary.stanford.edu/105/LinkedListProblems.pdf */
  Object *head = 0, **args = &head;
  for ( ; list ; list = cdr(list) ) {
    *args = cons( eval(car(list), env) , 0);
    args = &( (Object *) *args )->value.pair.next;
  }
  return head;
}

Object * evlist_bind_append(Object *list, Object *names, Object *env, Object *tail) {
  Object *head = tail, **args = &head;
  for ( ; list ; list = cdr(list), names = cdr(names) ) {
    *args = cons( cons(car(names), cons(eval(car(list), env), 0))
		, tail);
    args = &( (Object *) *args )->value.pair.next;
  }
  return head;
}

Object * evcond(Object *exp, Object *env) {
  if (exp == 0) {
    return 0;
  } else if (eval (car(car(exp)), env)) {
    return eval(car(cdr(car(exp))), env);
  } else {
    return evcond(cdr(exp), env);
  }
}

Object * apply_primitive(Object *primptr, Object *args) {
  return primptr->value.pfn (args);
}

Object * eval(Object *exp, Object *env) {
  if (exp->tag == _Symbol ) {
    for ( ; env != 0; env = cdr(env) ) {
      if (exp == car(car(env))) return car(cdr(car(env)));
    }
    puts("unbound variable");
    return 0;
  } else if (exp->tag == _Pair) {
    Object *func = 0;
    if ( car (exp) ->tag == _Symbol) { /* special forms */
      if (car(exp) == intern("quote")) {
        return car(cdr(exp));
      } else if (car(exp) == intern("cond")) {
        return evcond(cdr(exp), env);
      } else if (car(exp) == intern("lambda") || car(exp) == intern("λ")) {
        return newclosure(car(cdr(exp)), car(cdr(cdr(exp))), env);
      } else if (car(exp) == intern("apply")) { /* apply function to list */
        Object *args = evlist (cdr(cdr(exp)), env);
        args = car(args); /* assumes one argument and that it is a list */
        return apply_primitive( eval(car(cdr(exp)), env), args);
      }
    }
    /* function call */
    func = eval (car(exp), env);
    if (func && func->tag == _Closure) {
      env = evlist_bind_append(cdr(exp), func->value.closure.params, env, func->value.closure.env);
      return eval( func->value.closure.body, env );
    } else if (func) { /* built-in primitive */
      return apply_primitive(func, evlist(cdr(exp), env));
    } else {
      puts("invalid application");
      return 0;
    }
  }
  puts("cannot evaluate expression");
  return 0;
}

int main(int argc, char *argv[]) {
  Object *env = cons (cons(intern("car"), cons(newprimop(fcar), 0)),
              cons (cons(intern("cdr"), cons(newprimop(fcdr), 0)),
              cons (cons(intern("cons"), cons(newprimop(fcons), 0)),
              cons (cons(intern("eq?"), cons(newprimop(feq), 0)),
              cons (cons(intern("pair?"), cons(newprimop(fpair), 0)),
              cons (cons(intern("symbol?"), cons(newprimop(fatom), 0)),
              cons (cons(intern("null?"), cons(newprimop(fnull), 0)),
              cons (cons(intern("read"), cons(newprimop(freadobj), 0)),
              cons (cons(intern("write"), cons(newprimop(fwriteobj), 0)),
              cons (cons(intern("null"), cons(0,0)), 0))))))))));
  look = getchar();
  gettoken();
  print_obj( eval(getobj(), env), 1 );
  printf("\n");
  return 0;
}
