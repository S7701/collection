// c5.c - C in five (+X) functions

// char, int, structs and pointer types
// if, while, return and expression statements
// abstract syntax tree creation
// back-end code generator
// parameters passed in correct order
// various optimizations
// no enum identifiers allowd
// just enough features to allow self-compilation and a bit more

// Originally written by Robert Swierczek

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>

char *p, *lp, // current position in source code
     *data,   // data/bss pointer
     *ops;    // opcodes

int *e, *le,    // current position in emitted code
    *n,         // current node in abstract syntax tree
    tk,         // current token
    tk_val,     // current token value
    type,       // current expression type
    type_next,  // next available type
    *type_size, // array (indexed by type) of type sizes
    loc,        // local variable offset
    line,       // current line number
    src,        // print source and assembly flag
    debug;      // print executed instructions

// identifier
struct ident_s {
  int tk;
  int hash;
  char *str;
  int class;  // Num, Fun, Sys, Glo, Loc or Struct
  int type;   // CHAR, INT, STRUCT..., PTR..., PTR2...
  int val;
  struct ident_s *next;
} *id,  // currently parsed identifier
  *sym; // symbol table (linked list of identifiers)

// struct member
struct member_s {
  struct ident_s *id;
  int offset;
  int type;   // CHAR, INT, STRUCT..., PTR..., PTR2...
  struct member_s *next;
} **members; // array (indexed by type) of struct member lists

// tokens and classes (operators last and in precedence order)
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id, Load, Enter,
  Char, Else, Enum, If, Int, Return, Sizeof, Struct, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Dot, Arrow, Brak
};

// opcodes
enum { IMM, LEA, JMP, JSR, BZ, BNZ, ENTER, ADJ, LEAVE, LI, LC, SI, SC, PUSH,
       OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
       OPEN, READ, CLOSE, PRINTF, MALLOC, FREE, MEMSET, MEMCMP, EXIT };

// types
enum { CHAR, INT, STRUCT, PTR = 256, PTR2 = 512 };

void error(char *msg) { printf("%d: %s\n", line, msg); exit(-1); }

struct ident_s *sym_add(int tk, int hash, char *str) {
  struct ident_s *s = malloc(sizeof(struct ident_s));
  if (!s) error("sym_add: malloc failed");
  memset(s, 0, sizeof(struct ident_s));
  s->tk = tk;
  s->hash = hash;
  s->str = str;
  s->next = sym;
  return sym = s;
}

void sym_unwind(struct ident_s *s) { sym = s; } // TODO: free memory

void next() {
  while (tk = *p) {
    ++p;
    if (tk == '\n') {
      if (src) {
        printf("%d: %.*s", line, p - lp, lp);
        lp = p;
        while (le < e) {
          printf("  %s", &ops[*++le * 8]);
          if (*le <= ADJ) printf(" 0x%X\n", *++le); else printf("\n");
        }
      }
      ++line;
    }
    else if (tk == '#') {
      while (*p != 0 && *p != '\n') ++p;
    }
    else if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_') {
      char *str = p - 1;
      int hash = tk;
      while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        hash = hash * 147 + *p++;
      hash = (hash << 6) + (p - str);
      id = sym;
      while (id) {
        if (hash == id->hash && !memcmp(id->str, str, p - str)) { tk = id->tk; return; }
        id = id->next;
      }
      tk = Id;
      id = sym_add(tk, hash, str);
      return;
    }
    else if (tk >= '0' && tk <= '9') {
      if (tk_val = tk - '0') { while (*p >= '0' && *p <= '9') tk_val = tk_val * 10 + *p++ - '0'; }
      else if (*p == 'x' || *p == 'X') {
        while ((tk = *++p) && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F')))
          tk_val = tk_val * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
      }
      else { while (*p >= '0' && *p <= '7') tk_val = tk_val * 8 + *p++ - '0'; }
      tk = Num;
      return;
    }
    else if (tk == '/') {
      if (*p == '/') {
        ++p;
        while (*p != 0 && *p != '\n') ++p;
      }
      else {
        tk = Div;
        return;
      }
    }
    else if (tk == '\'' || tk == '"') {
      char *str = data;
      while (*p != 0 && *p != tk) {
        if ((tk_val = *p++) == '\\') {
          tk_val = *p++;
          if      (tk_val == '0') tk_val = '\0';
          else if (tk_val == 't') tk_val = '\t';
          else if (tk_val == 'v') tk_val = '\v';
          else if (tk_val == 'f') tk_val = '\f';
          else if (tk_val == 'r') tk_val = '\r';
          else if (tk_val == 'n') tk_val = '\n';
        }
        if (tk == '"') *data++ = tk_val;
      }
      ++p;
      if (tk == '"') tk_val = (int)str; else tk = Num;
      return;
    }
    else if (tk == '=') { if (*p == '=') { ++p; tk = Eq; } else tk = Assign; return; }
    else if (tk == '+') { if (*p == '+') { ++p; tk = Inc; } else tk = Add; return; }
    else if (tk == '-') { if (*p == '-') { ++p; tk = Dec; } else if (*p == '>') { ++p; tk = Arrow; } else tk = Sub; return; }
    else if (tk == '!') { if (*p == '=') { ++p; tk = Ne; } return; }
    else if (tk == '<') { if (*p == '=') { ++p; tk = Le; } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return; }
    else if (tk == '>') { if (*p == '=') { ++p; tk = Ge; } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return; }
    else if (tk == '|') { if (*p == '|') { ++p; tk = Lor; } else tk = Or; return; }
    else if (tk == '&') { if (*p == '&') { ++p; tk = Lan; } else tk = And; return; }
    else if (tk == '^') { tk = Xor; return; }
    else if (tk == '%') { tk = Mod; return; }
    else if (tk == '*') { tk = Mul; return; }
    else if (tk == '[') { tk = Brak; return; }
    else if (tk == '?') { tk = Cond; return; }
    else if (tk == '.') { tk = Dot; return; }
    else if (tk == '~' || tk == ';' || tk == '{' || tk == '}' || tk == '(' || tk == ')' || tk == ']' || tk == ',' || tk == ':') return;
  }
}

int match(int _tk) {
  if (_tk == tk) { next(); return 1; }
  return 0;
}

void expect(int _tk, char *msg) { if (!match(_tk)) error(msg); }

void expr(int lev) {
  int t, *b, sz;
  struct ident_s *d;
  struct member_s *m;

  if (!tk) { printf("%d: unexpected eof in expression\n", line); exit(-1); }
  else if (tk == Num) { *--n = tk_val; *--n = Num; next(); type = INT; }
  else if (tk == '"') {
    *--n = tk_val; *--n = Num; next();
    while (tk == '"') next();
    data = (char *)((int)data + sizeof(int) & -sizeof(int)); type = PTR;
  }
  else if (tk == Sizeof) {
    next(); if (tk == '(') next(); else { printf("%d: open paren expected in sizeof\n", line); exit(-1); }
    type = INT; if (tk == Int) next(); else if (tk == Char) { next(); type = CHAR; }
    else if (tk == Struct) { next(); if (tk != Id) { printf("%d: bad struct type\n", line); exit(-1); } type = id->stype; next(); }
    while (tk == Mul) { next(); type = type + PTR; }
    if (tk == ')') next(); else { printf("%d: close paren expected in sizeof\n", line); exit(-1); }
    *--n = type >= PTR ? sizeof(int) : type_size[type]; *--n = Num;
    type = INT;
  }
  else if (tk == Id) {
    d = id; next();
    if (tk == '(') {
      if (d->class != Sys && d->class != Fun) { printf("%d: bad function call\n", line); exit(-1); }
      next();
      t = 0; b = 0;
      while (tk != ')') { expr(Assign); *--n = (int)b; b = n; ++t; if (tk == ',') next(); }
      next();
      *--n = t; *--n = d->val; *--n = (int)b; *--n = d->class;
      type = d->type;
    }
    else if (d->class == Num) { *--n = d->val; *--n = Num; type = INT; }
    else {
      if (d->class == Loc) { *--n = d->val; *--n = Loc; }
      else if (d->class == Glo) { *--n = d->val; *--n = Num; }
      else { printf("%d: undefined variable\n", line); exit(-1); }
      *--n = type = d->type; *--n = Load;
    }
  }
  else if (tk == '(') {
    next();
    if (tk == Int || tk == Char || tk == Struct) {
      if (tk == Int) { next(); t = INT; } else if (tk == Char) { next(); t = CHAR; }
      else { next(); if (tk != Id) { printf("%d: bad struct type\n", line); exit(-1); } t = id->stype; next(); }
      while (tk == Mul) { next(); t = t + PTR; }
      if (tk == ')') next(); else { printf("%d: bad cast\n", line); exit(-1); }
      expr(Inc);
      type = t;
    }
    else {
      expr(Assign);
      if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    }
  }
  else if (tk == Mul) {
    next(); expr(Inc);
    if (type > INT) type = type - PTR; else { printf("%d: bad dereference\n", line); exit(-1); }
    *--n = type; *--n = Load;
  }
  else if (tk == And) {
    next(); expr(Inc);
    if (*n == Load) n = n+2; else { printf("%d: bad address-of\n", line); exit(-1); }
    type = type + PTR;
  }
  else if (tk == '!') {
    next(); expr(Inc);
    if (*n == Num) n[1] = !n[1]; else { *--n = 0; *--n = Num; --n; *n = (int)(n+3); *--n = Eq; }
    type = INT;
  }
  else if (tk == '~') {
    next(); expr(Inc);
    if (*n == Num) n[1] = ~n[1]; else { *--n = -1; *--n = Num; --n; *n = (int)(n+3); *--n = Xor; }
    type = INT;
  }
  else if (tk == Add) { next(); expr(Inc); type = INT; }
  else if (tk == Sub) {
    next(); expr(Inc);
    if (*n == Num) n[1] = -n[1]; else { *--n = -1; *--n = Num; --n; *n = (int)(n+3); *--n = Mul; }
    type = INT;
  }
  else if (tk == Inc || tk == Dec) {
    t = tk; next(); expr(Inc);
    if (*n == Load) *n = t; else { printf("%d: bad lvalue in pre-increment\n", line); exit(-1); }
  }
  else { printf("%d: bad expression\n", line); exit(-1); }

  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    t = type; b = n;
    if (tk == Assign) {
      next();
      if (*n != Load) { printf("%d: bad lvalue in assignment\n", line); exit(-1); }
      expr(Assign); *--n = (int)(b+2); *--n = type = t; *--n = Assign;
    }
    else if (tk == Cond) {
      next();
      expr(Assign);
      if (tk == ':') next(); else { printf("%d: conditional missing colon\n", line); exit(-1); }
      d = n;
      expr(Cond);
      --n; *n = (int)(n+1); *--n = (int)d; *--n = (int)b; *--n = Cond;
    }
    else if (tk == Lor) { next(); expr(Lan); if (*n==Num && *b==Num) n[1] = b[1] || n[1]; else { *--n = (int)b; *--n = Lor; } type = INT; }
    else if (tk == Lan) { next(); expr(Or);  if (*n==Num && *b==Num) n[1] = b[1] && n[1]; else { *--n = (int)b; *--n = Lan; } type = INT; }
    else if (tk == Or)  { next(); expr(Xor); if (*n==Num && *b==Num) n[1] = b[1] |  n[1]; else { *--n = (int)b; *--n = Or;  } type = INT; }
    else if (tk == Xor) { next(); expr(And); if (*n==Num && *b==Num) n[1] = b[1] ^  n[1]; else { *--n = (int)b; *--n = Xor; } type = INT; }
    else if (tk == And) { next(); expr(Eq);  if (*n==Num && *b==Num) n[1] = b[1] &  n[1]; else { *--n = (int)b; *--n = And; } type = INT; }
    else if (tk == Eq)  { next(); expr(Lt);  if (*n==Num && *b==Num) n[1] = b[1] == n[1]; else { *--n = (int)b; *--n = Eq;  } type = INT; }
    else if (tk == Ne)  { next(); expr(Lt);  if (*n==Num && *b==Num) n[1] = b[1] != n[1]; else { *--n = (int)b; *--n = Ne;  } type = INT; }
    else if (tk == Lt)  { next(); expr(Shl); if (*n==Num && *b==Num) n[1] = b[1] <  n[1]; else { *--n = (int)b; *--n = Lt;  } type = INT; }
    else if (tk == Gt)  { next(); expr(Shl); if (*n==Num && *b==Num) n[1] = b[1] >  n[1]; else { *--n = (int)b; *--n = Gt;  } type = INT; }
    else if (tk == Le)  { next(); expr(Shl); if (*n==Num && *b==Num) n[1] = b[1] <= n[1]; else { *--n = (int)b; *--n = Le;  } type = INT; }
    else if (tk == Ge)  { next(); expr(Shl); if (*n==Num && *b==Num) n[1] = b[1] >= n[1]; else { *--n = (int)b; *--n = Ge;  } type = INT; }
    else if (tk == Shl) { next(); expr(Add); if (*n==Num && *b==Num) n[1] = b[1] << n[1]; else { *--n = (int)b; *--n = Shl; } type = INT; }
    else if (tk == Shr) { next(); expr(Add); if (*n==Num && *b==Num) n[1] = b[1] >> n[1]; else { *--n = (int)b; *--n = Shr; } type = INT; }
    else if (tk == Add) {
      next(); expr(Mul);
      if (type >= PTR) { printf("%d: bad pointer addition\n", line); exit(-1); }
      if (t > PTR) {
        sz = t >= PTR2 ? sizeof(int) : type_size[t - PTR];
        if (*n == Num) n[1] = n[1] * sz; // lhs > PTR && rhs == Num
        else { *--n = sz; *--n = Num; --n; *n = (int)(n+3); *--n = Mul; } // lhs > PTR && rhs != Num
      }
      if (*n == Num && *b == Num) { n[1] = b[1] + n[1]; type = t; }
      else { *--n = (int)b; *--n = Add; type = (t == type) ? INT : t; }
    }
    else if (tk == Sub) {
      next(); expr(Mul);
      if (t < PTR && type >= PTR) { printf("%d: bad pointer subtraction\n", line); exit(-1); }
      if (t >= PTR && type >= PTR && t != type) { printf("%d: bad pointer types in subtraction\n", line); exit(-1); }
      if (t > PTR) {
        sz = t >= PTR2 ? sizeof(int) : type_size[t - PTR];
        if (*n == Num) n[1] = n[1] * sz;
        else if (t != type) { *--n = sz; *--n = Num; --n; *n = (int)(n+3); *--n = Mul; }
      }
      if (*n == Num && *b == Num) { n[1] = b[1] - n[1]; type = t; }
      else {
        *--n = (int)b; *--n = Sub;
        if (t > PTR && type > PTR) { *--n = sz; *--n = Num; --n; *n = (int)(n+3); *--n = Div; }
        type = (t == type) ? INT : t;
      }
    }
    else if (tk == Mul) { next(); expr(Inc); if (*n==Num && *b==Num) n[1] = b[1] * n[1]; else { *--n = (int)b; *--n = Mul; } type = INT; }
    else if (tk == Div) { next(); expr(Inc); if (*n==Num && *b==Num) n[1] = b[1] / n[1]; else { *--n = (int)b; *--n = Div; } type = INT; }
    else if (tk == Mod) { next(); expr(Inc); if (*n==Num && *b==Num) n[1] = b[1] % n[1]; else { *--n = (int)b; *--n = Mod; } type = INT; }
    else if (tk == Inc || tk == Dec) {
      if (*n == Load) *n = tk;
      else { printf("%d: bad lvalue in post-increment\n", line); exit(-1); }
      *--n = (type > PTR) ? sizeof(int) : sizeof(char); *--n = Num;
      *--n = (int)b; *--n = (tk == Inc) ? Sub : Add;
      next();
    }
    else if (tk == Dot || tk == Arrow) {
      if (tk == Dot) type = type + PTR;
      if (type <= PTR+INT || type >= PTR2) { printf("%d: structure expected\n", line); exit(-1); }
      next();
      if (tk != Id) { printf("%d: structure member expected\n", line); exit(-1); }
      m = members[type - PTR]; while (m && m->id != id) m = m->next;
      if (!m) { printf("%d: structure member not found\n", line); exit(-1); }
      if (m->offset) { *++e = PUSH; *++e = IMM; *++e = m->offset; *++e = ADD; }
      type = m->type;
      if (type <= INT || type >= PTR) *++e = (type == CHAR) ? LC : LI;
      next();
    }
    else if (tk == Brak) {
      next(); expr(Assign);
      if (tk == ']') next(); else { printf("%d: close bracket expected\n", line); exit(-1); }
      if (t > PTR) { if (*n == Num) n[1] = n[1] * sizeof(int); else { *--n = sizeof(int); *--n = Num; --n; *n = (int)(n+3); *--n = Mul; } }
      else if (t < PTR) { printf("%d: pointer type expected\n", line); exit(-1); }
      if (*n == Num && *b == Num) n[1] = b[1] + n[1]; else { *--n = (int)b; *--n = Add; }
      *--n = type = t - PTR; *--n = Load;
    }
    else { printf("%d: compiler error tk=%d\n", line, tk); exit(-1); }
  }
}

void stmt() {
  int *a, *b, *c;

  if (tk == Char || tk == Int || tk == Struct) {
    // TODO: add local variable definition
  }
  if (match(If)) {
    expect('(', "'(' expected in if");
    expr(Assign);
    a = n;
    expect(')', "')' expected in if");
    stmt();
    b = n;
    if (match(Else)) { stmt(); c = n; } else c = 0;
    *--n = (int)c; *--n = (int)b; *--n = (int)a; *--n = Cond;
  }
  else if (match(While)) {
    expect('(', "'(' expected in while");
    expr(Assign);
    a = n;
    expect(')', "')' expected in while");
    stmt();
    *--n = (int)a; *--n = While;
  }
  else if (match(Return)) {
    if (tk != ';') { expr(Assign); a = n; } else a = 0;
    expect(';', "';' expected in return");
    *--n = (int)a; *--n = Return;
  }
  else if (match('{')) {
    struct ident_s *_sym = sym;
    int _loc = loc;
    *--n = ';';
    while (!match('}')) { n1 = n; stmt(); *--n = (int)n1; *--n = '{'; }
    sym_unwind(_sym);
    loc = _loc;
  }
  else if (match(';')) {
    *--n = ';';
  }
  else {
    expr(Assign);
    expect(';', "semicolon expected");
  }
}

void gen(int *n) {
  int i, *a, *b;

  i = *n;
  if (i == Num) { *++e = IMM; *++e = n[1]; }
  else if (i == Loc) { *++e = LEA; *++e = n[1]; }
  else if (i == Load) { gen(n+2); *++e = (n[1] == CHAR) ? LC : LI; }
  else if (i == Assign) { gen((int *)n[2]); *++e = PUSH; gen(n+3); *++e = (n[1] == CHAR) ? SC : SI; }
  else if (i == Inc || i == Dec) {
    gen(n+2);
    *++e = PUSH; *++e = (n[1] == CHAR) ? LC : LI; *++e = PUSH;
    *++e = IMM; *++e = (n[1] > PTR) ? sizeof(int) : sizeof(char);
    *++e = (i == Inc) ? ADD : SUB;
    *++e = (n[1] == CHAR) ? SC : SI;
  }
  else if (i == Cond) {
    gen((int *)n[1]);
    *++e = BZ; b = ++e;
    gen((int *)n[2]);
    if (n[3]) { *b = (int)(e + 3); *++e = JMP; b = ++e; gen((int *)n[3]); }
    *b = (int)(e + 1);
  }
  else if (i == Lor) { gen((int *)n[1]); *++e = BNZ; b = ++e; gen(n+2); *b = (int)(e + 1); }
  else if (i == Lan) { gen((int *)n[1]); *++e = BZ;  b = ++e; gen(n+2); *b = (int)(e + 1); }
  else if (i == Or)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = OR; }
  else if (i == Xor) { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = XOR; }
  else if (i == And) { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = AND; }
  else if (i == Eq)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = EQ; }
  else if (i == Ne)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = NE; }
  else if (i == Lt)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = LT; }
  else if (i == Gt)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = GT; }
  else if (i == Le)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = LE; }
  else if (i == Ge)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = GE; }
  else if (i == Shl) { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = SHL; }
  else if (i == Shr) { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = SHR; }
  else if (i == Add) { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = ADD; }
  else if (i == Sub) { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = SUB; }
  else if (i == Mul) { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = MUL; }
  else if (i == Div) { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = DIV; }
  else if (i == Mod) { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = MOD; }
  else if (i == Sys || i == Fun) {
    b = (int *)n[1];
    while (b) { gen(b+1); *++e = PUSH; b = (int *)*b; }
    if (i == Fun) *++e = JSR; *++e = n[2];
    if (n[3]) { *++e = ADJ; *++e = n[3]; }
  }
  else if (i == While) {
    *++e = JMP; b = ++e; gen(n+2); *b = (int)(e + 1);
    gen((int *)n[1]);
    *++e = BNZ; *++e = (int)(b + 1);
  }
  else if (i == Return) { if (n[1]) gen((int *)n[1]); *++e = LEAVE; }
  else if (i == '{') { gen((int *)n[1]); gen(n+2); }
  else if (i == Enter) { *++e = ENTER; *++e = n[1]; gen(n+2); *++e = LEAVE; }
  else if (i != ';') { printf("%d: compiler error gen=%d\n", line, i); exit(-1); }
}

int main(int argc, char **argv) {
  int fd, poolsz, *ast;
  struct ident_s *id_main;
  struct member_s *m;
  int *pc, *sp, *bp, a, cycle; // vm registers
  int i, *t; // temps

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c5 [-s] [-d] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256*1024; // arbitrary size
  if (!(le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; }
  if (!(data = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; }
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%d) stack area\n", poolsz); return -1; }
  if (!(type_size = malloc(PTR * sizeof(int)))) { printf("could not malloc() type_size area\n"); return -1; }
  if (!(members = malloc(PTR * sizeof(struct member_s *)))) { printf("could not malloc() members area\n"); return -1; }
  if (!(ast = malloc(poolsz))) { printf("could not malloc(%d) abstract syntax tree area\n", poolsz); return -1; }
  ast = (int *)((int)ast + poolsz); // abstract syntax tree is most efficiently built as a stack

  memset(e, 0, poolsz);
  memset(data,0, poolsz);
  memset(type_size, 0, PTR * sizeof(int));
  memset(members, 0, PTR * sizeof(struct member_s *));

  ops = "IMM\0    LEA\0    JMP\0    JSR\0    BZ\0     BNZ\0    ENTER\0  ADJ\0    LEAVE\0  LI\0     LC\0     SI\0     SC\0     PUSH\0   "
        "OR\0     XOR\0    AND\0    EQ\0     NE\0     LT\0     GT\0     LE\0     GE\0     SHL\0    SHR\0    ADD\0    SUB\0    MUL\0    DIV\0    MOD\0    "
        "OPEN\0   READ\0   CLOSE\0  PRINTF\0 MALLOC\0 FREE\0   MEMSET\0 MEMCMP\0 EXIT\0   ";

  sym = 0;
  p = "char else enum if int return sizeof struct while "
      "open read close printf malloc free memset memcmp exit "
      "void main";
  i = Char; while (i <= While) { next(); id->tk = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { next(); id->class = Sys; id->type = INT; id->val = i++; } // add library to symbol table
  next(); id->tk = Char; // handle void type
  next(); id_main = id; // keep track of main

  if (!(lp = p = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }
  if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }
  p[i] = 0;
  close(fd);

  // add primitive types
  type_size[type_next++] = sizeof(char);
  type_size[type_next++] = sizeof(int);

  // parse global declarations
  line = 1;
  next();
  while (tk) {
    int bt = INT; // base type
    if (match(Char)) { bt = CHAR; }
    else if (match(Enum)) {
      expect('{', "bad enum definition: '{' expected; no enum identifier allowed");
      i = 0;
      while (!match('}')) {
        if (tk != Id) { printf("%d: bad enum identifier %d\n", line, tk); return -1; }
        if (id->class) { printf("%d: duplicate enum id '%.*s'; already decl'ed in line %d\n"line, tp - lp + 1, id->len, id->str, id->line); return -1; }
        struct ident_s _id = id; next();
        if (match(Assign)) {
          n = ast; expr(Cond);
          if (*n != Num) { printf("%d: bad enum initializer\n", line); return -1; }
          i = n[1];
        }
        _id->class = Num; _id->type = INT; _id->val = i++;
        if (!match(',') && tk != '}') error("',' or '}' expected in enum definition");
      }
    }
    else if (match(Struct)) {
      if (tk == Id) {
        if (id->class && id->class != Struct) error("bad struct identifier: not a struct identifier");
        if (!id->type) id->type = type_next++;
        bt = id->type;
        next();
      } else {
        bt = type_next++;
      }
      if (match('{')) {
        int size = 0;
        if (members[bt]) { printf("%d: duplicate struct definition\n", line); return -1; }
        while (!match('}')) {
          int mbt = INT; // member base type
          if (match(Char)) { mbt = CHAR; }
          else if (match(Struct)) {
            if (tk != Id) error("struct identifier expected in struct member definition");
            if (id->class != Struct) error("bad struct identifier in struct member definition: not a struct identifier");
            mbt = id->type;
            next();
          }
          else if (!match(Int)) { printf("%d: member base type expected\n", line); exit(-1); }

          while (!match(';')) {
            type = mbt;

            while (match(Mul)) { type = type + PTR; }

            if (tk != Id) { printf("%d: member identifier expected in struct member definition\n", line); return -1; }
            // TODO: check if already defined
            m = malloc(sizeof(struct member_s));
            m->id = id;
            m->offset = size;
            m->type = type;
            m->next = members[bt];
            members[bt] = m;
            size = size + (type >= PTR ? sizeof(int) : type_size[type]);
            size = (size + 3) & -4;
            next();
            if (!match(',') && tk != '}') error("',' or '}' expected in struct definition");
          }
        }
        type_size[bt] = size;
      }
    }
    else if (!match(Int)) { printf("%d: base type expected\n", line); exit(-1); }

    while (!match(';')) {
      type = bt;

      while (match(Mul)) { type = type + PTR; }

      if (tk != Id) { printf("%d: bad global declaration\n", line); return -1; }
      if (id->class) { printf("%d: duplicate global definition\n", line); return -1; }
      id->type = type;
      next();
      if (tk == '(') { // function
        struct ident_s *_sym = sym; // save symbol table pointer; unwind it later
        id->class = Fun;
        id->val = (int)(e + 1);
        i = 0;
        next();
        while (!match(')')) { // parameter list
          type = INT;
          if (match(Char)) { type = CHAR; }
          else if (match(Struct)) {
            if (tk != Id) { printf("%d: struct identifier expected in parameter declaration\n", line); return -1; }
            if (id->class != Struct) error("bad struct identifier in parameter declaration: not a struct identifier");
            type = id->type;
            next();
          }
          else if (!match(Int)) { printf("%d: base type expected\n", line); exit(-1); }

          while (match(Mul)) type = type + PTR;

          if (tk != Id) { printf("%d: bad parameter declaration\n", line); return -1; }
          if (id->class == Loc) { printf("%d: duplicate parameter definition\n", line); return -1; }
          if (id->class) id = sym_add(Id, id->hash, id->str);
          id->class = Loc;
          id->type = type;
          id->val = i++;
          next();
          if (!match(',') && tk != ')') { printf("%d: ',' or ')' expected in parameter declaration\n", line); exit(-1); }
        }
        if (!match('{')) { printf("%d: bad function definition\n", line); return -1; }
        n = ast;
        *--n = ';'; while (tk != '}') { t = n; stmt(); *--n = (int)t; *--n = '{'; }
        *--n = -i; *--n = Enter;
        gen(n);
        sym_unwind(_sym); // unwind symbol table locals
        tk = ';'; // break inner while
      }
      else {
        id->class = Glo;
        id->val = (int)data;
        data = data + sizeof(int);
      }
      if (tk == ',') next();
    }
  }

  if (!(pc = (int *)id_main->val)) { printf("main() not defined\n"); return -1; }
  if (src) return 0;

  // setup stack
  bp = sp = (int *)((int)sp + poolsz);
  *--sp = EXIT; // call exit if main returns
  *--sp = PUSH; t = sp;
  *--sp = argc;
  *--sp = (int)argv;
  *--sp = (int)t;

  // run...
  cycle = 0;
  while (1) {
    i = *pc++; ++cycle;
    if (debug) {
      printf("%d> %s", cycle, &ops[i * 8]);
      if (i <= ADJ) printf(" 0x%X\n", *pc); else printf("\n");
    }
    if      (i == IMM)   a = *pc++;                                         // load global address or immediate
    else if (i == LEA)   a = (int)(bp + *pc++);                             // load local address
    else if (i == JMP)   pc = (int *)*pc;                                   // jump
    else if (i == JSR)   { *--sp = (int)(pc + 1); pc = (int *)*pc; }        // jump to subroutine
    else if (i == BZ)    pc = a ? pc + 1 : (int *)*pc;                      // branch if zero
    else if (i == BNZ)   pc = a ? (int *)*pc : pc + 1;                      // branch if not zero
    else if (i == ENTER) { *--sp = (int)bp; bp = sp; sp = sp - *pc++; }     // enter subroutine
    else if (i == ADJ)   sp = sp + *pc++;                                   // stack adjust
    else if (i == LEAVE) { sp = bp; bp = (int *)*sp++; pc = (int *)*sp++; } // leave subroutine
    else if (i == LI)    a = *(int *)a;                                     // load int
    else if (i == LC)    a = *(char *)a;                                    // load char
    else if (i == SI)    *(int *)*sp++ = a;                                 // store int
    else if (i == SC)    a = *(char *)*sp++ = a;                            // store char
    else if (i == PUSH)   *--sp = a;                                         // push

    else if (i == OR)  a = *sp++ |  a;
    else if (i == XOR) a = *sp++ ^  a;
    else if (i == AND) a = *sp++ &  a;
    else if (i == EQ)  a = *sp++ == a;
    else if (i == NE)  a = *sp++ != a;
    else if (i == LT)  a = *sp++ <  a;
    else if (i == GT)  a = *sp++ >  a;
    else if (i == LE)  a = *sp++ <= a;
    else if (i == GE)  a = *sp++ >= a;
    else if (i == SHL) a = *sp++ << a;
    else if (i == SHR) a = *sp++ >> a;
    else if (i == ADD) a = *sp++ +  a;
    else if (i == SUB) a = *sp++ -  a;
    else if (i == MUL) a = *sp++ *  a;
    else if (i == DIV) a = *sp++ /  a;
    else if (i == MOD) a = *sp++ %  a;

    else if (i == OPEN) a = open((char *)sp[1], *sp);
    else if (i == READ) a = read(sp[2], (char *)sp[1], *sp);
    else if (i == CLOSE) a = close(*sp);
    else if (i == PRINTF) { t = sp + pc[1]; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]); }
    else if (i == MALLOC) a = (int)malloc(*sp);
    else if (i == FREE) free((void *)*sp);
    else if (i == MEMSET) a = (int)memset((char *)sp[2], sp[1], *sp);
    else if (i == MEMCMP) a = memcmp((char *)sp[2], (char *)sp[1], *sp);
    else if (i == EXIT) { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }
    else { printf("unknown instruction = %d! cycle = %d\n", i, cycle); return -1; }
  }
}
