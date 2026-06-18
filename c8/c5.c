// c5.c - C in five (+X) functions

// char, int, structs and pointer types
// if, while, return and expression statements
// abstract syntax tree creation
// back-end code generator
// parameters passed in correct order
// various optimizations
// no enum identifiers allowd
// no assignments in expressions allowed
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
    dbg;        // print executed instructions

// identifier
struct ident_s {
  int tk;
  int hash;
  char *str;
  int class;  // Num, Fun, Sys, Global, Local or Struct
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
  Num = 128, Fun, Sys, Global, Local, Id, Load, Enter,
  Char, Else, Enum, If, Int, Return, Sizeof, Struct, While,
  Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Dot, Arrow, Bracket
};

// opcodes
enum {
  IMM, LEA, JMP, JSR, BZ, BNZ, ENTER, ADJ, LEAVE, LI, LC, SI, SC, PUSH,
  OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
  OPEN, READ, WRITE, CLOSE, PRINTF, SCANF, MALLOC, FREE, MEMSET, MEMCMP, MEMCPY, EXIT
};

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
  while (*p) {
    tk = *p++;
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
      tk_val = tk - '0';
      if (tk_val) { while (*p >= '0' && *p <= '9') tk_val = tk_val * 10 + *p++ - '0'; }
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
    else if (tk == '&') { if (*p == '&') { ++p; tk = Land; } else tk = And; return; }
    else if (tk == '^') { tk = Xor; return; }
    else if (tk == '%') { tk = Mod; return; }
    else if (tk == '*') { tk = Mul; return; }
    else if (tk == '[') { tk = Bracket; return; }
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
  if (!tk) error("unexpected eof in expr");
  else if (match(Num)) {
    *--n = tk_val; *--n = Num;
    next();
    type = INT;
  }
  else if (tk == '"') {
    *--n = tk_val; *--n = Num;
    next();
    while (match('"')) /* nop */;
    data = (char *)((int)data + sizeof(int) & -sizeof(int));
    type = PTR;
  }
  else if (match(Sizeof)) {
    expect('(', "'(' expected in sizeof expr");
    if (match(Int)) type = INT;
    else if (match(Char)) type = CHAR;
    else if (match(Struct)) {
      if (tk != Id || id->class != Struct) error("bad struct type in sizeof expr");
      type = id->type;
      next();
    }
    else error("type expected in sizeof expr");
    while (match(Mul)) type = type + PTR;
    expect(')', "')' expected in sizeof expr");
    *--n = type >= PTR ? sizeof(int) : type_size[type]; *--n = Num;
    type = INT;
  }
  else if (tk == Id) {
    struct ident_s *_id = id;
    next();
    if (match('(')) { // function call
      int i = 0, *_n = 0;
      if (_id->class != Sys && _id->class != Fun) error("bad function call: not a function identifier");
      while (!match(')')) {
        expr(Cond); *--n = (int)_n; _n = n; ++i;
        if (!match(',') && tk != ')') error("',' or ')' expected in function call");
      }
      *--n = i; *--n = _id->val; *--n = (int)_n; *--n = _id->class;
      type = _id->type;
    }
    else if (_id->class == Num) {
      *--n = _id->val; *--n = Num;
      type = INT;
    }
    else {
      if (_id->class == Local) { *--n = _id->val; *--n = Local; }
      else if (_id->class == Global) { *--n = _id->val; *--n = Num; }
      else { printf("%d: undefined variable\n", line); exit(-1); }
      *--n = type = _id->type; *--n = Load;
    }
  }
  else if (match('(')) {
    if (tk == Int || tk == Char || tk == Struct) { // type cast
      int _type;
      if (match(Int)) { _type = INT; }
      else if (match(Char)) { _type = CHAR; }
      else {
        next();
        if (tk != Id || id->class != Struct) error("bad struct type in type cast");
        _type = id->type;
        next();
      }
      while (match(Mul)) _type = _type + PTR;
      expect(')', "')' expected in type cast expr");
      expr(Inc);
      type = _type;
    }
    else { // group
      expr(Cond);
      expect(')', "')' expected in group expr");
    }
  }
  else if (match(Mul)) {
    expr(Inc);
    if (type > INT) type = type - PTR;
    else error("bad dereference expr");
    *--n = type; *--n = Load;
  }
  else if (match(And)) {
    expr(Inc);
    if (*n == Load) n = n+2;
    else error("bad address-of expr");
    type = type + PTR;
  }
  else if (match('!')) {
    expr(Inc);
    if (*n == Num) n[1] = !n[1];
    else { *--n = 0; *--n = Num; --n; *n = (int)(n+3); *--n = Eq; }
    type = INT;
  }
  else if (match('~')) {
    expr(Inc);
    if (*n == Num) n[1] = ~n[1];
    else { *--n = -1; *--n = Num; --n; *n = (int)(n+3); *--n = Xor; }
    type = INT;
  }
  else if (match(Add)) { expr(Inc); type = INT; }
  else if (match(Sub)) {
    expr(Inc);
    if (*n == Num) n[1] = -n[1];
    else { *--n = -1; *--n = Num; --n; *n = (int)(n+3); *--n = Mul; }
    type = INT;
  }
  else if (tk == Inc || tk == Dec) {
    int _tk = tk;
    next();
    expr(Inc);
    if (*n == Load) *n = _tk;
    else error("bad lvalue in pre-increment expr");
  }
  else error("bad expression");

  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    int type_lhs = type;
    int *n_lhs = n;
    if (match(Assign)) {
      if (*n != Load) error("bad lvalue in assignment");
      expr(Assign);
      *--n = (int)(n_lhs+2); *--n = type = type_lhs; *--n = Assign;
    }
    else if (match(Cond)) {
      int *n_rhs;
      expr(Cond);
      expect(':', "conditional missing colon");
      n_rhs = n;
      expr(Cond);
      --n; *n = (int)(n+1); *--n = (int)n_rhs; *--n = (int)n_lhs; *--n = Cond;
    }
    else if (match(Lor))  { expr(Land); if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] || n[1]; else { *--n = (int)n_lhs; *--n = Lor;  } type = INT; }
    else if (match(Land)) { expr(Or);   if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] && n[1]; else { *--n = (int)n_lhs; *--n = Land; } type = INT; }
    else if (match(Or))   { expr(Xor);  if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] |  n[1]; else { *--n = (int)n_lhs; *--n = Or;   } type = INT; }
    else if (match(Xor))  { expr(And);  if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] ^  n[1]; else { *--n = (int)n_lhs; *--n = Xor;  } type = INT; }
    else if (match(And))  { expr(Eq);   if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] &  n[1]; else { *--n = (int)n_lhs; *--n = And;  } type = INT; }
    else if (match(Eq))   { expr(Lt);   if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] == n[1]; else { *--n = (int)n_lhs; *--n = Eq;   } type = INT; }
    else if (match(Ne))   { expr(Lt);   if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] != n[1]; else { *--n = (int)n_lhs; *--n = Ne;   } type = INT; }
    else if (match(Lt))   { expr(Shl);  if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] <  n[1]; else { *--n = (int)n_lhs; *--n = Lt;   } type = INT; }
    else if (match(Gt))   { expr(Shl);  if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] >  n[1]; else { *--n = (int)n_lhs; *--n = Gt;   } type = INT; }
    else if (match(Le))   { expr(Shl);  if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] <= n[1]; else { *--n = (int)n_lhs; *--n = Le;   } type = INT; }
    else if (match(Ge))   { expr(Shl);  if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] >= n[1]; else { *--n = (int)n_lhs; *--n = Ge;   } type = INT; }
    else if (match(Shl))  { expr(Add);  if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] << n[1]; else { *--n = (int)n_lhs; *--n = Shl;  } type = INT; }
    else if (match(Shr))  { expr(Add);  if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] >> n[1]; else { *--n = (int)n_lhs; *--n = Shr;  } type = INT; }
    else if (match(Add)) {
      expr(Mul);
      if (type >= PTR) error("bad pointer addition: no pointer on rhs allowed");
      if (type_lhs > PTR) {
        int sz = type_lhs >= PTR2 ? sizeof(int) : type_size[type_lhs - PTR];
        if (*n == Num) n[1] = n[1] * sz; // lhs > PTR && rhs == Num
        else { *--n = sz; *--n = Num; --n; *n = (int)(n+3); *--n = Mul; } // lhs > PTR && rhs != Num
      }
      if (*n == Num && *n_lhs == Num) {
        n[1] = n_lhs[1] + n[1];
        type = type_lhs;
      }
      else {
        *--n = (int)n_lhs; *--n = Add;
        type = (type_lhs == type) ? INT : type_lhs;
      }
    }
    else if (match(Sub)) {
      expr(Mul);
      if (type_lhs < PTR && type >= PTR) error("bad pointer subtraction");
      if (type_lhs >= PTR && type >= PTR && type_lhs != type) error("bad pointer types in subtraction");
      if (type_lhs > PTR) {
        int sz = type_lhs >= PTR2 ? sizeof(int) : type_size[type_lhs - PTR];
        if (*n == Num) n[1] = n[1] * sz; // lhs > PTR && rhs == Num
        else if (type_lhs != type) { *--n = sz; *--n = Num; --n; *n = (int)(n+3); *--n = Mul; }
      }
      if (*n == Num && *n_lhs == Num) { n[1] = n_lhs[1] - n[1]; type = type_lhs; }
      else {
        *--n = (int)n_lhs; *--n = Sub;
        if (type_lhs > PTR && type > PTR) { *--n = sizeof(int); *--n = Num; --n; *n = (int)(n+3); *--n = Div; }
        type = (type_lhs == type) ? INT : type_lhs;
      }
    }
    else if (match(Mul)) { expr(Inc); if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] * n[1]; else { *--n = (int)n_lhs; *--n = Mul; } type = INT; }
    else if (match(Div)) { expr(Inc); if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] / n[1]; else { *--n = (int)n_lhs; *--n = Div; } type = INT; }
    else if (match(Mod)) { expr(Inc); if (*n==Num && *n_lhs==Num) n[1] = n_lhs[1] % n[1]; else { *--n = (int)n_lhs; *--n = Mod; } type = INT; }
    else if (tk == Inc || tk == Dec) {
      if (*n == Load) *n = tk;
      else error("bad lvalue in post-increment expr");
      *--n = (type > PTR) ? sizeof(int) : sizeof(char); *--n = Num;
      *--n = (int)n_lhs; *--n = (tk == Inc) ? Sub : Add;
      next();
    }
    else if (tk == Dot || tk == Arrow) {
      if (match(Dot)) type = type + PTR;
      if (type <= PTR+INT || type >= PTR2) error("structure expected");
      if (tk != Id) error("structure member expected");
      struct member_s *m = members[type - PTR];
      while (m && m->id != id) m = m->next;
      if (!m) error("structure member not found");
      if (m->offset) { *++e = PUSH; *++e = IMM; *++e = m->offset; *++e = ADD; } // TODO: generate AST entries
      type = m->type;
      if (type <= INT || type >= PTR) *++e = (type == CHAR) ? LC : LI; // TODO: generate AST entries
      next();
    }
    else if (match(Bracket)) {
      expr(Cond);
      expect(']', "']' bracket expected in array indexing expr");
      if (type_lhs < PTR) error("pointer type expected on lhs of array indexing expr");
      else if (type_lhs > PTR) {
        int sz = type_lhs >= PTR2 ? sizeof(int) : type_size[type_lhs - PTR];
        if (*n == Num) n[1] = n[1] * sz;
        else { *--n = sz; *--n = Num; --n; *n = (int)(n+3); *--n = Mul; }
      }
      if (*n == Num && *n_lhs == Num) n[1] = n_lhs[1] + n[1];
      else { *--n = (int)n_lhs; *--n = Add; }
      *--n = type = type_lhs - PTR; *--n = Load;
    }
    else { printf("%d: compiler error tk=%d\n", line, tk); exit(-1); }
  }
}

void stmt() {
  if (tk == Char || tk == Int || tk == Struct) { // local
    if (match(Char)) type = CHAR;
    else if (match(Int)) type = INT;
    else if (match(Struct)) {
      if (tk != Id) error("struct identifier expected in local definition");
      if (id->class != Struct) error("bad struct identifier in local definition: not a struct identifier");
      type = id->type;
      next();
    }

    while (match(Mul)) type = type + PTR;

    if (tk != Id) error("local identifier expected in local definition");
    if (id->class == Local) error("duplicate local definition");
    if (id->class) id = sym_add(Id, id->hash, id->str); // hide prev definition
    int sz = type >= PTR ? sizeof (int) : type_size[type]; // in bytes
    sz = (sz + sizeof (int) - 1) & -sizeof (int); // align
    loc = loc - sz/sizeof (int);
    id->class = Local;
    id->type = type;
    id->val = loc;
    next();
  }
  else if (match(If)) {
    int *n1, *n2, *n3;
    expect('(', "'(' expected in if");
    expr(Cond);
    n1 = n;
    expect(')', "')' expected in if");
    stmt();
    n2 = n;
    if (match(Else)) { stmt(); n3 = n; } else n3 = 0;
    *--n = (int)n3; *--n = (int)n2; *--n = (int)n1; *--n = Cond;
  }
  else if (match(While)) {
    int *n1;
    expect('(', "'(' expected in while");
    expr(Cond);
    n1 = n;
    expect(')', "')' expected in while");
    stmt();
    *--n = (int)n1; *--n = While;
  }
  else if (match(Return)) {
    int *n1;
    if (tk != ';') { expr(Cond); n1 = n; } else n1 = 0;
    expect(';', "';' expected in return");
    *--n = (int)n1; *--n = Return;
  }
  else if (match('{')) {
    struct ident_s *_sym = sym;
    int _loc = loc;
    *--n = ';';
    while (!match('}')) { int *n1 = n; stmt(); *--n = (int)n1; *--n = '{'; }
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
  int i = *n, *pp;
  if (i == Num) { *++e = IMM; *++e = n[1]; }
  else if (i == Local) { *++e = LEA; *++e = n[1]; }
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
    *++e = BZ; pp = ++e;
    gen((int *)n[2]);
    if (n[3]) { *pp = (int)(e + 3); *++e = JMP; pp = ++e; gen((int *)n[3]); }
    *pp = (int)(e + 1);
  }
  else if (i == Lor)  { gen((int *)n[1]); *++e = BNZ; pp = ++e; gen(n+2); *pp = (int)(e + 1); }
  else if (i == Land) { gen((int *)n[1]); *++e = BZ;  pp = ++e; gen(n+2); *pp = (int)(e + 1); }
  else if (i == Or)   { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = OR;  }
  else if (i == Xor)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = XOR; }
  else if (i == And)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = AND; }
  else if (i == Eq)   { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = EQ;  }
  else if (i == Ne)   { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = NE;  }
  else if (i == Lt)   { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = LT;  }
  else if (i == Gt)   { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = GT;  }
  else if (i == Le)   { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = LE;  }
  else if (i == Ge)   { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = GE;  }
  else if (i == Shl)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = SHL; }
  else if (i == Shr)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = SHR; }
  else if (i == Add)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = ADD; }
  else if (i == Sub)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = SUB; }
  else if (i == Mul)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = MUL; }
  else if (i == Div)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = DIV; }
  else if (i == Mod)  { gen((int *)n[1]); *++e = PUSH; gen(n+2); *++e = MOD; }
  else if (i == Sys || i == Fun) {
    pp = (int *)n[1];
    while (pp) { gen(pp+1); *++e = PUSH; pp = (int *)*pp; }
    if (i == Fun) { *++e = JSR; } *++e = n[2];
    if (n[3]) { *++e = ADJ; *++e = n[3]; }
  }
  else if (i == While) {
    *++e = JMP; pp = ++e; gen(n+2); *pp = (int)(e + 1);
    gen((int *)n[1]);
    *++e = BNZ; *++e = (int)(pp + 1);
  }
  else if (i == Return) { if (n[1]) gen((int *)n[1]); *++e = LEAVE; }
  else if (i == '{') { gen((int *)n[1]); gen(n+2); }
  else if (i == Enter) { *++e = ENTER; *++e = n[1]; gen(n+2); *++e = LEAVE; }
  else if (i != ';') { printf("%d: compiler error (i=%d)\n", line, i); exit(-1); }
}

int main(int argc, char **argv) {
  int fd, poolsz, *ast;
  struct ident_s *id_main;
  struct member_s *m;
  int *pc, *sp, *bp, a, cycle; // vm registers
  int i, *t; // temps

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { dbg = 1; --argc; ++argv; }
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
    int bt; // base type
    if (match(Char)) bt = CHAR;
    if (!match(Int)) bt = INT;
    else if (match(Enum)) {
      expect('{', "bad enum definition: '{' expected; no enum identifier allowed");
      i = 0;
      while (!match('}')) {
        if (tk != Id) error("bad enum identifier");
        if (id->class) error("duplicate enum identifier");
        struct ident_s *_id = id;
        next();
        if (match(Assign)) {
          n = ast;
          expr(Cond);
          if (*n != Num) error("bad enum initializer");
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
        if (members[bt]) error("duplicate struct definition");
        while (!match('}')) {
          int mbt = INT; // member base type
          if (match(Char)) { mbt = CHAR; }
          else if (match(Struct)) {
            if (tk != Id) error("struct identifier expected in struct member definition");
            if (id->class != Struct) error("bad struct identifier in struct member definition: not a struct identifier");
            mbt = id->type;
            next();
          }
          else if (!match(Int)) error("member base type expected");

          while (!match(';')) {
            type = mbt;

            while (match(Mul)) { type = type + PTR; }

            if (tk != Id) error("member identifier expected in struct member definition");
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
    else error("base type expected");

    while (!match(';')) {
      type = bt;

      while (match(Mul)) { type = type + PTR; }

      if (tk != Id) error("bad global definition: identifier expected");
      if (id->class) error("duplicate global definition");
      struct ident_s *_id = id;
      _id->type = type;
      next();
      if (tk == '(') { // function
        struct ident_s *_sym = sym; // save symbol table pointer; unwind it later
        _id->class = Fun;
        _id->val = (int)(e + 1);
        i = 2;
        next();
        while (!match(')')) { // parameter list
          if (match(Char)) type = CHAR;
          else if (match(Int)) type = INT;
          else if (match(Struct)) {
            if (tk != Id) error("struct identifier expected in parameter definition");
            if (id->class != Struct) error("bad struct identifier in parameter definition: not a struct identifier");
            type = id->type;
            next();
          }
          else error("base type expected");

          while (match(Mul)) type = type + PTR;

          if (tk != Id) { printf("%d: bad parameter definition\n", line); return -1; }
          if (id->class == Local) { printf("%d: duplicate parameter definition\n", line); return -1; }
          if (id->class) id = sym_add(Id, id->hash, id->str);
          int sz = type >= PTR ? sizeof (int) : type_size[type]; // in bytes
          sz = (sz + sizeof (int) - 1) & -sizeof (int); // align
          i = i - sz/sizeof (int);
          id->class = Local;
          id->type = type;
          id->val = i;
          next();
          if (!match(',') && tk != ')') { printf("%d: ',' or ')' expected in parameter definition\n", line); exit(-1); }
        }
        if (!match('{')) { printf("%d: bad function definition: function body expected\n", line); return -1; }
        n = ast;
        loc = 0;
        *--n = ';'; while (tk != '}') { t = n; stmt(); *--n = (int)t; *--n = '{'; }
        *--n = -i; *--n = Enter;
        gen(n);
        sym_unwind(_sym); // unwind symbol table locals
        tk = ';'; // break inner while
      }
      else {
        _id->class = Global;
        _id->val = (int)data;
        if (match(Bracket)) { // array
          n = ast;
          expr(Cond);
          if (*n != Num) error("bad global array initializer");
          data = data + ((_id->type == CHAR) ? (n[1] + sizeof (int) - 1) & -sizeof (int) : n[1] * sizeof(int));
          _id->type = _id->type + PTR;
          if (dbg) printf("_id->type:#%d\n", _id->type);
          expect(']', "']' expected in global array definition");
        }
        else data = data + sizeof(int);
      }
      if (!match(',') && tk != ';') error("',' or ';' expected");
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
  a = cycle = 0;
  while (1) {
    i = *pc++; ++cycle;
    if (dbg) {
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
    else if (i == PUSH)  *--sp = a;                                         // push

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

    else if (i == OPEN)   a = open((char *)*sp, sp[1]);
    else if (i == READ)   a = read(*sp, sp[1], sp[2]);
    else if (i == WRITE)  a = write(*sp, sp[1], sp[2]);
    else if (i == CLOSE)  a = close(*sp);
    else if (i == PRINTF) { t = sp + pc[1]; a = printf((char *)*sp, sp[1], sp[2], sp[3], sp[4], sp[5]); }
    else if (i == SCANF)  { t = sp + pc[1]; a = scanf((char *)*sp, sp[1], sp[2], sp[3], sp[4], sp[5]); }
    else if (i == MALLOC) a = (int)malloc(*sp);
    else if (i == FREE)   free((void *)*sp);
    else if (i == MEMSET) a = (int)memset((char *)*sp, sp[1], sp[2]);
    else if (i == MEMCMP) a = memcmp((char *)*sp, sp[1], sp[2]);
    else if (i == MEMCPY) a = (int)memcpy((char *)*sp, sp[1], sp[2]);
    else if (i == EXIT)   { if (dbg) printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }

    else if (i == EXIT) { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }
    else { printf("unknown instruction = %d! cycle = %d\n", i, cycle); return -1; }
  }
}
