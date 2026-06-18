// c4.c - C in a few functions

// char, int, structs, pointer types and enums
// if, while, return and expression statements
// just enough features to allow self-compilation and a bit more
// no enum identifiers allowd
// no assignments in expressions allowed

// Based on c4.c - C in four functions
// Written by Robert Swierczek

// Compile with 'gcc [-Wall] -m32 -o c4+ c4+.c'

#include <unistd.h> // for read, write, close
#include <stdio.h>  // for printf, sprintf, scanf, sscanf
#include <stdlib.h> // for malloc, free
#include <memory.h> // for memset, memcmp, memcpy
#include <fcntl.h>  // for open

char *p, *lp, *tp, // current/line/token position in source code
     *d,      // data pointer
     *ops,    // opcodes
     *fn;     // filename

int *e, *le,  // current/line position in emitted code
    tk,       // current token
    tkval,    // current token value
    ty,       // current expression type
    tynew,    // next available type
    *tysz,    // array (indexed by type) of type sizes
    loc,      // local variable offset
    line,     // current line number
    src,      // print source and assembly flag
    dbg;      // print executed instructions

// identifier
struct ident_s {
  int tk;
  int hash;
  char *name;
  int len;
  int line;
  int class;  // Num, Fun, Sys, Global, Local
  int type;
  int val;
  int stype;  // struct type
  int sline;  // struct def line
  int hclass; // hidden class
  int htype;  // hidden type
  int hval;   // hidden value
  int hline;  // hidden line
} *id,  // currently parsed identifier
  *sym; // symbol table (simple list of identifiers)

struct member_s {
  struct ident_s *id;
  int offset;
  int type;
  struct member_s *next;
} **members; // array (indexed by type) of struct member lists

// tokens and classes (operators last and in precedence order)
enum {
  Num = 128, Fun, Sys, Global, Local, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, Struct, While,
  Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Dot, Arrow, Bracket
};

// opcodes (IMM...ADJ have parameter)
enum {
  IMM, LEA, JMP, JSR, BZ, BNZ, ENTER, ADJ, LEAVE, LI, LC, SI, SC, PUSH,
  OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
  OPEN, READ, WRITE, CLOSE, PRINTF, SCANF, FPRINTF, FSCANF, MALLOC, FREE, MEMSET, MEMCMP, MEMCPY, EXIT
};

// types
enum { CHAR, INT, PTR = 1000, PTR2 = 2000 };

// constants
enum { CODE_SZ = 1000000, DATA_SZ = 1000000, STACK_SZ = 1000000, SYM_SZ = 10000, SRC_SZ = 1000000 };

char *e2s(int x) {
  char *str;

  if (0 <= x && x < 32) {
    str = "0x00\0 0x01\0 0x02\0 0x03\0 0x04\0 0x05\0 0x06\0 0x07\0 0x08\0 '\\t'\0 '\\n'\0 '\\v'\0 '\\f'\0 '\\r'\0 0x0E\0 0x0F\0 "
          "0x10\0 0x11\0 0x12\0 0x13\0 0x14\0 0x15\0 0x16\0 0x17\0 0x18\0 0x19\0 0x1A\0 0x1B\0 0x1C\0 0x1D\0 0x1E\0 0x1F\0 ";
    return &str[x * 6];
  }
  if (32 <= x && x <= 126) { str = (char *)&tkval; memcpy(str, "' '", 4); str[1] = (char)x; return str; } // printable
  if (x == 127)  { return "DEL"; }
  if (Num <= x && x <= Id) {
    str = "num\0   fun\0   sys\0   global\0local\0 id\0     ";
    x = x - Num; return &str[x * 7];
  }
  if (Char <= x && x <= While) {
    str = "\"char\"\0   \"else\"\0   \"enum\"\0   \"if\"\0     \"int\"\0    \"return\"\0 \"sizeof\"\0 \"struct\"\0 \"while\"\0  ";
    x = x - Char; return &str[x * 10];
  }
  if (Assign <= x && x <= Bracket) {
    str = "'='\0  '?'\0  \"||\"\0 \"&&\"\0 '|'\0  '^'\0  '&'\0  \"==\"\0 \"!=\"\0 '<'\0  '>'\0  \"<=\"\0 \">=\"\0 \"<<\"\0 \">>\"\0 '+'\0  '-'\0  '*'\0  '/'\0  '%'\0  \"++\"\0 \"--\"\0 '.'\0  \"->\"\0 '['\0  ";
    x = x - Assign; return &str[x * 6];
  }
  return "<unknown enum>";
}

void next() {
  char *pp; // previous position

  while (*p != 0) {
    tp = p; tk = *p++;
    if (tk == '\n') { // end of line
      if (src) {
        printf("%d: %.*s", line, p - lp, lp);
        while (le < e) { printf("  %s", &ops[*++le * 8]); if (*le <= ADJ) { ++le; printf(" %d (0x%08X)\n", *le, *le); } else printf("\n"); }
      }
      lp = p; ++line;
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
        if (tk == id->hash && !memcmp(id->name, pp, p - pp)) { tk = id->tk; return; }
        ++id;
      }
      if (id >= sym + SYM_SZ) { printf("%s:%d:%d: FATAL: too many syms\n", fn, line, tp - lp + 1); exit(-1); }
      id->name = pp;
      id->len = p - pp;
      id->hash = tk;
      tk = id->tk = Id;
      return;
    }
    else if (tk >= '0' && tk <= '9') { // number
      tkval = tk - '0';
      if (tkval) { while (*p >= '0' && *p <= '9') tkval = tkval * 10 + *p++ - '0'; } // decimal
      else if (*p == 'x' || *p == 'X') { // hexadecimal
        tk = *++p;
        while (tk && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F'))) {
          tkval = tkval * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
          tk = *++p;
        }
      }
      else { while (*p >= '0' && *p <= '7') tkval = tkval * 8 + *p++ - '0'; } // octal
      tk = Num;
      return;
    }
    else if (tk == '/') { // line comment or operator /
      if (*p == '/') { // line comment
        ++p;
        while (*p != 0 && *p != '\n') ++p;
      }
      else { // operator /
        tk = Div;
        return;
      }
    }
    else if (tk == '\'' || tk == '"') { // string or character literal
      pp = d;
      while (*p != 0 && *p != tk) {
        tkval = *p++;
        if (tkval == '\\') {
          tkval = *p++;
          if      (tkval == '0') tkval = '\0';
          else if (tkval == 't') tkval = '\t';
          else if (tkval == 'v') tkval = '\v';
          else if (tkval == 'f') tkval = '\f';
          else if (tkval == 'r') tkval = '\r';
          else if (tkval == 'n') tkval = '\n';
        }
        if (tk == '"') *d++ = tkval;
      }
      ++p; // skip '\'' or '"'
      if (tk == '"') tkval = (int)pp;
      else tk = Num;
      return;
    }
    else if (tk == '=') { if (*p == '=') { ++p; tk = Eq; } else tk = Assign; return; } // operator == or =
    else if (tk == '+') { if (*p == '+') { ++p; tk = Inc; } else tk = Add; return; } // operator ++ or +
    else if (tk == '-') { if (*p == '-') { ++p; tk = Dec; } else if (*p == '>') { ++p; tk = Arrow; } else tk = Sub; return; } // operator --, -> or -
    else if (tk == '!') { if (*p == '=') { ++p; tk = Ne; } return; } // operator != or !
    else if (tk == '<') { if (*p == '=') { ++p; tk = Le; } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return; } // operator <= or << or <
    else if (tk == '>') { if (*p == '=') { ++p; tk = Ge; } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return; } // operator >= or >> or >
    else if (tk == '|') { if (*p == '|') { ++p; tk = Lor; } else tk = Or; return; } // operator || or |
    else if (tk == '&') { if (*p == '&') { ++p; tk = Land; } else tk = And; return; } // operator && or &
    else if (tk == '^') { tk = Xor; return; } // operator ^
    else if (tk == '%') { tk = Mod; return; } // operator %
    else if (tk == '*') { tk = Mul; return; } // operator *
    else if (tk == '[') { tk = Bracket; return; } // operator []
    else if (tk == '?') { tk = Cond; return; } // operator ?
    else if (tk == '.') { tk = Dot; return; } // operator .
    else if (tk != ' ' && tk != '\t' && tk != '\v' && tk != '\f' && tk != '\r') return; // skip whitespaces
  }
  tk = 0;
}

int match(int t) {
  if (t == tk) { next(); return 1; }
  return 0;
}

void expr(int lev) {
  int i, *pp, sz;
  struct ident_s *s; // symbol
  struct member_s *m; // member

  if (!tk) { printf("%s:%d:%d: bad expr: unexpected eof\n", fn, line, tp - lp + 1); exit(-1); }
  else if (tk == Num) { // number or character literal
    *++e = IMM; *++e = tkval;
    next();
    ty = INT;
  }
  else if (tk == '"') { // string literal
    *++e = IMM; *++e = tkval;
    next();
    while (match('"')) ; // concatenate string literals
    d = (char *)(((int)d + sizeof (int)) & -sizeof (int)); // align
    ty = PTR;
  }
  else if (match(Sizeof)) { // sizeof expr
    if (!match('(')) { printf("%s:%d:%d: bad sizeof expr: '(' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    if (match(Char)) ty = CHAR;
    else if (match(Int)) ty = INT;
    else if (match(Struct)) {
      if (tk != Id) { printf("%s:%d:%d: bad sizeof expr: struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      if (!id->stype) { printf("%s:%d:%d: bad sizeof expr: struct '%.*s' not def'ed\n", fn, line, tp - lp + 1, id->len, id->name); exit(-1); }
      ty = id->stype;
      next();
    }
    else { printf("%s:%d:%d: bad sizeof expr: type expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    while (match(Mul)) ty = ty + PTR;
    if (!match(')')) { printf("%s:%d:%d: bad sizeof expr: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    *++e = IMM; *++e = ty >= PTR ? sizeof (int) : tysz[ty];
    ty = INT;
  }
  else if (tk == Id) { // identifier
    s = id;
    next();
    if (match('(')) { // fun call
      i = 0;
      while (!match(')')) { // fun args
        expr(Cond);
        *++e = PUSH; ++i;
        if (!match(',') && tk != ')') { printf("%s:%d:%d: bad fun call: ',' or ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      }
      if (s->class == Sys) *++e = s->val;
      else if (s->class == Fun) { *++e = JSR; *++e = s->val; }
      else if (s->class == 0) { printf("%s:%d:%d: bad fun arg: '%.*s' not def'ed\n", fn, line, tp - lp + 1, s->len, s->name); exit(-1); }
      else { printf("%s:%d:%d: bad fun arg: unexpected class '%s' of id '%.*s'; def'ed in line %d\n", fn, line, tp - lp + 1, e2s(s->class), s->len, s->name, s->line); exit(-1); }
      if (i) { *++e = ADJ; *++e = i; } // stack adjust after fun call
      ty = s->type;
    }
    else if (s->class == Num) { *++e = IMM; *++e = s->val; ty = INT; } // enum identifier
    else { // var
      if (s->class == Local) { *++e = LEA; *++e = loc - s->val; } // local var
      else if (s->class == Global) { *++e = IMM; *++e = s->val; }  // global var
      else if (s->class == 0) { printf("%s:%d:%d: bad var expr: '%.*s' not def'ed\n", fn, line, tp - lp + 1, s->len, s->name); exit(-1); }
      else { printf("%s:%d:%d: bad var expr: unexpected class '%s' of id '%.*s'; def'ed in line %d\n", fn, line, tp - lp + 1, e2s(s->class), s->len, s->name, s->line); exit(-1); }
      ty = s->type;
      if (ty <= INT || ty >= PTR) *++e = (ty == CHAR) ? LC : LI;
    }
  }
  else if (match('(')) { // typecast or group expr
    if (tk == Int || tk == Char || tk == Struct) { // typecast expr
      if (match(Char)) i = CHAR;
      else if (match(Int)) i = INT;
      else { // struct
        next();
        if (tk != Id) { printf("%s:%d:%d: bad typecast expr: struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
        if (!id->stype) { printf("%s:%d:%d: bad typecast expr: struct '%.*s' not def'ed\n", fn, line, tp - lp + 1, id->len, id->name); exit(-1); }
        i = id->stype;
        next();
      }
      while (match(Mul)) i = i + PTR;
      if (!match(')')) { printf("%s:%d:%d: bad typecast expr: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      expr(Inc);
      ty = i;
    }
    else { // group expr
      expr(Cond);
      if (!match(')')) { printf("%s:%d:%d: bad expr: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    }
  }
  else if (match(Mul)) { // unary dereference expr
    expr(Inc);
    if (ty >= PTR) ty = ty - PTR;
    else { printf("%s:%d:%d: bad dereference expr: pointer type expected; got #%d\n", fn, line, tp - lp + 1, ty); exit(-1); }
    if (ty <= INT || ty >= PTR) *++e = (ty == CHAR) ? LC : LI;
  }
  else if (match(And)) { // unary address-of expr
    expr(Inc);
    if (*e == LC || *e == LI) --e;
    ty = ty + PTR;
  }
  else if (match('!')) { expr(Inc); *++e = PUSH; *++e = IMM; *++e = 0; *++e = EQ; ty = INT; } // logical not expr
  else if (match('~')) { expr(Inc); *++e = PUSH; *++e = IMM; *++e = -1; *++e = XOR; ty = INT; } // binary not expr
  else if (match(Add)) { expr(Inc); ty = INT; } // unary plus expr
  else if (match(Sub)) { // unary minus expr
    *++e = IMM;
    if (tk == Num) { *++e = -tkval; next(); }
    else { *++e = 0; *++e = PUSH; expr(Inc); *++e = SUB; }
    ty = INT;
  }
  else if (tk == Inc || tk == Dec) { // pre-incr/-dec expr
    i = tk;
    next();
    expr(Inc);
    if (*e == LC) { *e = PUSH; *++e = LC; }
    else if (*e == LI) { *e = PUSH; *++e = LI; }
    else { printf("%s:%d:%d: bad pre-incr/-dec expr: unexpected opcode %s\n", fn, line, tp - lp + 1, &ops[*e * 8]); exit(-1); }
    *++e = PUSH;
    *++e = IMM; *++e = ty >= PTR2 ? sizeof (int) : (ty >= PTR) ? tysz[ty - PTR] : 1;
    *++e = (i == Inc) ? ADD : SUB;
    *++e = (ty == CHAR) ? SC : SI;
  }
  else { printf("%s:%d:%d: bad unary/prefix expr: got unexpected token %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }

  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    i = ty;
    if (match(Assign)) { // assignment expr
      if (*e == LC || *e == LI) *e = PUSH;
      else { printf("%s:%d:%d: bad assign expr: unexpected opcode %s\n", fn, line, tp - lp + 1, &ops[*e * 8]); exit(-1); }
      expr(Assign);
      *++e = (i == CHAR) ? SC : SI; ty = i;
    }
    else if (match(Cond)) { // conditional expr
      *++e = BZ; pp = ++e;
      expr(Cond);
      if (!match(':')) { printf("%s:%d:%d: bad cond expr: ':' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      *pp = (int)(e + 3); *++e = JMP; pp = ++e;
      expr(Cond);
      *pp = (int)(e + 1);
    }
    else if (match(Lor))  { *++e = BNZ; pp = ++e; expr(Land); *pp = (int)(e + 1); ty = INT; } // logical or expr
    else if (match(Land)) { *++e = BZ;  pp = ++e; expr(Or);   *pp = (int)(e + 1); ty = INT; } // logical and expr
    else if (match(Or))  { *++e = PUSH; expr(Xor); *++e = OR;  ty = INT; } // binary or expr
    else if (match(Xor)) { *++e = PUSH; expr(And); *++e = XOR; ty = INT; } // binary xor expr
    else if (match(And)) { *++e = PUSH; expr(Eq);  *++e = AND; ty = INT; } // binary and expr
    else if (match(Eq))  { *++e = PUSH; expr(Lt);  *++e = EQ;  ty = INT; } // equal expr
    else if (match(Ne))  { *++e = PUSH; expr(Lt);  *++e = NE;  ty = INT; } // not-equal expr
    else if (match(Lt))  { *++e = PUSH; expr(Shl); *++e = LT;  ty = INT; } // less-than expr
    else if (match(Gt))  { *++e = PUSH; expr(Shl); *++e = GT;  ty = INT; } // greater-than expr
    else if (match(Le))  { *++e = PUSH; expr(Shl); *++e = LE;  ty = INT; } // less-or-equal expr
    else if (match(Ge))  { *++e = PUSH; expr(Shl); *++e = GE;  ty = INT; } // greater-or-equal expr
    else if (match(Shl)) { *++e = PUSH; expr(Add); *++e = SHL; ty = INT; } // shift-left expr
    else if (match(Shr)) { *++e = PUSH; expr(Add); *++e = SHR; ty = INT; } // shift-right expr
    else if (match(Add)) { // addition: num + num, ptr + num, num + ptr OK; ptr + ptr NOT ALLOWED
      *++e = PUSH;
      expr(Mul);
      if (i >= PTR && ty >= PTR) { printf("%s:%d:%d: bad add expr: ptr + ptr not allowed\n", fn, line, tp - lp + 1); exit(-1); }
      sz = i >= PTR2 ? sizeof (int) : i >= PTR ? tysz[i - PTR] : 1;
      if (sz > 1) { *++e = PUSH; *++e = IMM; *++e = sz; *++e = MUL;  }
      *++e = ADD;
      ty = i;
    }
    else if (match(Sub)) { // subtraction: num - num, ptr - num, ptr - ptr OK; num - ptr NOT ALLOWED
      *++e = PUSH;
      expr(Mul);
      if (i < PTR && ty >= PTR) { printf("%s:%d:%d: bad sub expr: num - ptr not allowed\n", fn, line, tp - lp + 1); exit(-1); }
      sz = i >= PTR2 ? sizeof (int) : i >= PTR ? tysz[i - PTR] : 1;
      if (i == ty && sz > 1) { *++e = SUB; *++e = PUSH; *++e = IMM; *++e = sz; *++e = DIV; ty = INT; }
      else if (sz > 1) { *++e = PUSH; *++e = IMM; *++e = sz; *++e = MUL; *++e = SUB; }
      else *++e = SUB;
      ty = i;
    }
    else if (match(Mul)) { *++e = PUSH; expr(Inc); *++e = MUL; ty = INT; } // multiplication
    else if (match(Div)) { *++e = PUSH; expr(Inc); *++e = DIV; ty = INT; } // division
    else if (match(Mod)) { *++e = PUSH; expr(Inc); *++e = MOD; ty = INT; } // modulo
    else if (tk == Inc || tk == Dec) { // post-inc/-dec
      if (*e == LC) { *e = PUSH; *++e = LC; }
      else if (*e == LI) { *e = PUSH; *++e = LI; }
      else { printf("%s:%d:%d: bad post-inc/-dec expr: unexpected opcode %s\n", fn, line, tp - lp + 1, &ops[*e * 8]); exit(-1); }
      sz = ty >= PTR2 ? sizeof (int) : ty >= PTR ? tysz[ty - PTR] : 1;
      *++e = PUSH; *++e = IMM; *++e = sz;
      *++e = (tk == Inc) ? ADD : SUB;
      *++e = (ty == CHAR) ? SC : SI;
      *++e = PUSH; *++e = IMM; *++e = sz;
      *++e = (tk == Inc) ? SUB : ADD;
      next();
    }
    else if (tk == Dot || tk == Arrow) { // struct member expr
      if (tk == Dot) ty = ty + PTR;
      next();
      if (ty <= PTR + INT || ty >= PTR2) { printf("%s:%d:%d: bad struct member expr: expected lhs expr type #%d...#%d; got #%d\n", fn, line, tp - lp + 1, PTR + INT + 1, PTR2 - 1, ty); exit(-1); }
      if (tk != Id) { printf("%s:%d:%d: bad struct member expr: member id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      m = members[ty - PTR]; while (m && m->id != id) m = m->next; // search struct member
      if (!m) { printf("%s:%d:%d: struct member not def'ed\n", fn, line, tp - lp + 1); exit(-1); }
      if (m->offset) { *++e = PUSH; *++e = IMM; *++e = m->offset; *++e = ADD; }
      ty = m->type;
      if (ty <= INT || ty >= PTR) *++e = (ty == CHAR) ? LC : LI;
      next();
    }
    else if (match(Bracket)) { // array element expr
      *++e = PUSH;
      expr(Cond);
      if (!match(']')) { printf("%s:%d:%d: bad array element expr: ']' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      if (i < PTR) { printf("%s:%d:%d: bad array element expr: lhs pointer type expected; got type #%d\n", fn, line, tp - lp + 1, i); exit(-1); }
      i = i - PTR; sz = i >= PTR ? sizeof (int) : tysz[i];
      if (sz > 1) { *++e = PUSH; *++e = IMM; *++e = sz; *++e = MUL;  }
      *++e = ADD;
      ty = i;
      if (ty <= INT || ty >= PTR) *++e = (ty == CHAR) ? LC : LI;
    }
    else { printf("%s:%d:%d: bad binary/postfix expr: got unexpected token %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
  }
}

void stmt() {
  int *a, *b;

  if (match(If)) {
    if (!match('(')) { printf("%s:%d:%d: bad if stmt: '(' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    expr(Cond);
    if (!match(')')) { printf("%s:%d:%d: bad if stmt: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    *++e = BZ; b = ++e;
    stmt();
    if (match(Else)) {
      *b = (int)(e + 3); *++e = JMP; b = ++e;
      stmt();
    }
    *b = (int)(e + 1);
  }
  else if (match(While)) {
    a = e + 1;
    if (!match('(')) { printf("%s:%d:%d: bad while stmt: '(' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    expr(Cond);
    if (!match(')')) { printf("%s:%d:%d: bad while stmt: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    *++e = BZ; b = ++e;
    stmt();
    *++e = JMP; *++e = (int)a;
    *b = (int)(e + 1);
  }
  else if (match(Return)) {
    if (tk != ';') expr(Cond);
    if (!match(';')) { printf("%s:%d:%d: bad return stmt: ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    *++e = LEAVE;
  }
  else if (match('{')) {
    while (!match('}')) stmt();
  }
  else if (match(';')) {
    // empty statement
  }
  else {
    expr(Assign);
    if (!match(';')) { printf("%s:%d:%d: bad assign stmt: ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
  }
}

int parse_enum() {
  int i;
  struct ident_s *enum_id;

  // no enum identifier allowed!
  if (!match('{')) { printf("%s:%d:%d: bad enum def: '{' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
  i = 0;
  while (i == 0 || !match('}')) { // at least one enum id required
    if (tk != Id) { printf("%s:%d:%d: bad enum def: enum id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    if (id->class) { printf("%s:%d:%d: duplicate enum id '%.*s'; already def'ed in line %d\n", fn, line, tp - lp + 1, id->len, id->name, id->line); exit(-1); }
    enum_id = id;
    next();
    if (match(Assign)) {
      if (tk != Num) { printf("%s:%d:%d: bad enum def: num initializer expected after '='; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      i = tkval;
      next();
    }
    enum_id->line = line;
    enum_id->class = Num;
    enum_id->type = INT;
    enum_id->val = i++;
    if (!match(',') && tk != '}') { printf("%s:%d:%d: bad enum def: ',' or '}' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
  }
  return INT;
}

int parse_struct() {
  int bt, i;
  struct member_s *m;

  if (tk == Id) { // struct id
    if (!id->stype) { id->stype = tynew++; id->sline = line; }
    bt = id->stype;
    //printf("DBG struct decl: '%.*s' ty:#%d\n", id->len, id->name, id->stype);
    next();
  } else { // anonymous struct
    bt = tynew++;
    //printf("DBG anonymous struct decl: ty:#%d\n", bt);
  }
  if (tynew >= PTR) { printf("%s:%d:%d: FATAL: too many structs\n", fn, line, tp - lp + 1); exit(-1); }
  if (tk == '{') { // struct def
    if (members[bt]) { printf("%s:%d:%d: duplicate struct '%.*s'; already def'ed in line %d\n", fn, line, tp - lp + 1, id->len, id->name, id->sline); exit(-1); }
    next();
    i = 0;
    while (!i || !match('}')) { // struct member def
      // member base type
      if (match(Char)) mbt = CHAR;
      else if (match(Int)) mbt = INT;
      else if (match(Struct)) {
        if (tk != Id) { printf("%s:%d:%d: bad struct member def: struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
        if (!id->stype) { printf("%s:%d:%d: bad struct member def: struct '%.*s' not def'ed\n", fn, line, tp - lp + 1, id->len, id->name); exit(-1); }
        mbt = id->stype;
        next();
      }
      else { printf("%s:%d:%d: bad struct member def: member type expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }

      m = 0;
      while (!m || !match(';')) { // at least one struct member required
        ty = mbt;
        while (match(Mul)) ty = ty + PTR;
        if (tk != Id) { printf("%s:%d:%d: bad struct member def: member id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
        // TODO: struct member already defined?
        if (ty > CHAR) i = (i + sizeof (int) - 1) & -sizeof (int); // align non-CHAR members
        m = malloc(sizeof (struct member_s));
        m->id = id;
        m->offset = i;
        m->type = ty;
        m->next = members[bt];
        members[bt] = m;
        i = i + (ty >= PTR ? sizeof (int) : tysz[ty]);
        next();
        if (!match(',') && tk != ';') { printf("%s:%d:%d: bad struct member def: ',' or ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      }
    }
    tysz[bt] = (i + sizeof (int) - 1) & -sizeof (int);
  }
  return bt;
}

void parse_func(struct ident_s *func_id) {
  int i, bt;

  func_id->class = Fun;
  func_id->val = (int)(e + 1);
  //printf("DBG fun def: '%.*s' ty:#%d\n", id->len, id->name, id->type);
  next();

  // func args
  i = 0;
  while (!match(')')) {
    if (match(Char)) ty = CHAR;
    else if (match(Int)) ty = INT;
    else if (match(Struct)) {
      if (tk != Id) { printf("%s:%d:%d: bad arg def: struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
      if (!id->stype) { printf("%s:%d:%d: bad arg def: struct '%.*s' not def'ed\n", fn, line, tp - lp + 1, id->len, id->name); return -1; }
      ty = id->stype;
      next();
    }
    else { printf("%s:%d:%d: bad arg def: arg type expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }

    while (match(Mul)) ty = ty + PTR;
    if (tk != Id) { printf("%s:%d:%d: bad arg def: arg id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
    if (id->class == Local) { printf("%s:%d:%d: duplicate arg '%.*s': already def'ed in line %d\n", fn, line, tp - lp + 1, id->len, id->name, id->line); return -1; }
    if (ty > INT && ty < PTR) i = i + tysz[ty]/sizeof (int) - 1; // adjust for structs
    id->hclass = id->class; id->class = Local;
    id->htype = id->type; id->type = ty;
    id->hval = id->val; id->val = i++;
    id->hline = id->line; id->line = line;
    //printf("DBG arg def: '%.*s' ty:#%d sz:%d @%d\n", id->len, id->name, id->type, id->type < PTR ? tysz[id->type] : sizeof (int), id->val);
    next();
    if (!match(',') && tk != ')') { printf("%s:%d:%d: bad arg def: ',' or ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
  }
  if (!match('{')) { printf("%s:%d:%d: bad func def: '{' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }

  // local vars
  loc = ++i;
  while (tk == Char || tk == Int || tk == Struct) {
    if (match(Char)) bt = CHAR;
    else if (match(Int)) bt = INT;
    else { // struct
      next();
      if (tk != Id) { printf("%s:%d:%d: bad local def; struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
      if (!id->stype) { printf("%s:%d:%d: bad local def: struct '%.*s' not def'ed\n", fn, line, tp - lp + 1, id->len, id->name); return -1; }
      bt = id->stype;
      next();
    }

    ty = 0;
    while (!ty || !match(';')) { // at least one local var required
      ty = bt;
      while (match(Mul)) ty = ty + PTR;

      if (tk != Id) { printf("%s:%d:%d: bad local def; local id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
      if (id->class == Local) { printf("%s:%d:%d: duplicate local '%.*s': already def'ed in line %d\n", fn, line, tp - lp + 1, id->len, id->name, id->line); return -1; }
      if (ty > INT && ty < PTR) i = i + tysz[ty]/sizeof (int) - 1; // adjust for structs
      id->hclass = id->class; id->class = Local;
      id->htype = id->type; id->type = ty;
      id->hval = id->val; id->val = ++i;
      id->hline = id->line; id->line = line;
      //printf("DBG local def: '%.*s' ty:#%d sz:%d @%d\n", id->len, id->name, id->type, id->type < PTR ? tysz[id->type] : sizeof (int), id->val);
      next();
      if (!match(',') && tk != ';') { printf("%s:%d:%d: bad local def: ',' or ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
    }
  }

  // func body
  *++e = ENTER; *++e = i - loc; // TODO: pp = ++e;
  while (tk != '}') stmt();
  *++e = LEAVE; // TODO: *pp = loc_ofs_max

  // unwind symbol table locals
  id = sym;
  while (id->tk) {
    if (id->class == Local) {
      id->class = id->hclass;
      id->type = id->htype;
      id->val = id->hval;
      id->line = id->hline;
    }
    ++id;
  }
}

int main(int argc, char **argv) {
  int *code, *stack;
  char *data;
  int fd, bt;
  struct ident_s *idmain;
  int *pc, *sp, *bp, a, cycle; // vm registers
  int i, *pp; // temps

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { dbg = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c4 [-s] [-d] [-v] file ...\n"); return -1; }

  fn = *argv;
  fd = open(fn, 0); if (fd < 0) { printf("could not open(%s)\n", *argv); return -1; }

  le = e = code = malloc(CODE_SZ * sizeof (int)); if (!code) { printf("could not alloc code segment\n"); return -1; }
  d = data = malloc(DATA_SZ); if (!data) { printf("could not alloc data segment\n"); return -1; }
  sp = stack = malloc(STACK_SZ * sizeof (int)); if (!stack) { printf("could not alloc stack segment\n"); return -1; }
  sym = malloc(SYM_SZ * sizeof (struct ident_s)); if (!sym) { printf("could not alloc symbol table\n"); return -1; }
  tysz = malloc(PTR * sizeof (int)); if (!tysz) { printf("could not alloc type size array\n"); return -1; }
  members = malloc(PTR * sizeof (struct member_s *)); if (!members) { printf("could not alloc members array\n"); return -1; }

  memset(code,  0, CODE_SZ * sizeof (int));
  memset(data,  0, DATA_SZ);
  memset(stack, 0, STACK_SZ * sizeof (int));
  memset(sym,   0, SYM_SZ * sizeof (struct ident_s));
  memset(tysz,   0, PTR * sizeof (int));
  memset(members, 0, PTR * sizeof (struct member_s *));

  ops = "IMM\0    LEA\0    JMP\0    JSR\0    BZ\0     BNZ\0    ENTER\0  ADJ\0    LEAVE\0  LI\0     LC\0     SI\0     SC\0     PUSH\0   "
        "OR\0     XOR\0    AND\0    EQ\0     NE\0     LT\0     GT\0     LE\0     GE\0     SHL\0    SHR\0    ADD\0    SUB\0    MUL\0    DIV\0    MOD\0    "
        "OPEN\0   READ\0   WRITE\0  CLOSE\0  PRINTF\0 SCANF\0  FPRINTF\0FSCANF\0 MALLOC\0 FREE\0   MEMSET\0 MEMCMP\0 MEMCPY\0 EXIT\0   ";

  p = "char else enum if int return sizeof struct while "
      "open read write close printf scanf fprintf fscanf malloc free memset memcmp memcpy exit "
      "void main";
  i = Char; while (i <= While) { next(); id->tk = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { next(); id->class = Sys; id->type = INT; id->val = i++; } // add library to symbol table
  next(); id->tk = Char; // handle void type
  next(); idmain = id; // keep track of main

  lp = p = malloc(SRC_SZ); if (!p) { printf("could not alloc source buffer\n"); return -1; }
  i = read(fd, p, SRC_SZ-1); if (i <= 0) { printf("%s: read() returned %d\n", fn, i); return -1; }
  p[i] = 0;
  close(fd);

  // add primitive types
  tysz[tynew++] = sizeof (char);
  tysz[tynew++] = sizeof (int);

  // parse defs
  line = 1;
  next();
  while (tk) {
    // basetype
    if (match(Char)) bt = CHAR;
    else if (match(Int)) bt = INT;
    else if (match(Enum)) bt = parse_enum();
    else if (match(Struct)) bt = parse_struct();
    else { printf("%s:%d:%d: bad def: type expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }

    while (!match(';')) {
      ty = bt;
      while (match(Mul)) ty = ty + PTR; // pointer declarator
      if (ty > INT && ty < PTR && !members[ty]) { printf("%s:%d:%d: bad def: struct not def'ed, only decl'ed\n", fn, line, tp - lp + 1); return -1; }
      if (tk != Id) { printf("%s:%d:%d: bad def: global id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
      if (id->class) { printf("%s:%d:%d: duplicate global '%.*s'; already def'ed as %s in line %d\n", fn, line, tp - lp + 1, id->len, id->name, e2s(id->class), id->line); return -1; }
      id->type = ty;
      id->line = line;
      next();
      if (tk == '(') { // func def
        parse_func(id);
        tk = ';'; // leave inner while loop
      }
      else { // global var
        if (id->type > CHAR) d = (char *)(((int)d + sizeof (int) - 1) & -sizeof (int)); // align non-CHAR globals
        id->class = Global;
        id->val = (int)d;
        //printf("DBG global def: '%.*s' ty:#%d sz:%d @%d\n", id->len, id->name, id->type, id->type < PTR ? tysz[id->type] : sizeof (int), d - data);
        d = d + (ty >= PTR ? sizeof (int) : tysz[id->type]);
      }
      if (!match(',') && tk != ';') { printf("%s:%d:%d: bad global def: ',' or ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
    }
  }

  pc = (int *)idmain->val; if (!pc) { printf("main() not def'ed\n"); return -1; }
  if (src) return 0;

  // call exit if main returns
  *++e = PUSH; pp = e;
  *++e = EXIT;
  // setup stack
  bp = sp = (int *)((int)sp + STACK_SZ);
  *--sp = argc;
  *--sp = (int)argv;
  *--sp = (int)pp;

  // run...
  a = cycle = 0;
  while (1) {
    i = *pc++; ++cycle;
    if (dbg) {
      printf("%d> %s", cycle, &ops[i * 8]);
      if (i <= ADJ) printf(" %d (0x%08X)\n", *pc, *pc); else printf("\n");
    }
    if (d >= data + DATA_SZ) { printf("FATAL: data buffer overflow (pc %p, d %p, data %p, DATA_SZ 0x%08x)\n", pc, d, data, DATA_SZ); exit(-1); }
    if (sp < stack || sp >= stack + STACK_SZ) { printf("FATAL: stack overflow (pc %p, sp %p, stack %p, STACK_SZ 0x%08x)\n", pc, sp, stack, STACK_SZ); exit(-1); }

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
    else if (i == PRINTF) { pp = sp + pc[1]; a = printf((char *)pp[-1], pp[-2], pp[-3], pp[-4], pp[-5], pp[-6], pp[-7], pp[-8]); }
    else if (i == SCANF)  { pp = sp + pc[1]; a = scanf((char *)pp[-1], pp[-2], pp[-3], pp[-4], pp[-5], pp[-6], pp[-7], pp[-8]); }
    else if (i == MALLOC) a = (int)malloc(*sp);
    else if (i == FREE)   { a = *sp; free((void *)a); }
    else if (i == MEMSET) a = (int)memset((char *)sp[2], sp[1], *sp);
    else if (i == MEMCMP) a = memcmp((char *)sp[2], (char *)sp[1], *sp);
    else if (i == MEMCPY) a = (int)memcpy((char *)sp[2], (char *)sp[1], *sp);
    else if (i == EXIT)   { if (dbg) printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }

    else { printf("unknown instruction %d! cycle = %d\n", i, cycle); return -1; }
  }
  return -1;
}
