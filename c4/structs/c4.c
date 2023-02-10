// c4.c - C in four functions

// char, int, pointer types, structs and enums
// if, while, return and expression statements
// just enough features to allow self-compilation and a bit more

// Written by Robert Swierczek

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <io.h>
#include <fcntl.h>

char *p, *lp, // current position in source code
     *data,   // data/bss pointer
     *ops;    // opcodes

int *e, *le,  // current position in emitted code
    *tsize,   // array (indexed by type) of type sizes
    tnew,     // next available type
    tk,       // current token
    val,      // current token value
    ty,       // current expression type
    loc,      // local variable offset
    line,     // current line number
    src,      // print source and assembly flag
    debug;    // print executed instructions

// identifier
struct ident_s {
  int tk;
  int hash;
  char *name;
  int class;  // Num, Fun, Sys, Glo, Loc
  int type;
  int val;
  int stype;
  int hclass; // hidden class
  int htype;  // hidden type
  int hval;   // hidden value
} *id,  // currently parsed identifier
  *sym; // symbol table (simple list of identifiers)

struct member_s {
  struct ident_s *id;
  int offset;
  int type;
  struct member_s *next;
} **members; // array (indexed by type) of struct member lists

// tokens and classes (operators last and in precedence order)
enum Token { Num = 128, Fun, Sys, Glo, Loc, Id,
             Char, Else, Enum, If, Int, Return, Sizeof, Struct, While,
             Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Dot, Arrow, Bracket };

// opcodes
enum Opcode { IMM, LEA, JMP, JSR, BZ, BNZ, ENTER, ADJ, LEAVE, LI, LC, SI, SC, PUSH,
              OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD, 
              OPEN, READ, WRITE, CLOSE, PRINTF, MALLOC, FREE, MEMSET, MEMCPY, MEMCMP, EXIT };

// types
enum Type { CHAR, INT, PTR = 256, PTR2 = 512 };

void next() {
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
      while (id->tk) {
        if (tk == id->hash && !memcmp((char *)id->name, pp, p - pp)) { tk = id->tk; return; }
        id = id + Idsz;
      }
      id->name = (int)pp;
      id->hash = tk;
      tk = id->tk = Id;
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
    else if (tk == '-') { if (*p == '-') { ++p; tk = Dec; } else if (*p == '>') { ++p; tk = Arrow; } else tk = Sub; return; }   // operator --, -> or -
    else if (tk == '!') { if (*p == '=') { ++p; tk = Ne; } return; }                   // operator != or !
    else if (tk == '<') { if (*p == '=') { ++p; tk = Le; } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return; } // operator <= or << or <
    else if (tk == '>') { if (*p == '=') { ++p; tk = Ge; } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return; } // operator >= or >> or >
    else if (tk == '|') { if (*p == '|') { ++p; tk = Lor; } else tk = Or; return; }    // operator || or |
    else if (tk == '&') { if (*p == '&') { ++p; tk = Land; } else tk = And; return; }  // operator && or &
    else if (tk == '^') { tk = Xor; return; }     // operator ^
    else if (tk == '%') { tk = Mod; return; }     // operator %
    else if (tk == '*') { tk = Mul; return; }     // operator *
    else if (tk == '?') { tk = Cond; return; }    // operator ?
    else if (tk == '.') { tk = Dot; return; }     // operator .
    else if (tk == '[') { tk = Bracket; return; } // operator []
    else if (tk != ' ' && tk != '\t' && tk != '\v' && tk != '\f' && tk != '\r') return; // skip whitespaces
  }
}

void expr(int lev) {
  int t, *b, sz;
  struct ident_s *d;
  struct member_s *m;

  if (!tk) { printf("%d: unexpected eof in expression\n", line); exit(-1); }
  else if (tk == Num) { *++e = IMM; *++e = val; next(); ty = INT; } // number or character literal
  else if (tk == '"') { // string literal
    *++e = IMM; *++e = val; next();
    while (tk == '"') next();
    data = (char *)((int)data + sizeof(int) & -sizeof(int)); ty = PTR;
  }
  else if (tk == Sizeof) { // sizeof operator
    next(); if (tk == '(') next(); else { printf("%d: open paren expected in sizeof (tk=%d)\n", line, tk); exit(-1); }
    ty = INT; if (tk == Int) next(); else if (tk == Char) { next(); ty = CHAR; }
    while (tk == Mul) { next(); ty = ty + PTR; }
    if (tk == ')') next(); else { printf("%d: close paren expected in sizeof (tk=%d)\n", line, tk); exit(-1); }
    *++e = IMM; *++e = (ty == CHAR) ? sizeof(char) : sizeof(int);
    ty = INT;
  }
  else if (tk == Id) { // identifier
    d = id; next();
    if (tk == '(') { // function call
      next();
      t = 0;
      while (tk != ')') { expr(Assign); *++e = PUSH; ++t; if (tk == ',') next(); } // parameter
      next();
      if (d->class == Sys) *++e = d->val;
      else if (d->class == Fun) { *++e = JSR; *++e = d->val; }
      else { printf("%d: bad function call (class=%d)\n", line, d->class); exit(-1); }
      if (t) { *++e = ADJ; *++e = t; } // stack adjust after call
      ty = d->type;
    }
    else if (d->class == Num) { *++e = IMM; *++e = d->val; ty = INT; } // enum identifier
    else { // variable
      if (d->class == Loc) { *++e = LEA; *++e = loc - d->val; } // local
      else if (d->class == Glo) { *++e = IMM; *++e = d->val; }  // global
      else { printf("%d: undefined variable (class=%d)\n", line, d->class); exit(-1); }
      *++e = ((ty = d->type) == CHAR) ? LC : LI;
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
    *++e = (ty == CHAR) ? LC : LI;
  }
  else if (tk == And) { // unary address of operator
    next(); expr(Inc);
    if (*e == LC || *e == LI) --e; else { printf("%d: bad address-of (instruction=%d)\n", line, *e); exit(-1); }
    ty = ty + PTR;
  }
  else if (tk == '!') { next(); expr(Inc); *++e = PUSH; *++e = IMM; *++e =  0; *++e = EQ;  ty = INT; } // logical not operator
  else if (tk == '~') { next(); expr(Inc); *++e = PUSH; *++e = IMM; *++e = -1; *++e = XOR; ty = INT; } // binary not operator
  else if (tk == Add) { next(); expr(Inc); ty = INT; } // unary plus operator
  else if (tk == Sub) { // unary minus operator
    next(); *++e = IMM;
    if (tk == Num) { *++e = -val; next(); } else { *++e = 0; *++e = PUSH; expr(Inc); *++e = SUB; }
    ty = INT;
  }
  else if (tk == Inc || tk == Dec) { // pre-increment/-decrement
    t = tk; next(); expr(Inc);
    if (*e == LC) { *e = PUSH; *++e = LC; }
    else if (*e == LI) { *e = PUSH; *++e = LI; }
    else { printf("%d: bad lvalue in pre-increment/-decrement (instruction=%d)\n", line, *e); exit(-1); }
    *++e = PUSH;
    *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
    *++e = (t == Inc) ? ADD : SUB;
    *++e = (ty == CHAR) ? SC : SI;
  }
  else { printf("%d: bad expression (tk=%d)\n", line, tk); exit(-1); }

  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    t = ty;
    if (tk == Assign) {
      next();
      if (*e == LC || *e == LI) *e = PUSH; else { printf("%d: bad lvalue in assignment (instruction=%d)\n", line, *e); exit(-1); }
      expr(Assign); *++e = ((ty = t) == CHAR) ? SC : SI;
    }
    else if (tk == Cond) {
      next();
      *++e = BZ; b = ++e;
      expr(Assign);
      if (tk == ':') next(); else { printf("%d: conditional missing colon (tk=%d)\n", line, tk); exit(-1); }
      *b = (int)(e + 3); *++e = JMP; b = ++e;
      expr(Cond);
      *b = (int)(e + 1);
    }
    else if (tk == Lor)  { next(); *++e = BNZ; b = ++e; expr(Land); *b = (int)(e + 1); ty = INT; }
    else if (tk == Land) { next(); *++e = BZ;  b = ++e; expr(Or);   *b = (int)(e + 1); ty = INT; }
    else if (tk == Or)  { next(); *++e = PUSH; expr(Xor); *++e = OR;  ty = INT; }
    else if (tk == Xor) { next(); *++e = PUSH; expr(And); *++e = XOR; ty = INT; }
    else if (tk == And) { next(); *++e = PUSH; expr(Eq);  *++e = AND; ty = INT; }
    else if (tk == Eq)  { next(); *++e = PUSH; expr(Lt);  *++e = EQ;  ty = INT; }
    else if (tk == Ne)  { next(); *++e = PUSH; expr(Lt);  *++e = NE;  ty = INT; }
    else if (tk == Lt)  { next(); *++e = PUSH; expr(Shl); *++e = LT;  ty = INT; }
    else if (tk == Gt)  { next(); *++e = PUSH; expr(Shl); *++e = GT;  ty = INT; }
    else if (tk == Le)  { next(); *++e = PUSH; expr(Shl); *++e = LE;  ty = INT; }
    else if (tk == Ge)  { next(); *++e = PUSH; expr(Shl); *++e = GE;  ty = INT; }
    else if (tk == Shl) { next(); *++e = PUSH; expr(Add); *++e = SHL; ty = INT; }
    else if (tk == Shr) { next(); *++e = PUSH; expr(Add); *++e = SHR; ty = INT; }
    else if (tk == Add) {
      next(); *++e = PUSH; expr(Mul);
      if ((ty = t) > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof(int); *++e = MUL; } // add value to int pointer
      *++e = ADD;
    }
    else if (tk == Sub) {
      next(); *++e = PUSH; expr(Mul);
      if (t > PTR && t == ty) { *++e = SUB; *++e = PUSH; *++e = IMM; *++e = sizeof(int); *++e = DIV; ty = INT; } // subtract int pointer or pointer to pointer from int pointer or pointer to pointer
      else if ((ty = t) > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof(int); *++e = MUL; *++e = SUB; } // subtract value from int pointer
      else *++e = SUB; // subtract value from value or value from char pointer or char pointer from char pointer
    }
    else if (tk == Mul) { next(); *++e = PUSH; expr(Inc); *++e = MUL; ty = INT; }
    else if (tk == Div) { next(); *++e = PUSH; expr(Inc); *++e = DIV; ty = INT; }
    else if (tk == Mod) { next(); *++e = PUSH; expr(Inc); *++e = MOD; ty = INT; }
    else if (tk == Inc || tk == Dec) { // post-increment/-decrement
      if (*e == LC) { *e = PUSH; *++e = LC; }
      else if (*e == LI) { *e = PUSH; *++e = LI; }
      else { printf("%d: bad lvalue in post-increment/-decrement (instruction=%d)\n", line, *e); exit(-1); }
      *++e = PUSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
      *++e = (tk == Inc) ? ADD : SUB;
      *++e = (ty == CHAR) ? SC : SI;
      *++e = PUSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
      *++e = (tk == Inc) ? SUB : ADD;
      next();
    }
    else if (tk == Dot || tk == Arrow) {
      if (tk == Dot) { ty = ty + PTR; }
      if (ty <= PTR + INT || ty >= PTR2) { printf("%d: structure expected (ty=%d)\n", line, ty); exit(-1); }
      next();
      if (tk != Id) { printf("%d: structure member expected (tk=%d)\n", line, tk); exit(-1); }
      m = members[ty - PTR]; while (m && m->id != id) m = m->next;
      if (!m) { printf("%d: structure member not found\n", line); exit(-1); }
      if (m->offset) { *++e = PUSH; *++e = IMM; *++e = m->offset; *++e = ADD; }
      ty = m->type;
      if (ty <= INT || ty >= PTR) *++e = (ty == CHAR) ? LDB : LDI;
      next();
    }
    else if (tk == Bracket) {
      next(); *++e = PUSH; expr(Assign);
      if (tk == ']') next(); else { printf("%d: close bracket expected (tk=%d)\n", line, tk); exit(-1); }
      if (t > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  }
      else if (t < PTR) { printf("%d: pointer type expected (t=%d)\n", line, t); exit(-1); }
      *++e = ADD;
      *++e = ((ty = t - PTR) == CHAR) ? LC : LI;
    }
    else { printf("%d: compiler error (tk=%d)\n", line, tk); exit(-1); }
  }
}

void stmt() {
  int *a, *b;

  if (tk == If) {
    next();
    if (tk == '(') next(); else { printf("%d: open paren expected (tk=%d)\n", line, tk); exit(-1); }
    expr(Assign);
    if (tk == ')') next(); else { printf("%d: close paren expected (tk=%d)\n", line, tk); exit(-1); }
    *++e = BZ; b = ++e;
    stmt();
    if (tk == Else) {
      *b = (int)(e + 3); *++e = JMP; b = ++e;
      next();
      stmt();
    }
    *b = (int)(e + 1);
  }
  else if (tk == While) {
    next();
    a = e + 1;
    if (tk == '(') next(); else { printf("%d: open paren expected (tk=%d)\n", line, tk); exit(-1); }
    expr(Assign);
    if (tk == ')') next(); else { printf("%d: close paren expected (tk=%d)\n", line, tk); exit(-1); }
    *++e = BZ; b = ++e;
    stmt();
    *++e = JMP; *++e = (int)a;
    *b = (int)(e + 1);
  }
  else if (tk == Return) {
    next();
    if (tk != ';') expr(Assign);
    *++e = LEAVE;
    if (tk == ';') next(); else { printf("%d: semicolon expected (tk=%d)\n", line, tk); exit(-1); }
  }
  else if (tk == '{') {
    next();
    while (tk != '}') stmt();
    next();
  }
  else if (tk == ';') {
    next();
  }
  else {
    expr(Assign);
    if (tk == ';') next(); else { printf("%d: semicolon expected (tk=%d)\n", line, tk); exit(-1); }
  }
}

int main(int argc, char **argv) {
  int fd, bt, mbt, poolsz;
  struct ident_s *idmain, *d;
  struct member_s *m;
  int *pc, *sp, *bp, a, cycle; // vm registers
  int i, *t; // temps

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c4 [-s] [-d] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256 * 1024; // arbitrary size
  if (!(sym = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; }
  if (!(le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; }
  if (!(data = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; }
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%d) stack area\n", poolsz); return -1; }
  if (!(tsize = malloc(PTR * sizeof(int)))) { printf("could not malloc() tsize area\n"); return -1; }
  if (!(members = malloc(PTR * sizeof(struct member_s *)))) { printf("could not malloc() members area\n"); return -1; }

  memset(sym,  0, poolsz);
  memset(e,    0, poolsz);
  memset(data, 0, poolsz);
  memset(sp,   0, poolsz);
  memset(tsize,  0, PTR * sizeof(int));
  memset(members, 0, PTR * sizeof(struct member_s *));

  ops = "IMM\0    LEA\0    JMP\0    JSR\0    BZ\0     BNZ\0    ENTER\0  ADJ\0    LEAVE\0  LI\0     LC\0     SI\0     SC\0     PUSH\0   "
        "OR\0     XOR\0    AND\0    EQ\0     NE\0     LT\0     GT\0     LE\0     GE\0     SHL\0    SHR\0    ADD\0    SUB\0    MUL\0    DIV\0    MOD\0    "
        "OPEN\0   READ\0   WRITE\0  CLOSE\0  PRINTF\0 MALLOC\0 FREE\0   MEMSET\0 MEMCPY\0 MEMCMP\0 EXIT\0   ";

  p = "char else enum if int return sizeof struct while "
      "open read write close printf malloc free memset memcpy memcmp exit "
      "void main";
  i = Char; while (i <= While) { next(); id->tk = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { next(); id->class = Sys; id->type = INT; id->val = i++; } // add library to symbol table
  next(); id->tk = Char; // handle void type
  next(); idmain = id; // keep track of main

  if (!(lp = p = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }
  if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }
  p[i] = 0;
  close(fd);

  // add primitive types
  tsize[tnew++] = sizeof(char);
  tsize[tnew++] = sizeof(int);
  
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
            if (tk != Num) { printf("%d: bad enum initializer (tk=%d)\n", line, tk); return -1; }
            i = val;
            next();
          }
          id->class = Num; id->type = INT; id->val = i++;
          if (tk == ',') next();
        }
        next();
      }
    }
    else if (tk == Struct) {
      next();
      if (tk == Id) {
        if (!id->stype) id->stype = tnew++;
        bt = id->stype;
        next();
      } else { 
        bt = tnew++;
      }
      if (tk == '{') {
        next();
        if (members[bt]) { printf("%d: duplicate structure definition\n", line); return -1; }
        i = 0;
        while (tk != '}') {
          mbt = INT;
          if (tk == Int) next();
          else if (tk == Char) { next(); mbt = CHAR; }
          else if (tk == Struct) {
            next(); 
            if (tk != Id) { printf("%d: bad struct declaration\n", line); return -1; }
            mbt = id->stype;
            next();
            break;
          }
          else { printf("%d: bad struct member declaration\n", line); return -1; }
          while (tk != ';') {
            ty = mbt;
            while (tk == Mul) { next(); ty = ty + PTR; }
            if (tk != Id) { printf("%d: bad struct member definition\n", line); return -1; }
            m = malloc(sizeof(struct member_s));
            m->id = id;
            m->offset = i;
            m->type = ty;
            m->next = members[bt];
            members[bt] = m;
            i = i + (ty >= PTR ? sizeof(int) : tsize[ty]);
            i = (i + 3) & -4;
            next();
            if (tk == ',') next();
          }
          next();
        }
        next();
        tsize[bt] = i;
      }
      break;
    }
    while (tk != ';') {
      ty = bt;
      while (tk == Mul) { next(); ty = ty + PTR; }
      if (tk != Id) { printf("%d: bad global declaration; identifier expected (tk=%d)\n", line, tk); return -1; }
      if (id->class) { printf("%d: duplicate global definition (class=%d)\n", line, id->class); return -1; }
      next();
      id->type = ty;
      if (tk == '(') { // function
        id->class = Fun;
        id->val = (int)(e + 1);
        next(); i = 0;
        while (tk != ')') { // parameter
          ty = INT;
          if (tk == Char) { next(); ty = CHAR; }
          else if (tk == Int) next();
          else if (tk == Struct) {
            next(); 
            if (tk != Id) { printf("%d: bad struct declaration\n", line); return -1; }
            ty = id->stype;
            next();
          }
          else { printf("%d: bad parameter declaration\n", line); return -1; }
          while (tk == Mul) { next(); ty = ty + PTR; }
          if (tk != Id) { printf("%d: bad parameter declaration; identifier expected (tk=%d)\n", line, tk); return -1; }
          if (id->class == Loc) { printf("%d: duplicate parameter definition\n", line); return -1; }
          id->hclass = id->class; id->class = Loc;
          id->htype = id->type; id->type = ty;
          id->hval = id->val; id->val = i;
          i = i + (ty >= PTR ? 1 : (tsize[ty] + 3 >> 2));
          next();
          if (tk == ',') next();
        }
        next();
        if (tk != '{') { printf("%d: bad function definition; '{' expected (tk=%d)\n", line, tk); return -1; }
        loc = ++i;
        next();
        while (tk == Int || tk == Char || tk == Struct) { // locals
          bt = INT;
          if (tk == Char) { next(); bt = CHAR; }
          else if (tk == Int) next();
          else { // struct
            next(); 
            if (tk != Id) { printf("%d: bad struct declaration\n", line); return -1; }
            bt = id->stype;
            next(); 
          }
          while (tk != ';') {
            ty = bt;
            while (tk == Mul) { next(); ty = ty + PTR; }
            if (tk != Id) { printf("%d: bad local declaration; identifier expected (tk=%d)\n", line, tk); return -1; }
            if (id->class == Loc) { printf("%d: duplicate local definition\n", line); return -1; }
            i = i + (ty >= PTR ? 1 : (tysize[ty] + 3 >> 2));
            id->hclass = id->class; id->class = Loc;
            id->htype = id->type; id->type = ty;
            id->hval = id->val; id->val = i;
            next();
            if (tk == ',') next();
          }
          next();
        }
        *++e = ENTER; *++e = i - loc;
        while (tk != '}') stmt();
        *++e = LEAVE;
        id = sym; // unwind symbol table locals
        while (id->tk) {
          if (id->class == Loc) {
            id->class = id->hclass;
            id->type = id->htype;
            id->val = id->hval;
          }
          id = id + 1;
        }
        tk = ';'; // break inner while loop
      }
      else { // global
        id->class = Glo;
        id->val = (int)data;
        data = data + (ty >= PTR ? sizeof(int) : tysize[ty]);
      }
      if (tk == ',') next();
    }
    next();
  }

  if (!(pc = (int *)idmain->val)) { printf("main() not defined\n"); return -1; }
  if (src) return 0;

  // call exit if main returns
  *++e = PUSH; t = e;
  *++e = EXIT;
  // setup stack
  bp = sp = (int *)((int)sp + poolsz);
  *--sp = argc;
  *--sp = (int)argv;
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

    else if (i == OPEN)   a = open((char *)sp[1], *sp);
    else if (i == READ)   a = read(sp[2], (char *)sp[1], *sp);
    else if (i == WRITE)  a = write(sp[2], (char *)sp[1], *sp);
    else if (i == CLOSE)  a = close(*sp);
    else if (i == PRINTF) {
      if (*pc == ADJ) t = sp + pc[1]; else { printf("bad instruction sequence; expected ADJ after PRINTF (instruction=%d)\n", *pc); return -1; }
      a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]);
    }
    else if (i == MALLOC) a = (int)malloc(*sp);
    else if (i == FREE)   free((void *)(a = *sp));
    else if (i == MEMSET) a = (int)memset((char *)sp[2], sp[1], *sp);
    else if (i == MEMCPY) a = (int)memcpy((char *)sp[2], (char *)sp[1], *sp);
    else if (i == MEMCMP) a = memcmp((char *)sp[2], (char *)sp[1], *sp);
    else if (i == EXIT)   { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }
    else { printf("unknown instruction %d! cycle = %d\n", i, cycle); return -1; }
  }
  return -1;
}
