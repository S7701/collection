/* micro lisp (C) A. Carl Douglas */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define debug(m, e) printf("%s:%d: %s: ", __FILE__, __LINE__, m); print_obj(e, 1); puts("");

typedef struct Pair {
  void *left;
  struct Pair *right;
} Pair;

static Pair *symbols = 0, *s_quote, *s_if, *s_lambda, *s_macro, *s_apply, *e_true, *e_false = 0;
static int look; /* look ahead character */

#define SYMBOL_MAX 256

static char token[SYMBOL_MAX]; /* token */
static int is_space(char x)  { return x == ' ' || x == '\t' || x == '\n'; }
static int is_parens(char x) { return x == '(' || x == ')'; }

static void get_token() {
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

#define is_pair(x) (((uintptr_t)x & 0x1) == 0x1)  /* tag pointer to pair with 0x1 (alignment dependent)*/
#define is_atom(x) (((uintptr_t)x & 0x1) == 0x0)
#define untag(x)   ((uintptr_t) x & ~0x1)
#define tag(x)     ((uintptr_t) x | 0x1)
#define left(x)    (((Pair*)untag(x))->left)
#define right(x)   (((Pair*)untag(x))->right)

static Pair *pair(void *_left, void *_right) {
  Pair *_pair = calloc(1, sizeof (Pair));
  _pair->left = _left;
  _pair->right = _right;
  return (Pair*) tag(_pair);
}

static void *intern(char *sym) {
  Pair *_pair;
  for (_pair = symbols; _pair ; _pair = right(_pair))
    if (strncmp(sym, (char*) left(_pair), SYMBOL_MAX)==0) return left(_pair);
  symbols = pair(strdup(sym), symbols);
  return left(symbols);
}

static Pair *get_list();

static void * get_obj() {
  if (token[0] == '(') return get_list();
  return intern(token);
}

static Pair *get_list() {
  Pair *tmp;
  get_token();
  if (token[0] == ')') return 0;
  tmp = get_obj();
  return pair(tmp, get_list());
}

static void print_obj(Pair *obj, int head_of_list) {
  if (!is_pair(obj) ) {
    printf("%s", obj ? (char*) obj : "null" );
  } else {
    if (head_of_list) printf("(");
    print_obj(left(obj), 1);
    if (right(obj) != 0) {
      printf(" ");
      print_obj(right(obj), 0);
    } else  printf(")");
  }
}

static Pair *eval(Pair *exp, Pair *env);

static Pair *eval_list(Pair *list, Pair *env) {
  /* http://cslibrary.stanford.edu/105/LinkedListProblems.pdf */
  Pair *head = 0, **args = &head;
  for (; list ; list = right(list)) {
    *args = pair(eval(left(list), env) , 0);
    args = &((Pair*) untag(*args))->right;
  }
  return head;
}

static Pair *apply_primitive(void *primfn, Pair *args) {
  return ((Pair*(*) (Pair*)) primfn) (args);
}

static Pair *eval(Pair *exp, Pair *env) {
  if (is_atom(exp) ) {
    for (; env != 0; env = right(env))
      if (exp == left(left(env)))  return left(right(left(env)));
    return 0;
  } else if (is_atom(left(exp))) { /* special forms */
    if (left(exp) == s_quote) {
      return left(right(exp));
    } else if (left(exp) == s_if) {
      if (eval(left(right(exp)), env) != 0)
        return eval(left(right(right(exp))), env);
      else
        return eval(left(right(right(right(exp)))), env);
    } else if (left(exp) == s_lambda || left(exp) == s_macro) {
      return exp; /* TODO: create a closure and capture free vars */
    } else if (left(exp) == s_apply) { /* apply function to list */
      Pair *args = eval_list(right(right(exp)), env);
      args = left(args); /* assumes one argument and that it is a list */
      return apply_primitive(eval(left(right(exp)), env), args);
    } else { /* function call */
      Pair *primop = eval(left(exp), env);
      if (is_pair(primop)) { /* user defined lambda or macro, arg list eval happens in binding  below */
        return eval(pair(primop, right(exp)), env );
      } else if (primop) { /* built-in primitive */
        return apply_primitive(primop, eval_list(right(exp), env));
      }
    }
  } else if (left(left(exp)) == s_lambda) { /* should be a lambda, bind names into env and eval body */
    Pair *extenv = env, *names = left(right(left(exp))), *vars = right(exp);
    for (; names; names = right(names), vars = right(vars))
      extenv = pair(pair(left(names), pair(eval(left(vars), env), 0)), extenv);
    return eval(left(right(right(left(exp)))), extenv);
  } else if (left(left(exp)) == s_macro) { /* should be a macro, bind names into env and eval body */
    Pair *extenv = env, *names = left(right(left(exp))), *vars = right(exp);
    for (; names; names = right(names), vars = right(vars))
      extenv = pair(pair(left(names), pair(left(vars), 0)), extenv);
    return eval(left(right(right(left(exp)))), extenv);
  }
  puts("cannot evaluate expression: "); print_obj(exp, 1);
  return 0;
}

static Pair *f_left(Pair *a)     { return left(left(a)); }
static Pair *f_right(Pair *a)    { return right(left(a)); }
static Pair *f_pair(Pair *a)     { return pair(left(a), left(right(a))); }
static Pair *f_eq(Pair *a)       { return left(a) == left(right(a)) ? e_true : e_false; }
static Pair *f_pair(Pair *a)     { return is_pair(left(a)) ? e_true : e_false; }
static Pair *f_atom(Pair *a)     { return is_atom(left(a)) ? e_true : e_false; }
static Pair *f_null(Pair *a)     { return left(a) == 0     ? e_true : e_false; }
static Pair *f_readobj(Pair *a)  { look = getchar(); get_token(); return get_obj(); }
static Pair *f_writeobj(Pair *a) { print_obj(left(a), 1); puts(""); return e_true; }

int main(int argc, char *argv[]) {
  Pair *env = pair(pair(intern("left"), pair((void *)f_left, 0)),
              pair(pair(intern("right"), pair((void *)f_right, 0)),
              pair(pair(intern("pair"), pair((void *)f_pair, 0)),
              pair(pair(intern("eq?"), pair((void *)f_eq, 0)),
              pair(pair(intern("pair?"), pair((void *)f_pair, 0)),
              pair(pair(intern("symbol?"), pair((void *)f_atom, 0)),
              pair(pair(intern("null?"), pair((void *)f_null, 0)),
              pair(pair(intern("read"), pair((void *)f_readobj, 0)),
              pair(pair(intern("write"), pair((void *)f_writeobj, 0)),
              pair(pair(intern("null"), pair(0,0)), 0))))))))));
  s_quote = intern("quote"); s_if = intern("if"); s_lambda = intern("lambda"); s_macro = intern("macro"); s_apply = intern("apply");
  e_true = pair(s_quote, pair( intern("t"), 0));
  look = getchar();
  get_token();
  print_obj(eval(get_obj(), env), 1);
  printf("\n");
  return 0;
}
