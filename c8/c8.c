// c8.c - C in eight functions
//   no enum name

// Based on c4.c - C in four functions
// Written by Robert Swierczek

#include <unistd.h> // for read, write, close
#include <stdio.h>  // for printf, scanf
#include <stdlib.h> // for malloc, free
#include <memory.h> // for memset, memcmp, memcpy
#include <fcntl.h>  // for open

char *p, *lp, *tp, // current/line/token position in source code
     *d, *data,    // current data pointer
     *ops,         // opcodes
     *fn;          // filename

int *e, *le, *code, // current/line position in emitted code
    *stack,         // 
    *sym,           // symbol table (simple list of identifiers)
    *id, *ast,      // currently parsed identifier
    *n,             // current node in abstract syntax tree
    *idmain,        // 
    tk,             // current token
    ival,           // current token value
    ty,             // current expression type
    line,           // current line number
    src,            // print source and assembly flag
    dbg;            // print executed instructions

// tokens and classes (operators last and in precedence order)
enum {
  Num = 128, Fun, Sys, Global, Local, Id, Load, Enter,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Bracket
};

// opcodes (IMM...ADJ have parameter)
enum {
  IMM, LEA, JMP, JSR, BZ, BNZ, ENTER, ADJ, LEAVE, LI, LC, SI, SC, PUSH,
  OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD, 
  OPEN, READ, WRITE, CLOSE, PRINTF, SCANF, MALLOC, FREE, MEMSET, MEMCMP, MEMCPY, SBRK, BRK, EXIT
};

// types
enum { CHAR, INT, PTR };

// identifier offsets (since we can't create an ident struct)
enum { Tk, Hash, Name, Len, Class, Type, Val, HClass, HType, HVal, IdSz };

enum { SymSz = 1024*IdSz, PoolSz = 256*1024, CodeSz = PoolSz, DataSz = PoolSz, StackSz = PoolSz, AstSz = PoolSz, SrcSz = PoolSz };

void next() {
  char *pp;

  while ((tk = *p)) {
    tp = p++;
    if (tk == '\n') {
      if (src) {
        printf("%d: %.*s", line, p - lp, lp);
        while (le < e) {
          printf("%8.4s", &ops[*++le * 8]);
          if (*le <= ADJ) printf(" %d\n", *++le); else printf("\n");
        }
      }
      lp = p; ++line;
    }
    else if (tk == '#') {
      while (*p != 0 && *p != '\n') ++p;
    }
    else if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_') {
      pp = p - 1;
      while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        tk = tk * 147 + *p++;
      tk = (tk << 6) + (p - pp);
      id = sym;
      while (id[Tk]) {
        if (tk == id[Hash] && !memcmp((char *)id[Name], pp, p - pp)) { tk = id[Tk]; return; }
        id = id + IdSz; if (id >= sym + SymSz) { printf("%s:%d:%d: FATAL: symbol table overflow!\n", fn, line, tp - lp + 1); exit(-1); }
      }
      id[Hash] = tk;
      id[Name] = (int)pp;
      id[Len]  = p - pp;
      tk = id[Tk] = Id;
      return;
    }
    else if (tk >= '0' && tk <= '9') {
      if ((ival = tk - '0')) { while (*p >= '0' && *p <= '9') ival = ival * 10 + *p++ - '0'; }
      else if (*p == 'x' || *p == 'X') {
        while ((tk = *++p) && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F')))
          ival = ival * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
      }
      else { while (*p >= '0' && *p <= '7') ival = ival * 8 + *p++ - '0'; }
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
      pp = d;
      while (*p != 0 && *p != tk) {
        if ((ival = *p++) == '\\') {
          if ((ival = *p++) == 'n') ival = '\n';
          else if (ival == '0') ival = '\0';
        }
        if (tk == '"') *d++ = ival;
      }
      ++p;
      if (tk == '"') ival = (int)pp; else tk = Num;
      return;
    }
    else if (tk == '=') { if (*p == '=') { ++p; tk = Eq;   } else tk = Assign; return; }
    else if (tk == '+') { if (*p == '+') { ++p; tk = Inc;  } else tk = Add; return; }
    else if (tk == '-') { if (*p == '-') { ++p; tk = Dec;  } else tk = Sub; return; }
    else if (tk == '!') { if (*p == '=') { ++p; tk = Ne;   } return; }
    else if (tk == '<') { if (*p == '=') { ++p; tk = Le;   } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return; }
    else if (tk == '>') { if (*p == '=') { ++p; tk = Ge;   } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return; }
    else if (tk == '|') { if (*p == '|') { ++p; tk = Lor;  } else tk = Or; return; }
    else if (tk == '&') { if (*p == '&') { ++p; tk = Land; } else tk = And; return; }
    else if (tk == '^') { tk = Xor; return; }
    else if (tk == '%') { tk = Mod; return; }
    else if (tk == '*') { tk = Mul; return; }
    else if (tk == '[') { tk = Bracket; return; }
    else if (tk == '?') { tk = Cond; return; }
    else if (tk == '~' || tk == ';' || tk == '{' || tk == '}' || tk == '(' || tk == ')' || tk == ']' || tk == ',' || tk == ':') return;
  }
}

int match(int _tk) {
  if (_tk == tk) { next(); return 1; }
  return 0;
}

void expr(int lev) {
  int *_id, i, _ty, *_n, *pp;

  if (!tk) { printf("%s:%d:%d: unexpected eof in expression\n", fn, line, tp - lp + 1); exit(-1); }
  else if (tk == Num) {
    *--n = ival; *--n = Num; next();
    ty = INT;
  }
  else if (tk == '"') {
    *--n = ival; *--n = Num; next();
    while (match('"')) ;
    d = (char *)(((int)d + sizeof(int)) & -sizeof(int)); ty = PTR;
  }
  else if (match(Sizeof)) {
    if (!match('(')) { printf("%s:%d:%d: '(' expected in sizeof\n", fn, line, tp - lp + 1); exit(-1); }
    if (match(Int)) ty = INT;
    else if (match(Char)) { ty = CHAR; }
    else { printf("%s:%d:%d: type expected in sizeof\n", fn, line, tp - lp + 1); exit(-1); }
    while (match(Mul)) ty = ty + PTR;
    if (!match(')')) { printf("%s:%d:%d: ')' expected in sizeof\n", fn, line, tp - lp + 1); exit(-1); }
    *--n = (ty == CHAR) ? sizeof(char) : sizeof(int); *--n = Num;
    ty = INT;
  }
  else if (tk == Id) {
    _id = id; next();
    if (match('(')) {
      if (_id[Class] != Sys && _id[Class] != Fun) { printf("%s:%d:%d: bad function call\n", fn, line, tp - lp + 1); exit(-1); }
      i = 0; pp = 0;
      while (!match(')')) {
        expr(Assign); *--n = (int)pp; pp = n; ++i;
        if (!match(',') && tk != ')') { printf("%s:%d:%d: ',' or ')' expected in function call\n", fn, line, tp - lp + 1); exit(-1); }
      }
      *--n = i; *--n = _id[Val]; *--n = (int)pp; *--n = _id[Class];
      ty = _id[Type];
    }
    else if (_id[Class] == Num) {
      *--n = _id[Val]; *--n = Num;
      ty = INT;
    }
    else {
      if (_id[Class] == Local) { *--n = _id[Val]; *--n = Local; }
      else if (_id[Class] == Global) { *--n = _id[Val]; *--n = Num; }
      else { printf("%s:%d:%d: undefined variable\n", fn, line, tp - lp + 1); exit(-1); }
      *--n = ty = _id[Type]; *--n = Load;
    }
  }
  else if (match('(')) {
    if (tk == Int || tk == Char) {
      _ty = (tk == Int) ? INT : CHAR; next();
      while (match(Mul)) _ty = _ty + PTR;
      if (!match(')')) { printf("%s:%d:%d: bad cast\n", fn, line, tp - lp + 1); exit(-1); }
      expr(Inc);
      ty = _ty;
    }
    else {
      expr(Assign);
      if (!match(')')) { printf("%s:%d:%d: ')' expected\n", fn, line, tp - lp + 1); exit(-1); }
    }
  }
  else if (match(Mul)) {
    expr(Inc);
    if (ty > INT) ty = ty - PTR;
    else { printf("%s:%d:%d: bad dereference\n", fn, line, tp - lp + 1); exit(-1); }
    *--n = ty; *--n = Load;
  }
  else if (match(And)) {
    expr(Inc);
    if (*n == Load) n = n+2;
    else { printf("%s:%d:%d: bad address-of\n", fn, line, tp - lp + 1); exit(-1); }
    ty = ty + PTR;
  }
  else if (match('!')) {
    expr(Inc);
    if (*n == Num) n[1] = !n[1];
    else { *--n = 0; *--n = Num; --n; *n = (int)(n+3); *--n = Eq; }
    ty = INT;
  }
  else if (match('~')) {
    expr(Inc);
    if (*n == Num) n[1] = ~n[1]; else { *--n = -1; *--n = Num; --n; *n = (int)(n+3); *--n = Xor; }
    ty = INT;
  }
  else if (match(Add)) { expr(Inc); ty = INT; }
  else if (match(Sub)) {
    expr(Inc);
    if (*n == Num) n[1] = -n[1]; else { *--n = -1; *--n = Num; --n; *n = (int)(n+3); *--n = Mul; }
    ty = INT;
  }
  else if (tk == Inc || tk == Dec) {
    i = tk; next();
    expr(Inc);
    if (*n == Load) *n = i;
    else { printf("%s:%d:%d: bad lvalue in pre-increment\n", fn, line, tp - lp + 1); exit(-1); }
  }
  else { printf("%s:%d:%d: bad expression\n", fn, line, tp - lp + 1); exit(-1); }

  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    _ty = ty; _n = n;
    if (match(Assign)) {
      if (*n != Load) { printf("%s:%d:%d: bad lvalue in assignment\n", fn, line, tp - lp + 1); exit(-1); }
      expr(Assign);
      *--n = (int)(_n+2); *--n = ty = _ty; *--n = Assign;
    }
    else if (match(Cond)) {
      expr(Assign);
      if (!match(':')) { printf("%s:%d:%d: conditional missing colon\n", fn, line, tp - lp + 1); exit(-1); }
      pp = n;
      expr(Cond);
      --n; *n = (int)(n+1); *--n = (int)pp; *--n = (int)_n; *--n = Cond;
    }
    else if (match(Lor))  { expr(Land); if (*n==Num && *_n==Num) n[1] = _n[1] || n[1]; else { *--n = (int)_n; *--n = Lor;  } ty = INT; }
    else if (match(Land)) { expr(Or);   if (*n==Num && *_n==Num) n[1] = _n[1] && n[1]; else { *--n = (int)_n; *--n = Land; } ty = INT; }
    else if (match(Or))   { expr(Xor);  if (*n==Num && *_n==Num) n[1] = _n[1] |  n[1]; else { *--n = (int)_n; *--n = Or;   } ty = INT; }
    else if (match(Xor))  { expr(And);  if (*n==Num && *_n==Num) n[1] = _n[1] ^  n[1]; else { *--n = (int)_n; *--n = Xor;  } ty = INT; }
    else if (match(And))  { expr(Eq);   if (*n==Num && *_n==Num) n[1] = _n[1] &  n[1]; else { *--n = (int)_n; *--n = And;  } ty = INT; }
    else if (match(Eq))   { expr(Lt);   if (*n==Num && *_n==Num) n[1] = _n[1] == n[1]; else { *--n = (int)_n; *--n = Eq;   } ty = INT; }
    else if (match(Ne))   { expr(Lt);   if (*n==Num && *_n==Num) n[1] = _n[1] != n[1]; else { *--n = (int)_n; *--n = Ne;   } ty = INT; }
    else if (match(Lt))   { expr(Shl);  if (*n==Num && *_n==Num) n[1] = _n[1] <  n[1]; else { *--n = (int)_n; *--n = Lt;   } ty = INT; }
    else if (match(Gt))   { expr(Shl);  if (*n==Num && *_n==Num) n[1] = _n[1] >  n[1]; else { *--n = (int)_n; *--n = Gt;   } ty = INT; }
    else if (match(Le))   { expr(Shl);  if (*n==Num && *_n==Num) n[1] = _n[1] <= n[1]; else { *--n = (int)_n; *--n = Le;   } ty = INT; }
    else if (match(Ge))   { expr(Shl);  if (*n==Num && *_n==Num) n[1] = _n[1] >= n[1]; else { *--n = (int)_n; *--n = Ge;   } ty = INT; }
    else if (match(Shl))  { expr(Add);  if (*n==Num && *_n==Num) n[1] = _n[1] << n[1]; else { *--n = (int)_n; *--n = Shl;  } ty = INT; }
    else if (match(Shr))  { expr(Add);  if (*n==Num && *_n==Num) n[1] = _n[1] >> n[1]; else { *--n = (int)_n; *--n = Shr;  } ty = INT; }
    else if (match(Add)) {
      expr(Mul);
      if (ty >= PTR) { printf("%s:%d:%d: bad pointer addition\n", fn, line, tp - lp + 1); exit(-1); }
      if (_ty > PTR) {
        if (*n == Num) n[1] = n[1] * sizeof(int); // lhs > PTR && rhs == Num
        else { *--n = sizeof(int); *--n = Num; --n; *n = (int)(n+3); *--n = Mul; } // lhs > PTR && rhs != Num
      }
      if (*n == Num && *_n == Num) { n[1] = _n[1] + n[1]; ty = _ty; }
      else { *--n = (int)_n; *--n = Add; ty = (_ty == ty) ? INT : _ty; }
    }
    else if (match(Sub)) {
      expr(Mul);
      if (_ty < PTR && ty >= PTR) { printf("%s:%d:%d: bad pointer subtraction\n", fn, line, tp - lp + 1); exit(-1); }
      if (_ty >= PTR && ty >= PTR && _ty != ty) { printf("%s:%d:%d: bad pointer types in subtraction\n", fn, line, tp - lp + 1); exit(-1); }
      if (_ty > PTR) {
        if (*n == Num) n[1] = n[1] * sizeof(int);
        else if (_ty != ty) { *--n = sizeof(int); *--n = Num; --n; *n = (int)(n+3); *--n = Mul; }
      }
      if (*n == Num && *_n == Num) { n[1] = _n[1] - n[1]; ty = _ty; }
      else {
        *--n = (int)_n; *--n = Sub;
        if (_ty > PTR && ty > PTR) { *--n = sizeof(int); *--n = Num; --n; *n = (int)(n+3); *--n = Div; }
        ty = (_ty == ty) ? INT : _ty;
      }
    }
    else if (match(Mul)) { expr(Inc); if (*n==Num && *_n==Num) n[1] = _n[1] * n[1]; else { *--n = (int)_n; *--n = Mul; } ty = INT; }
    else if (match(Div)) { expr(Inc); if (*n==Num && *_n==Num) n[1] = _n[1] / n[1]; else { *--n = (int)_n; *--n = Div; } ty = INT; }
    else if (match(Mod)) { expr(Inc); if (*n==Num && *_n==Num) n[1] = _n[1] % n[1]; else { *--n = (int)_n; *--n = Mod; } ty = INT; }
    else if (tk == Inc || tk == Dec) {
      if (*n == Load) *n = tk;
      else { printf("%s:%d:%d: bad lvalue in post-increment\n", fn, line, tp - lp + 1); exit(-1); }
      *--n = (ty > PTR) ? sizeof(int) : sizeof(char); *--n = Num;
      *--n = (int)_n; *--n = (tk == Inc) ? Sub : Add; next();
    }
    else if (match(Bracket)) {
      if (_ty < PTR) { printf("%s:%d:%d: pointer type expected\n", fn, line, tp - lp + 1); exit(-1); }
      expr(Assign);
      if (!match(']')) { printf("%s:%d:%d: ']' expected\n", fn, line, tp - lp + 1); exit(-1); }
      if (_ty > PTR) { if (*n == Num) n[1] = n[1] * sizeof(int); else { *--n = sizeof(int); *--n = Num; --n; *n = (int)(n+3); *--n = Mul; } }
      if (*n == Num && *_n == Num) n[1] = _n[1] + n[1]; else { *--n = (int)_n; *--n = Add; }
      *--n = ty = _ty - PTR; *--n = Load;
    }
    else { printf("%s:%d:%d: compiler error (tk=%d)\n", fn, line, tp - lp + 1, tk); exit(-1); }
  }
}

void stmt() {
  int *n1, *n2, *n3;

  if (match(If)) {
    if (!match('(')) { printf("%s:%d:%d: '(' expected in if\n", fn, line, tp - lp + 1); exit(-1); }
    expr(Assign); n1 = n;
    if (!match(')')) { printf("%s:%d:%d: ')' expected in if\n", fn, line, tp - lp + 1); exit(-1); }
    stmt(); n2 = n;
    if (match(Else)) { stmt(); n3 = n; } else n3 = 0;
    *--n = (int)n3; *--n = (int)n2; *--n = (int)n1; *--n = Cond;
  }
  else if (match(While)) {
    if (!match('(')) { printf("%s:%d:%d: '(' expected in while\n", fn, line, tp - lp + 1); exit(-1); }
    expr(Assign); n1 = n;
    if (!match(')')) { printf("%s:%d:%d: ')' expected in while\n", fn, line, tp - lp + 1); exit(-1); }
    stmt();
    *--n = (int)n1; *--n = While;
  }
  else if (match(Return)) {
    if (tk != ';') { expr(Assign); n1 = n; } else n1 = 0;
    if (!match(';')) { printf("%s:%d:%d: ';' expected in return\n", fn, line, tp - lp + 1); exit(-1); }
    *--n = (int)n1; *--n = Return;
  }
  else if (match('{')) {
    *--n = ';';
    while (!match('}')) { n1 = n; stmt(); *--n = (int)n1; *--n = '{'; }
  }
  else if (match(';')) {
    *--n = ';';
  }
  else {
    expr(Assign);
    if (!match(';')) { printf("%s:%d:%d: ';' expected\n", fn, line, tp - lp + 1); exit(-1); }
  }
}

void gen(int *n) { // hide global n
  int i, *pp;

  if (n < ast) printf("%s:%d:%d: FATAL: abstract syntax tree overflow\n", fn, line, tp - lp + 1);

  i = *n;
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
  else if (i != ';') { printf("%s:%d:%d: compiler error (i=%d)\n", fn, line, tp - lp + 1, i); exit(-1); }
}

void parse() {
  int ty; // hide global ty
  char *src; // hide global src
  int fd, *top, bt, i, *_id, *_n;

  if (!(lp = p = src = malloc(SrcSz))) { printf("FATAL: could not malloc(%d) source area\n", SrcSz); exit(-1); }

  if ((fd = open(fn, 0)) < 0) { printf("FATAL: could not open(%s)\n", fn); exit(-1); }
  if ((i = read(fd, p, SrcSz-1)) <= 0) { printf("FATAL: read() returned %d\n", i); exit(-1); }
  close(fd);
  p[i] = 0;

  if (!(ast = malloc(AstSz))) { printf("FATAL: could not malloc(%d) abstract syntax tree area\n", AstSz); exit(-1); }
  top = (int *)((int)ast + AstSz); // abstract syntax tree is most efficiently built as a stack

  line = 1; next();
  while (tk) {
    bt = INT; // basetype
    if (match(Char)) bt = CHAR;
    else if (match(Enum)) {
      if (!match('{')) { printf("%s:%d:%d: bad enum definition\n", fn, line, tp - lp + 1); exit(-1); }
      i = 0;
      while (!match('}')) {
        if (tk != Id) { printf("%s:%d:%d: bad enum identifier\n", fn, line, tp - lp + 1); exit(-1); }
        if (id[Class]) { printf("%s:%d:%d: duplicate enum identifier\n", fn, line, tp - lp + 1); exit(-1); }
        _id = id; next();
        if (match(Assign)) {
          n = top;
          expr(Cond);
          if (*n != Num) { printf("%s:%d:%d: bad enum initializer\n", fn, line, tp - lp + 1); exit(-1); }
          i = n[1];
        }
        _id[Class] = Num; _id[Type] = INT; _id[Val] = i++;
        if (!match(',') && tk != '}') { printf("%s:%d:%d: ',' or '}' expected in enum declaration\n", fn, line, tp - lp + 1); exit(-1); }
      }
    }
    else if (!match(Int)) { printf("%s:%d:%d: type expected\n", fn, line, tp - lp + 1); exit(-1); }

    while (!match(';')) {
      ty = bt;
      while (match(Mul)) ty = ty + PTR;

      if (tk != Id) { printf("%s:%d:%d: bad global declaration\n", fn, line, tp - lp + 1); exit(-1); }
      if (id[Class]) { printf("%s:%d:%d: duplicate global definition\n", fn, line, tp - lp + 1); exit(-1); }
      id[Type] = ty;
      _id = id; next();

      if (match('(')) { // function
        _id[Class] = Fun;
        _id[Val] = (int)(e + 1);

        i = 2;
        while (!match(')')) {
          if (match(Int)) ty = INT;
          else if (match(Char)) ty = CHAR;
          else { printf("%s:%d:%d: parameter type expected\n", fn, line, tp - lp + 1); exit(-1); }
          while (match(Mul))ty = ty + PTR;
          if (tk != Id) { printf("%s:%d:%d: bad parameter declaration\n", fn, line, tp - lp + 1); exit(-1); }
          if (id[Class] == Local) { printf("%s:%d:%d: duplicate parameter definition\n", fn, line, tp - lp + 1); exit(-1); }
          id[HClass] = id[Class]; id[Class] = Local;
          id[HType]  = id[Type];  id[Type]  = ty;
          id[HVal]   = id[Val];   id[Val]   = i++;
          next();
          if (!match(',') && tk != ')') { printf("%s:%d:%d: ',' or ')' expected in parameter declaration\n", fn, line, tp - lp + 1); exit(-1); }
        }

        if (!match('{')) { printf("%s:%d:%d: bad function definition\n", fn, line, tp - lp + 1); exit(-1); }
        i = 0;
        while (tk == Int || tk == Char) {
          bt = (tk == Int) ? INT : CHAR; next();
          while (!match(';')) {
            ty = bt;
            while (match(Mul)) ty = ty + PTR;
            if (tk != Id) { printf("%s:%d:%d: bad local declaration\n", fn, line, tp - lp + 1); exit(-1); }
            if (id[Class] == Local) { printf("%s:%d:%d: duplicate local definition\n", fn, line, tp - lp + 1); exit(-1); }
            _id = id; next();
            if (match(Bracket)) {
              n = top;
              expr(Cond);
              if (*n != Num) { printf("%s:%d:%d: bad local array initializer\n", fn, line, tp - lp + 1); exit(-1); }
              i = i - ((ty == CHAR) ? ((n[1] + sizeof (int) - 1) & -sizeof (int))/sizeof (int) : n[1]);
              ty = ty + PTR;
              if (dbg) printf("i:%d, ty:#%d\n", i, ty);
              if (!match(']')) { printf("%s:%d:%d: ']' expected in local array declaration\n", fn, line, tp - lp + 1); exit(-1); }
            }
            else --i;
            _id[HClass] = _id[Class]; _id[Class] = Local;
            _id[HType]  = _id[Type];  _id[Type]  = ty;
            _id[HVal]   = _id[Val];   _id[Val]   = i;
            if (!match(',') && tk != ';') { printf("%s:%d:%d: ',' or ';' expected in local declaration\n", fn, line, tp - lp + 1); exit(-1); }
          }
        }
        n = top;
        *--n = ';'; while (tk != '}') { _n = n; stmt(); *--n = (int)_n; *--n = '{'; }
        *--n = -i; *--n = Enter;
        gen(n);

        id = sym; // unwind symbol table locals
        while (id[Tk]) {
          if (id[Class] == Local) {
            id[Class] = id[HClass];
            id[Type]  = id[HType];
            id[Val]   = id[HVal];
          }
          id = id + IdSz;
        }
        tk = ';'; // break inner while
      }

      else {
        _id[Class] = Global;
        _id[Val] = (int)d;
        if (match(Bracket)) {
          n = top;
          expr(Cond);
          if (*n != Num) { printf("%s:%d:%d: bad global array initializer\n", fn, line, tp - lp + 1); exit(-1); }
          d = d + ((_id[Type] == CHAR) ? (n[1] + sizeof (int) - 1) & -sizeof (int) : n[1] * sizeof(int));
          _id[Type] = _id[Type] + PTR;
          if (dbg) printf("_id[Type]:#%d\n", _id[Type]);
          if (!match(']')) { printf("%s:%d:%d: ']' expected in global array declaration\n", fn, line, tp - lp + 1); exit(-1); }
        }
        else d = d + sizeof(int);
      }
      if (!match(',') && tk != ';') { printf("%s:%d:%d: ',' or ';' expected\n", fn, line, tp - lp + 1); exit(-1); }
    }
  }

  free(ast);
  free(src); lp = p = 0;
}

int run(int argc, char **argv) {
  int *pc, *sp, *bp, a; // vm registers
  int i, *pp, cycle;

  if (dbg) printf("DBG: code size:%d data size:%d\n", e - code, d - data);

  if (!(pc = (int *)idmain[Val])) { printf("main() not defined\n"); exit(-1); }
  if (src) return 0;

  // call exit if main returns
  *++e = PUSH; pp = e;
  *++e = EXIT;
  // setup stack
  bp = sp = (int *)((int)stack + StackSz);
  *--sp = (int)argv;
  *--sp = argc;
  *--sp = (int)pp;

  // run...
  a = cycle = 0;
  while (1) {
    i = *pc++; ++cycle;
    if (dbg) {
      printf("%d> %s", cycle,
        &ops[i * 8]);
      if (i <= ADJ) printf(" %d\n", *pc); else printf("\n");
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
    else if (i == READ)   a = read(*sp, (char *)sp[1], sp[2]);
    else if (i == WRITE)  a = write(*sp, (char *)sp[1], sp[2]);
    else if (i == CLOSE)  a = close(*sp);
    else if (i == PRINTF) a = printf((char *)*sp, sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7]);
    else if (i == SCANF)  a = scanf((char *)*sp, sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7]);
    else if (i == MALLOC) a = (int)malloc(*sp);
    else if (i == FREE)   free((char *)*sp);
    else if (i == MEMSET) a = (int)memset((char *)*sp, sp[1], sp[2]);
    else if (i == MEMCMP) a = memcmp((char *)*sp, (char *)sp[1], sp[2]);
    else if (i == MEMCPY) a = (int)memcpy((char *)*sp, (char *)sp[1], sp[2]);
    else if (i == SBRK)   { a = (int)d; d = d + *sp; }
    else if (i == BRK)    d = (char *)*sp;
    else if (i == EXIT)   { if (dbg) printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }

    else { printf("unknown instruction = %d! cycle = %d\n", i, cycle); exit(-1); }
  }
  return -1;
}

int main(int argc, char **argv) {
  int i;

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { dbg = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c7 [-s] [-d] file ...\n"); return -1; }

  fn = *argv;

  if (dbg) printf("DBG: sizeof (int):%d SymSz:%d CodeSz:%d DataSz:%d StackSz:%d AstSz:%d SrcSz:%d\n", sizeof (int), SymSz, CodeSz, DataSz, StackSz, AstSz, SrcSz);

  if (!(sym = malloc(SymSz))) { printf("FATAL: could not malloc(%d) symbol area\n", SymSz); return -1; }
  memset(sym, 0, SymSz);
  if (!(le = e = code = malloc(CodeSz))) { printf("FATAL: could not malloc(%d) code area\n", CodeSz); return -1; }
  memset(e, 0, CodeSz);
  if (!(d = data = malloc(DataSz))) { printf("FATAL: could not malloc(%d) data area\n", DataSz); return -1; }
  memset(d, 0, DataSz);
  if (!(stack = malloc(StackSz))) { printf("FATAL: could not malloc(%d) stack area\n", StackSz); exit(-1); }

  ops = "IMM\0    LEA\0    JMP\0    JSR\0    BZ\0     BNZ\0    ENTER\0  ADJ\0    LEAVE\0  LI\0     LC\0     SI\0     SC\0     PUSH\0   "
        "OR\0     XOR\0    AND\0    EQ\0     NE\0     LT\0     GT\0     LE\0     GE\0     SHL\0    SHR\0    ADD\0    SUB\0    MUL\0    DIV\0    MOD\0    "
        "OPEN\0   READ\0   WRITE\0  CLOSE\0  PRINTF\0 SCANF\0  MALLOC\0 FREE\0   MEMSET\0 MEMCMP\0 MEMCPY\0 SBRK\0   BRK\0    EXIT\0   ";

  line = 0;
  lp = p = "char else enum if int return sizeof while "
           "open read write close printf scanf malloc free memset memcmp memcpy sbrk brk exit "
           "void main";
  i = Char; while (i <= While) { next(); id[Tk] = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { next(); id[Class] = Sys; id[Type] = INT; id[Val] = i++; } // add library to symbol table
  next(); id[Tk] = Char; // handle void type
  next(); idmain = id; // keep track of main

  parse();

  free(sym); sym = 0;

  return run(argc, argv);
}
