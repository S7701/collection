/* micro lisp (C) A. Carl Douglas */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define debug(m, e) printf("%s:%d: %s: ", __FILE__, __LINE__, m); print_obj(e, 1); puts("");

typedef struct List {
  void *head;
  struct List *tail;
} List;

static List *symbols = 0, *sym_quote, *sym_if, *sym_lambda, *sym_macro, *sym_apply, *exp_true, *exp_false = 0;

#define SYMBOL_MAX 256

static int look; /* look ahead character */
static char token[SYMBOL_MAX]; /* token */

static int is_space(char x)  { return x == ' ' || x == '\t' || x == '\n'; }
static int is_parens(char x) { return x == '(' || x == ')'; }

static void read_token() {
  int len = 0;
  while(is_space(look)) { look = getchar(); }
  if (is_parens(look)) {
    token[len++] = look;  look = getchar();
  } else {
    while(len < SYMBOL_MAX - 1 && look != EOF && !is_space(look) && !is_parens(look)) {
      token[len++] = look;  look = getchar();
    }
  }
  token[len] = '\0';
}

#define is_list(x)  (((uintptr_t)x & 0x1) == 0x1)  /* tag pointer to list with 0x1 (alignment dependent)*/
#define is_atom(x)  (((uintptr_t)x & 0x1) == 0x0)
#define untag(x)    ((uintptr_t) x & ~0x1)
#define tag(x)      ((uintptr_t) x | 0x1)
#define get_head(x) (((List*)untag(x))->head)
#define get_tail(x) (((List*)untag(x))->tail)

static List *new_list(void *h, void *t) {
  List *p = calloc(1, sizeof (List));
  p->head = h;
  p->tail = t;
  return (List*) tag(p);
}

static void *get_sym(char *str) {
  List *p;
  for (p = symbols; p ; p = get_tail(p))
    if (strncmp(str, (char*) get_head(p), SYMBOL_MAX)==0) return get_head(p);
  symbols = new_list(strdup(str), symbols);
  return get_head(symbols);
}

static List *read_list();

static void *read_obj() {
  if (token[0] == '(') return read_list();
  return get_sym(token);
}

static List *read_list() {
  List *p;
  read_token();
  if (token[0] == ')') return 0;
  p = read_obj();
  return new_list(p, read_list());
}

static void print_obj(List *obj, int head_of_list) {
  if (!is_list(obj) ) {
    printf("%s", obj ? (char*) obj : "null" );
  } else {
    if (head_of_list) printf("(");
    print_obj(get_head(obj), 1);
    if (get_tail(obj) != 0) {
      printf(" ");
      print_obj(get_tail(obj), 0);
    } else  printf(")");
  }
}

static List *eval(List *exp, List *env);

static List *eval_list(List *list, List *env) {
  /* http://cslibrary.stanford.edu/105/LinkedListProblems.pdf */
  List *head = 0, **args = &head;
  for (; list ; list = get_tail(list)) {
    *args = new_list(eval(get_head(list), env) , 0);
    args = &((List*) untag(*args))->tail;
  }
  return head;
}

static List *apply_primitive(void *primfn, List *args) {
  return ((List*(*) (List*)) primfn) (args);
}

static List *eval(List *exp, List *env) {
  if (is_atom(exp) ) {
    for (; env != 0; env = get_tail(env))
      if (exp == get_head(get_head(env)))  return get_head(get_tail(get_head(env)));
    return 0;
  } else if (is_atom(get_head(exp))) { /* special forms */
    if (get_head(exp) == sym_quote) {
      return get_head(get_tail(exp));
    } else if (get_head(exp) == sym_if) {
      if (eval(get_head(get_tail(exp)), env) != 0)
        return eval(get_head(get_tail(get_tail(exp))), env);
      else
        return eval(get_head(get_tail(get_tail(get_tail(exp)))), env);
    } else if (get_head(exp) == sym_lambda || get_head(exp) == sym_macro) {
      return exp; /* TODO: create a closure and capture free vars */
    } else if (get_head(exp) == sym_apply) { /* apply function to list */
      List *args = eval_list(get_tail(get_tail(exp)), env);
      args = get_head(args); /* assumes one argument and that it is a list */
      return apply_primitive(eval(get_head(get_tail(exp)), env), args);
    } else { /* function call */
      List *primop = eval(get_head(exp), env);
      if (is_list(primop)) { /* user defined lambda or macro, arg list eval happens in binding  below */
        return eval(new_list(primop, get_tail(exp)), env );
      } else if (primop) { /* built-in primitive */
        return apply_primitive(primop, eval_list(get_tail(exp), env));
      }
    }
  } else if (get_head(get_head(exp)) == sym_lambda) { /* should be a lambda, bind names into env and eval body */
    List *extenv = env, *names = get_head(get_tail(get_head(exp))), *vars = get_tail(exp);
    for (; names; names = get_tail(names), vars = get_tail(vars))
      extenv = new_list(new_list(get_head(names), new_list(eval(get_head(vars), env), 0)), extenv);
    return eval(get_head(get_tail(get_tail(get_head(exp)))), extenv);
  } else if (get_head(get_head(exp)) == sym_macro) { /* should be a macro, bind names into env and eval body */
    List *extenv = env, *names = get_head(get_tail(get_head(exp))), *vars = get_tail(exp);
    for (; names; names = get_tail(names), vars = get_tail(vars))
      extenv = new_list(new_list(get_head(names), new_list(get_head(vars), 0)), extenv);
    return eval(get_head(get_tail(get_tail(get_head(exp)))), extenv);
  }
  puts("cannot evaluate expression: "); print_obj(exp, 1);
  return 0;
}

static List *fn_head(List *a)    { return get_head(get_head(a)); }
static List *fn_tail(List *a)    { return get_tail(get_head(a)); }
static List *fn_list(List *a)    { return new_list(get_head(a), get_head(get_tail(a))); }
static List *fn_is_eq(List *a)   { return get_head(a) == get_head(get_tail(a)) ? exp_true : exp_false; }
static List *fn_is_list(List *a) { return is_list(get_head(a)) ? exp_true : exp_false; }
static List *fn_is_atom(List *a) { return is_atom(get_head(a)) ? exp_true : exp_false; }
static List *fn_is_null(List *a) { return get_head(a) == 0     ? exp_true : exp_false; }
static List *fn_read(List *a)    { look = getchar(); read_token(); return read_obj(); }
static List *fn_write(List *a)   { print_obj(get_head(a), 1); puts(""); return exp_true; }

int main(int argc, char *argv[]) {
  List *env = new_list(new_list(get_sym("head"),  new_list((void *)fn_head, 0)),
              new_list(new_list(get_sym("tail"),  new_list((void *)fn_tail, 0)),
              new_list(new_list(get_sym("list"),  new_list((void *)fn_list, 0)),
              new_list(new_list(get_sym("eq?"),   new_list((void *)fn_is_eq, 0)),
              new_list(new_list(get_sym("list?"), new_list((void *)fn_is_list, 0)),
              new_list(new_list(get_sym("atom?"), new_list((void *)fn_is_atom, 0)),
              new_list(new_list(get_sym("null?"), new_list((void *)fn_is_null, 0)),
              new_list(new_list(get_sym("read"),  new_list((void *)fn_read, 0)),
              new_list(new_list(get_sym("write"), new_list((void *)fn_write, 0)),
              new_list(new_list(get_sym("null"),  new_list(0,0)), 0))))))))));
  sym_quote = get_sym("quote"); sym_if = get_sym("if"); sym_lambda = get_sym("lambda"); sym_macro = get_sym("macro"); sym_apply = get_sym("apply");
  exp_true = new_list(sym_quote, new_list(get_sym("t"), 0));
  look = getchar();
  read_token();
  print_obj(eval(read_obj(), env), 1);
  printf("\n");
  return 0;
}
