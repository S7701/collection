// c5.c - C in five functions

// c4.c plus
//   abstract syntax tree creation
//   back-end code generator
//   parameters passed in correct order
//   various optimizations

// Written by Robert Swierczek

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
//#include <unistd.h>
#include <io.h>
#include <fcntl.h>
//#define int long long

char *p, *lp, // current position in source code
     *data,   // data/bss pointer
     *ops;    // opcodes

int *e, *le,  // current position in emitted code
    *id,      // currently parsed identifier
    *n,       // current node in abstract syntax tree
    *sym,     // symbol table (simple list of identifiers)
    tk,       // current token
    val,      // current token value
    ty,       // current expression type
    line,     // current line number
    src,      // print source and assembly flag
    debug;    // print executed instructions

// tokens and classes (operators last and in precedence order)
enum Token { Num = 128, Fun, Sys, Glo, Loc, Id, Load, Enter,
             Char, Else, Enum, If, Int, Return, Sizeof, While,
             Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Bracket };

// opcodes
enum Opcode { IMM, LEA, JMP, JSR, BZ, BNZ, ENTER, ADJ, LEAVE, LI, LC, SI, SC, PUSH,
              OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
              OPEN, READ, WRITE, CLOSE, PRINTF, MALLOC, FREE, MEMSET, MEMCPY, MEMCMP, EXIT };

// types
enum Type { CHAR, INT, PTR };

// identifier offsets (since we can't create an ident struct)
enum Identifier { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

void next(void) {
  char *pp; // previous position

  while (tk = *p) {
    ++p;
    if (tk == '\n') { // end of line
      if (src) {
        printf("%d: %.*s", line, p - lp, lp);
        lp = p;
        while (le < e) {
          printf("    %s", &ops[*++le * 8]);
          if (*le <= ADJ) { ++le; printf(" %d (0x%08X)\n", *le, *le); } else printf("\n");
        }
      }
      ++line;
    }
    else if (tk == '#') { // treat preprocessor lines as line comments
      while (*p != 0 && *p != '\n') ++p;
    }
    else if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_') { // identifier
      pp = p - 1;
      while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        tk = tk * 147 + *p++;
      tk = (tk << 6) + (p - pp);
      id = sym;
      while (id[Tk]) {
        if (tk == id[Hash] && !memcmp((char *)id[Name], pp, p - pp)) { tk = id[Tk]; return; }
        id = id + Idsz;
      }
      id[Name] = (int)pp;
      id[Hash] = tk;
      tk = id[Tk] = Id;
      return;
    }
    else if (tk >= '0' && tk <= '9') { // number
      if (val = tk - '0') { while (*p >= '0' && *p <= '9') val = val * 10 + *p++ - '0'; }
      else if (*p == 'x' || *p == 'X') {
        while ((tk = *++p) && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F')))
          val = val * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
      }
      else { while (*p >= '0' && *p <= '7') val = val * 8 + *p++ - '0'; }
      tk = Num;
      return;
    }
    else if (tk == '/') { // line comment or operator /
      if (*p == '/') {
        ++p;
        while (*p != 0 && *p != '\n') ++p;
      }
      else {
        tk = Div;
        return;
      }
    }
    else if (tk == '\'' || tk == '"') { // string or character literal
      pp = data;
      while (*p != 0 && *p != tk) {
        if ((val = *p++) == '\\') {
          if ((val = *p++) == '0') val = '\0';
          else if (val == 't') val = '\t';
          else if (val == 'v') val = '\v';
          else if (val == 'f') val = '\f';
          else if (val == 'r') val = '\r';
          else if (val == 'n') val = '\n';
        }
        if (tk == '"') *data++ = (char)val;
      }
      ++p;
      if (tk == '"') val = (int)pp; else tk = Num;
      return;
    }
    else if (tk == '=') { if (*p == '=') { ++p; tk = Eq; } else tk = Assign; return; } // operator == or =
    else if (tk == '+') { if (*p == '+') { ++p; tk = Inc; } else tk = Add; return; }   // operator ++ or +
    else if (tk == '-') { if (*p == '-') { ++p; tk = Dec; } else tk = Sub; return; }   // operator -- or -
    else if (tk == '!') { if (*p == '=') { ++p; tk = Ne; } return; }                   // operator != or !
    else if (tk == '<') { if (*p == '=') { ++p; tk = Le; } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return; } // operator <= or << or <
    else if (tk == '>') { if (*p == '=') { ++p; tk = Ge; } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return; } // operator >= or >> or >
    else if (tk == '|') { if (*p == '|') { ++p; tk = Lor; } else tk = Or; return; }    // operator || or |
    else if (tk == '&') { if (*p == '&') { ++p; tk = Land; } else tk = And; return; }  // operator && or &
    else if (tk == '^') { tk = Xor; return; }     // operator ^
    else if (tk == '%') { tk = Mod; return; }     // operator %
    else if (tk == '*') { tk = Mul; return; }     // operator *
    else if (tk == '?') { tk = Cond; return; }    // operator ?
    else if (tk == '[') { tk = Bracket; return; } // operator []
    else if (tk != ' ' && tk != '\t' && tk != '\v' && tk != '\f' && tk != '\r') return; // skip whitespaces
  }
}

void expr(int lev) {
  int t, *d, *b;

  if (!tk) { printf("%d: unexpected eof in expression\n", line); exit(-1); }
  else if (tk == Num) { *--n = val; *--n = Num; next(); ty = INT; } // number or character literal
  else if (tk == '"') { // string literal
    *--n = val; *--n = Num; next();
    while (tk == '"') next();
    data = (char *)((int)data + sizeof(int) & -sizeof(int)); ty = PTR;
  }
  else if (tk == Sizeof) { // sizeof operator
    next(); if (tk == '(') next(); else { printf("%d: open paren expected in sizeof (tk=%d)\n", line, tk); exit(-1); }
    ty = INT; if (tk == Int) next(); else if (tk == Char) { next(); ty = CHAR; }
    while (tk == Mul) { next(); ty = ty + PTR; }
    if (tk == ')') next(); else { printf("%d: close paren expected in sizeof (tk=%d)\n", line, tk); exit(-1); }
    *--n = (ty == CHAR) ? sizeof(char) : sizeof(int); *--n = Num;
    ty = INT;
  }
  else if (tk == Id) { // identifier
    d = id; next();
    if (tk == '(') { // function call
      if (d[Class] != Sys && d[Class] != Fun) { printf("%d: bad function call (class=%d)\n", line, d[Class]); exit(-1); }
      next();
      t = 0; b = 0;
      while (tk != ')') { expr(Assign); *--n = (int)b; b = n; ++t; if (tk == ',') next(); } // parameter
      next();
      *--n = t; *--n = d[Val]; *--n = (int)b; *--n = d[Class];
      ty = d[Type];
    }
    else if (d[Class] == Num) { *--n = d[Val]; *--n = Num; ty = INT; } // enum identifier
    else { // variable
      if (d[Class] == Loc) { *--n = d[Val]; *--n = Loc; }
      else if (d[Class] == Glo) { *--n = d[Val]; *--n = Num; }
      else { printf("%d: undefined variable (class=%d)\n", line, d[Class]); exit(-1); }
      *--n = ty = d[Type]; *--n = Load;
    }
  }
  else if (tk == '(') { // type cast or group
    next();
    if (tk == Int || tk == Char) { // type cast
      t = (tk == Int) ? INT : CHAR; next();
      while (tk == Mul) { next(); t = t + PTR; }
      if (tk == ')') next(); else { printf("%d: bad cast (tk=%d)\n", line, tk); exit(-1); }
      expr(Inc);
      ty = t;
    }
    else { // group
      expr(Assign);
      if (tk == ')') next(); else { printf("%d: close paren expected (tk=%d)\n", line, tk); exit(-1); }
    }
  }
  else if (tk == Mul) { // unary dereference operator
    next(); expr(Inc);
    if (ty > INT) ty = ty - PTR; else { printf("%d: bad dereference (ty=%d)\n", line, ty); exit(-1); }
    *--n = ty; *--n = Load;
  }
  else if (tk == And) { // unary address of operator
    next(); expr(Inc);
    if (*n == Load) n = n + 2; else { printf("%d: bad address-of (node=%d)\n", line, *n); exit(-1); }
    ty = ty + PTR;
  }
  else if (tk == '!') { // logical not operator
    next(); expr(Inc);
    if (*n == Num) n[1] = !n[1]; else { *--n = 0; *--n = Num; --n; *n = (int)(n+3); *--n = Eq; }
    ty = INT;
  }
  else if (tk == '~') { // binary not operator
    next(); expr(Inc);
    if (*n == Num) n[1] = ~n[1]; else { *--n = -1; *--n = Num; --n; *n = (int)(n+3); *--n = Xor; }
    ty = INT;
  }
  else if (tk == Add) { next(); expr(Inc); ty = INT; } // unary plus operator
  else if (tk == Sub) { // unary minus operator
    next(); expr(Inc);
    if (*n == Num) n[1] = -n[1]; else { *--n = -1; *--n = Num; --n; *n = (int)(n+3); *--n = Mul; }
    ty = INT;
  }
  else if (tk == Inc || tk == Dec) { // pre-increment/-decrement
    t = tk; next(); expr(Inc);
    if (*n == Load) *n = t; else { printf("%d: bad lvalue in pre-increment/-decrement (node=%d)\n", line, *n); exit(-1); }
  }
  else { printf("%d: bad expression (tk=%d)\n", line, tk); exit(-1); }

  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    t = ty; b = n;
    if (tk == Assign) {
      next();
      if (*n != Load) { printf("%d: bad lvalue in assignment (node=%d)\n", line, *n); exit(-1); }
      expr(Assign); *--n = (int)(b+2); *--n = ty = t; *--n = Assign;
    }
    else if (tk == Cond) {
      next();
      expr(Assign);
      if (tk == ':') next(); else { printf("%d: conditional missing colon (tk=%d)\n", line, tk); exit(-1); }
      d = n;
      expr(Cond);
      --n; *n = (int)(n+1); *--n = (int)d; *--n = (int)b; *--n = Cond;
    }
    else if (tk == Lor)  { next(); expr(Land); if (*n==Num && *b==Num) n[1] = b[1] || n[1]; else { *--n = (int)b; *--n = Lor;  } ty = INT; }
    else if (tk == Land) { next(); expr(Or);   if (*n==Num && *b==Num) n[1] = b[1] && n[1]; else { *--n = (int)b; *--n = Land; } ty = INT; }
    else if (tk == Or)  { next(); expr(Xor); if (*n==Num && *b==Num) n[1] = b[1] |  n[1]; else { *--n = (int)b; *--n = Or;  } ty = INT; }
    else if (tk == Xor) { next(); expr(And); if (*n==Num && *b==Num) n[1] = b[1] ^  n[1]; else { *--n = (int)b; *--n = Xor; } ty = INT; }
    else if (tk == And) { next(); expr(Eq);  if (*n==Num && *b==Num) n[1] = b[1] &  n[1]; else { *--n = (int)b; *--n = And; } ty = INT; }
    else if (tk == Eq)  { next(); expr(Lt);  if (*n==Num && *b==Num) n[1] = b[1] == n[1]; else { *--n = (int)b; *--n = Eq;  } ty = INT; }
    else if (tk == Ne)  { next(); expr(Lt);  if (*n==Num && *b==Num) n[1] = b[1] != n[1]; else { *--n = (int)b; *--n = Ne;  } ty = INT; }
    else if (tk == Lt)  { next(); expr(Shl); if (*n==Num && *b==Num) n[1] = b[1] <  n[1]; else { *--n = (int)b; *--n = Lt;  } ty = INT; }
    else if (tk == Gt)  { next(); expr(Shl); if (*n==Num && *b==Num) n[1] = b[1] >  n[1]; else { *--n = (int)b; *--n = Gt;  } ty = INT; }
    else if (tk == Le)  { next(); expr(Shl); if (*n==Num && *b==Num) n[1] = b[1] <= n[1]; else { *--n = (int)b; *--n = Le;  } ty = INT; }
    else if (tk == Ge)  { next(); expr(Shl); if (*n==Num && *b==Num) n[1] = b[1] >= n[1]; else { *--n = (int)b; *--n = Ge;  } ty = INT; }
    else if (tk == Shl) { next(); expr(Add); if (*n==Num && *b==Num) n[1] = b[1] << n[1]; else { *--n = (int)b; *--n = Shl; } ty = INT; }
    else if (tk == Shr) { next(); expr(Add); if (*n==Num && *b==Num) n[1] = b[1] >> n[1]; else { *--n = (int)b; *--n = Shr; } ty = INT; }
    else if (tk == Add) {
      next(); expr(Mul);
      if ((ty = t) > PTR) { if (*n == Num) n[1] = n[1] * sizeof(int); else { *--n = sizeof(int); *--n = Num; --n; *n = (int)(n+3); *--n = Mul; } }
      if (*n == Num && *b == Num) n[1] = b[1] + n[1]; else { *--n = (int)b; *--n = Add; }
    }
    else if (tk == Sub) {
      next(); expr(Mul);
      if ((ty = t) > PTR) { if (*n == Num) n[1] = n[1] * sizeof(int); else { *--n = sizeof(int); *--n = Num; --n; *n = (int)(n+3); *--n = Mul; } }
      if (*n == Num && *b == Num) n[1] = b[1] - n[1]; else { *--n = (int)b; *--n = Sub; }
    }
    else if (tk == Mul) { next(); expr(Inc); if (*n==Num && *b==Num) n[1] = b[1] * n[1]; else { *--n = (int)b; *--n = Mul; } ty = INT; }
    else if (tk == Div) { next(); expr(Inc); if (*n==Num && *b==Num) n[1] = b[1] / n[1]; else { *--n = (int)b; *--n = Div; } ty = INT; }
    else if (tk == Mod) { next(); expr(Inc); if (*n==Num && *b==Num) n[1] = b[1] % n[1]; else { *--n = (int)b; *--n = Mod; } ty = INT; }
    else if (tk == Inc || tk == Dec) { // post-increment/-decrement
      if (*n == Load) *n = tk; else { printf("%d: bad lvalue in post-increment/-decrement (node=%d)\n", line, *n); exit(-1); }
      *--n = (ty > PTR) ? sizeof(int) : sizeof(char); *--n = Num;
      *--n = (int)b; *--n = (tk == Inc) ? Sub : Add;
      next();
    }
    else if (tk == Bracket) {
      next(); expr(Assign);
      if (tk == ']') next(); else { printf("%d: close bracket expected (tk=%d)\n", line, tk); exit(-1); }
      if (t > PTR) { if (*n == Num) n[1] = n[1] * sizeof(int); else { *--n = sizeof(int); *--n = Num; --n; *n = (int)(n+3); *--n = Mul; } }
      else if (t < PTR) { printf("%d: pointer type expected (t=%d)\n", line, t); exit(-1); }
      if (*n == Num && *b == Num) n[1] = b[1] + n[1]; else { *--n = (int)b; *--n = Add; }
      *--n = ty = t - PTR; *--n = Load;
    }
    else { printf("%d: compiler error (tk=%d)\n", line, tk); exit(-1); }
  }
}

void stmt(void) {
  int *a, *b, *c;

  if (tk == If) {
    next();
    if (tk == '(') next(); else { printf("%d: open paren expected (tk=%d)\n", line, tk); exit(-1); }
    expr(Assign); a = n;
    if (tk == ')') next(); else { printf("%d: close paren expected (tk=%d)\n", line, tk); exit(-1); }
    stmt(); b = n;
    if (tk == Else) { next(); stmt(); c = n; } else c = 0;
    *--n = (int)c; *--n = (int)b; *--n = (int)a; *--n = Cond;
  }
  else if (tk == While) {
    next();
    if (tk == '(') next(); else { printf("%d: open paren expected (tk=%d)\n", line, tk); exit(-1); }
    expr(Assign); a = n;
    if (tk == ')') next(); else { printf("%d: close paren expected (tk=%d)\n", line, tk); exit(-1); }
    stmt();
    *--n = (int)a; *--n = While;
  }
  else if (tk == Return) {
    next();
    if (tk != ';') { expr(Assign); a = n; } else a = 0;
    if (tk == ';') next(); else { printf("%d: semicolon expected (tk=%d)\n", line, tk); exit(-1); }
    *--n = (int)a; *--n = Return;
  }
  else if (tk == '{') {
    next();
    *--n = ';';
    while (tk != '}') { a = n; stmt(); *--n = (int)a; *--n = '{'; }
    next();
  }
  else if (tk == ';') {
    next(); *--n = ';';
  }
  else {
    expr(Assign);
    if (tk == ';') next(); else { printf("%d: semicolon expected (tk=%d)\n", line, tk); exit(-1); }
  }
}

void gen(int *n) {
  int i, *b;

  i = *n;
  if (i == Num) { *++e = IMM; *++e = n[1]; }
  else if (i == Loc) { *++e = LEA; *++e = n[1]; }
  else if (i == Load) { gen(n + 2); *++e = (n[1] == CHAR) ? LC : LI; }
  else if (i == Assign) { gen((int *)n[2]); *++e = PUSH; gen(n+3); *++e = (n[1] == CHAR) ? SC : SI; }
  else if (i == Inc || i == Dec) {
    gen(n + 2);
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
  else if (i == Lor)  { gen((int *)n[1]); *++e = BNZ; b = ++e; gen(n + 2); *b = (int)(e + 1); }
  else if (i == Land) { gen((int *)n[1]); *++e = BZ;  b = ++e; gen(n + 2); *b = (int)(e + 1); }
  else if (i == Or)  { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = OR; }
  else if (i == Xor) { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = XOR; }
  else if (i == And) { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = AND; }
  else if (i == Eq)  { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = EQ; }
  else if (i == Ne)  { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = NE; }
  else if (i == Lt)  { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = LT; }
  else if (i == Gt)  { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = GT; }
  else if (i == Le)  { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = LE; }
  else if (i == Ge)  { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = GE; }
  else if (i == Shl) { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = SHL; }
  else if (i == Shr) { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = SHR; }
  else if (i == Add) { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = ADD; }
  else if (i == Sub) { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = SUB; }
  else if (i == Mul) { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = MUL; }
  else if (i == Div) { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = DIV; }
  else if (i == Mod) { gen((int *)n[1]); *++e = PUSH; gen(n + 2); *++e = MOD; }
  else if (i == Sys || i == Fun) {
    b = (int *)n[1];
    while (b) { gen(b+1); *++e = PUSH; b = (int *)*b; }
    if (i == Fun) *++e = JSR; *++e = n[2];
    if (n[3]) { *++e = ADJ; *++e = n[3]; }
  }
  else if (i == While) {
    *++e = JMP; b = ++e; gen(n + 2); *b = (int)(e + 1);
    gen((int *)n[1]);
    *++e = BNZ; *++e = (int)(b + 1);
  }
  else if (i == Return) { if (n[1]) gen((int *)n[1]); *++e = LEAVE; }
  else if (i == '{') { gen((int *)n[1]); gen(n + 2); }
  else if (i == Enter) { *++e = ENTER; *++e = n[1]; gen(n + 2); *++e = LEAVE; }
  else if (i != ';') { printf("%d: compiler error gen=%d\n", line, i); exit(-1); }
}

int main(int argc, char **argv) {
  int fd, bt, poolsz, *idmain, *ast;
  int *pc, *sp, *bp, a, cycle; // vm registers
  int i, *t; // temps

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c5 [-s] [-d] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256 * 1024; // arbitrary size
  if (!(sym = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; }
  if (!(le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; }
  if (!(data = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; }
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%d) stack area\n", poolsz); return -1; }
  if (!(ast = malloc(poolsz))) { printf("could not malloc(%d) abstract syntax tree area\n", poolsz); return -1; }
  ast = (int *)((int)ast + poolsz); // abstract syntax tree is most efficiently built as a stack

  memset(sym,  0, poolsz);
  memset(e,    0, poolsz);
  memset(data, 0, poolsz);

  ops = "IMM\0    LEA\0    JMP\0    JSR\0    BZ\0     BNZ\0    ENTER\0  ADJ\0    LEAVE\0  LI\0     LC\0     SI\0     SC\0     PUSH\0   "
        "OR\0     XOR\0    AND\0    EQ\0     NE\0     LT\0     GT\0     LE\0     GE\0     SHL\0    SHR\0    ADD\0    SUB\0    MUL\0    DIV\0    MOD\0    "
        "OPEN\0   READ\0   WRITE\0  CLOSE\0  PRINTF\0 MALLOC\0 FREE\0   MEMSET\0 MEMCPY\0 MEMCMP\0 EXIT\0   ";

  p = "char else enum if int return sizeof while "
      "open read write close printf malloc free memset memcpy memcmp exit "
      "void main";

  i = Char; while (i <= While) { next(); id[Tk] = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { next(); id[Class] = Sys; id[Type] = INT; id[Val] = i++; } // add library to symbol table
  next(); id[Tk] = Char; // handle void type
  next(); idmain = id; // keep track of main

  if (!(lp = p = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }
  if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }
  p[i] = 0;
  close(fd);

  // parse declarations
  line = 1;
  next();
  while (tk) {
    bt = INT; // basetype
    if (tk == Char) { next(); bt = CHAR; }
    else if (tk == Int) next();
    else if (tk == Enum) {
      next();
      if (tk != '{') next(); // skip enum name
      if (tk == '{') {
        next();
        i = 0;
        while (tk != '}') {
          if (tk != Id) { printf("%d: bad enum identifier (tk=%d)\n", line, tk); return -1; }
          next();
          if (tk == Assign) {
            next();
            n = ast; expr(Cond);
            if (*n != Num) { printf("%d: bad enum initializer (tk=%d)\n", line, tk); return -1; }
            i = n[1];
          }
          id[Class] = Num; id[Type] = INT; id[Val] = i++;
          if (tk == ',') next();
        }
        next();
      }
    }
    while (tk != ';') {
      ty = bt;
      while (tk == Mul) { next(); ty = ty + PTR; }
      if (tk != Id) { printf("%d: bad global declaration; identifier expected (tk=%d)\n", line, tk); return -1; }
      if (id[Class]) { printf("%d: duplicate global definition (class=%d)\n", line, id[Class]); return -1; }
      next();
      id[Type] = ty;
      if (tk == '(') { // function
        id[Class] = Fun;
        id[Val] = (int)(e + 1);
        next(); i = 2;
        while (tk != ')') { // parameter
          ty = INT;
          if (tk == Char) { next(); ty = CHAR; }
          else if (tk == Int) next();
          while (tk == Mul) { next(); ty = ty + PTR; }
          if (tk != Id) { printf("%d: bad parameter declaration; identifier expected (tk=%d)\n", line, tk); return -1; }
          if (id[Class] == Loc) { printf("%d: duplicate parameter definition\n", line); return -1; }
          id[HClass] = id[Class]; id[Class] = Loc;
          id[HType]  = id[Type];  id[Type] = ty;
          id[HVal]   = id[Val];   id[Val] = i++;
          next();
          if (tk == ',') next();
        }
        next();
        if (tk != '{') { printf("%d: bad function definition; '{' expected (tk=%d)\n", line, tk); return -1; }
        i = 0;
        next();
        while (tk == Int || tk == Char) { // locals
          bt = (tk == Int) ? INT : CHAR;
          next();
          while (tk != ';') {
            ty = bt;
            while (tk == Mul) { next(); ty = ty + PTR; }
            if (tk != Id) { printf("%d: bad local declaration; identifier expected (tk=%d)\n", line, tk); return -1; }
            if (id[Class] == Loc) { printf("%d: duplicate local definition\n", line); return -1; }
            id[HClass] = id[Class]; id[Class] = Loc;
            id[HType]  = id[Type];  id[Type] = ty;
            id[HVal]   = id[Val];   id[Val] = --i;
            next();
            if (tk == ',') next();
          }
          next();
        }
        n = ast;
        *--n = ';'; while (tk != '}') { t = n; stmt(); *--n = (int)t; *--n = '{'; }
        *--n = -i; *--n = Enter;
        gen(n);
        id = sym; // unwind symbol table locals
        while (id[Tk]) {
          if (id[Class] == Loc) {
            id[Class] = id[HClass];
            id[Type] = id[HType];
            id[Val] = id[HVal];
          }
          id = id + Idsz;
        }
        tk = ';'; // break inner while loop
      }
      else { // global
        id[Class] = Glo;
        id[Val] = (int)data;
        data = data + sizeof(int);
      }
      if (tk == ',') next();
    }
    next();
  }

  if (!(pc = (int *)idmain[Val])) { printf("main() not defined\n"); return -1; }
  if (src) return 0;

  // call exit if main returns
  *++e = PUSH; t = e;
  *++e = EXIT;
  // setup stack
  bp = sp = (int *)((int)sp + poolsz);
  *--sp = (int)argv;
  *--sp = argc;
  *--sp = (int)t;

  // run...
  a = cycle = 0;
  while (1) {
    i = *pc++; ++cycle;
    if (debug) {
      printf("%d> %s", cycle, &ops[i * 8]);
      if (i <= ADJ) printf(" %d (0x%08X)\n", *pc, *pc); else printf("\n");
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
    else if (i == SC)    a = *(char *)*sp++ = (char)a;                      // store char
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
    else if (i == PRINTF) a = printf((char *)*sp, sp[1], sp[2], sp[3], sp[4], sp[5]);
    else if (i == MALLOC) a = (int)malloc(*sp);
    else if (i == FREE)   free((void *)(a = *sp));
    else if (i == MEMSET) a = (int)memset((char *)*sp, sp[1], sp[2]);
    else if (i == MEMCPY) a = (int)memcpy((char *)*sp, (char *)sp[1], sp[2]);
    else if (i == MEMCMP) a = memcmp((char *)*sp, (char *)sp[1], sp[2]);
    else if (i == EXIT)   { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }
    else { printf("unknown instruction %d! cycle = %d\n", i, cycle); return -1; }
  }
  return -1;
}
