// c4.c - C in four (+1) functions

// char, int, structs, pointer types and enums
// if, while, return and expression statements
// just enough features to allow self-compilation and a bit more
// no enum names
// no assignments in expressions
// structs without members

// Based on c4.c - C in four functions
// Written by Robert Swierczek

// Compile with 'gcc [-Wall] -m32 -o c4+ c4+.c'

#include <unistd.h> // for read, write, close
#include <stdio.h> // for printf, scanf
#include <stdlib.h> // for malloc, free
#include <memory.h> // for memset, memcmp, memcpy
#include <fcntl.h> // for open

char *p, *lp, *tp, // current/line/token position in source code
     *d,      // data pointer
     *ops,    // opcodes
     *fn;     // filename

int *e, *le,  // current/line position in emitted code
    *tsize,   // array (indexed by type) of type sizes
    tnew,     // next available type
    tk,       // current token
    ival,     // current token value
    ty,       // current expression type
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
  int sline;
  int class;  // Num, Fun, Sys, Glo, Loc
  int type;
  int val;
  int stype;  // struct type
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
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, Struct, While,
  Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Dot, Arrow, Bracket
};

// opcodes (IMM...ADJ have parameter)
enum {
  IMM, LEA, JMP, JSR, BZ, BNZ, ENTER, ADJ, LEAVE, LI, LC, SI, SC, PUSH,
  OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD, 
  OPEN, READ, WRITE, CLOSE, PRINTF, SCANF, MALLOC, FREE, MEMSET, MEMCMP, MEMCPY, EXIT
};

// types
enum { CHAR, INT, PTR = 256, PTR2 = 512 };

char *e2s(int x) {
  char *str;
  if (0 <= x && x < 32) {
    str = "0x00\0 0x01\0 0x02\0 0x03\0 0x04\0 0x05\0 0x06\0 0x07\0 0x08\0 '\\t'\0 '\\n'\0 '\\v'\0 '\\f'\0 '\\r'\0 0x0E\0 0x0F\0 "
          "0x10\0 0x11\0 0x12\0 0x13\0 0x14\0 0x15\0 0x16\0 0x17\0 0x18\0 0x19\0 0x1A\0 0x1B\0 0x1C\0 0x1D\0 0x1E\0 0x1F\0 ";
    return &str[x * 6];
  }
  if (32 <= x && x <= 126) { str = (char *)&ival; memcpy(str, "' '", 4); str[1] = (char)x; return str; } // printable
  if (x == 127)  { return "DEL"; }
  if (Num <= x && x <= Id) {
    str = "num\0 fun\0 sys\0 glo\0 loc\0 id\0  ";
    x = x - Num; return &str[x * 5];
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
      id->name = pp;
      id->len = p - pp;
      id->hash = tk;
      tk = id->tk = Id;
      return;
    }
    else if (tk >= '0' && tk <= '9') { // number
      ival = tk - '0';
      if (ival) { while (*p >= '0' && *p <= '9') ival = ival * 10 + *p++ - '0'; } // decimal
      else if (*p == 'x' || *p == 'X') { // hexadecimal
        tk = *++p;
        while (tk && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F'))) {
          ival = ival * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
          tk = *++p;
        }
      }
      else { while (*p >= '0' && *p <= '7') ival = ival * 8 + *p++ - '0'; } // octal
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
        ival = *p++;
        if (ival == '\\') {
          ival = *p++;
          if      (ival == '0') ival = '\0';
          else if (ival == 't') ival = '\t';
          else if (ival == 'v') ival = '\v';
          else if (ival == 'f') ival = '\f';
          else if (ival == 'r') ival = '\r';
          else if (ival == 'n') ival = '\n';
        }
        if (tk == '"') *d++ = ival;
      }
      ++p; // skip '\'' or '"'
      if (tk == '"') ival = (int)pp;
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

void expr(int lev) {
  int i, *pp, sz;
  struct ident_s *s; // symbol
  struct member_s *m;

  if (!tk) { printf("%s:%d:%d: bad expr: unexpected eof\n", fn, line, tp - lp + 1); exit(-1); }
  else if (tk == Num) { // number or character literal
    *++e = IMM; *++e = ival;
    next();
    ty = INT;
  }
  else if (tk == '"') { // string literal
    *++e = IMM; *++e = ival;
    next();
    while (tk == '"') next(); // concatenate string literals
    d = (char *)(((int)d + sizeof(int)) & -sizeof(int)); // align
    ty = PTR;
  }
  else if (tk == Sizeof) { // sizeof expr
    next();
    if (tk == '(') next();
    else { printf("%s:%d:%d: bad sizeof expr: '(' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    ty = INT;
    if (tk == Int) next();
    else if (tk == Char) { next(); ty = CHAR; }
    else if (tk == Struct) {
      next();
      if (tk != Id) { printf("%s:%d:%d: bad sizeof expr: struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      if (!id->stype) { printf("%s:%d:%d: bad sizeof expr: struct '%.*s' not def'ed\n", fn, line, tp - lp + 1, id->len, id->name); exit(-1); }
      ty = id->stype;
//    printf("DBG sizeof expr: struct '%.*s', ty:#%d\n", id->len, id->name, id->stype);
      next();
    }
    else { printf("%s:%d:%d: bad sizeof expr: type expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    while (tk == Mul) { next(); ty = ty + PTR; }
    if (tk == ')') next();
    else { printf("%s:%d:%d: bad sizeof expr: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    *++e = IMM; *++e = ty >= PTR ? sizeof(int) : tsize[ty];
    ty = INT;
  }
  else if (tk == Id) { // identifier
    s = id; next();
    if (tk == '(') { // fun call
      next();
      i = 0;
      while (tk != ')') { // fun args
        expr(Cond);
        *++e = PUSH; ++i;
        if (tk == ',') next();
        else if (tk != ')') { printf("%s:%d:%d: bad fun call: ',' or ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      }
      next();
      if (s->class == Sys) *++e = s->val;
      else if (s->class == Fun) { *++e = JSR; *++e = s->val; }
      else if (s->class == 0) { printf("%s:%d:%d: bad fun arg: '%.*s' not def'ed\n", fn, line, tp - lp + 1, s->len, s->name); exit(-1); }
      else { printf("%s:%d:%d: bad fun arg: unexpected class '%s' of id '%.*s'; decl'ed in line %d\n", fn, line, tp - lp + 1, e2s(s->class), s->len, s->name, s->line); exit(-1); }
      if (i) { *++e = ADJ; *++e = i; } // stack adjust after fun call
      ty = s->type;
    }
    else if (s->class == Num) { *++e = IMM; *++e = s->val; ty = INT; } // enum identifier
    else { // variable
      if (s->class == Loc) { *++e = LEA; *++e = loc - s->val; } // local
      else if (s->class == Glo) { *++e = IMM; *++e = s->val; }  // global
      else if (s->class == 0) { printf("%s:%d:%d: bad var expr: '%.*s' not def'ed\n", fn, line, tp - lp + 1, s->len, s->name); exit(-1); }
      else { printf("%s:%d:%d: bad var expr: unexpected class '%s' of id '%.*s'; decl'ed in line %d\n", fn, line, tp - lp + 1, e2s(s->class), s->len, s->name, s->line); exit(-1); }
      ty = s->type;
      if (ty <= INT || ty >= PTR) *++e = (ty == CHAR) ? LC : LI;
    }
  }
  else if (tk == '(') { // typecast or group expr
    next();
    if (tk == Int || tk == Char || tk == Struct) { // typecast expr
      if (tk == Int) { i = INT; }
      else if (tk == Char) { i = CHAR; }
      else { // struct
        next();
        if (tk != Id) { printf("%s:%d:%d: bad typecast expr: struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
        i = id->stype;
      }
      next();
      while (tk == Mul) { next(); i = i + PTR; }
      if (tk == ')') next();
      else { printf("%s:%d:%d: bad typecast expr: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      expr(Inc);
      ty = i;
    }
    else { // group expr
      expr(Cond);
      if (tk == ')') next();
      else { printf("%s:%d:%d: bad expr: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    }
  }
  else if (tk == Mul) { // unary dereference expr
    next();
    expr(Inc);
    if (ty >= PTR) ty = ty - PTR;
    else { printf("%s:%d:%d: bad dereference expr: pointer type expected; got #%d\n", fn, line, tp - lp + 1, ty); exit(-1); }
    if (ty <= INT || ty >= PTR) *++e = (ty == CHAR) ? LC : LI;
  }
  else if (tk == And) { // unary address-of expr
    next();
    expr(Inc);
//  printf("DBG %s:%d:%d: address-of expr: op:%s(%d) ty:#%d\n", fn, line, tp - lp + 1, (IMM <= *e && *e <= EXIT) ? &ops[*e * 8] : "<unknown>", *e, ty);
    if (*e == LC || *e == LI) --e;
    // XXX else { printf("%s:%d:%d: bad address-of expr: unexpected opcode %s\n", fn, line, tp - lp + 1, &ops[*e * 8]); exit(-1); }
    ty = ty + PTR;
  }
  else if (tk == '!') { next(); expr(Inc); *++e = PUSH; *++e = IMM; *++e = 0; *++e = EQ; ty = INT; } // logical not expr
  else if (tk == '~') { next(); expr(Inc); *++e = PUSH; *++e = IMM; *++e = -1; *++e = XOR; ty = INT; } // binary not expr
  else if (tk == Add) { next(); expr(Inc); ty = INT; } // unary plus expr
  else if (tk == Sub) { // unary minus expr
    next();
    *++e = IMM;
    if (tk == Num) { *++e = -ival; next(); }
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
    *++e = IMM; *++e = ty >= PTR2 ? sizeof(int) : (ty >= PTR) ? tsize[ty - PTR] : 1;
    *++e = (i == Inc) ? ADD : SUB;
    *++e = (ty == CHAR) ? SC : SI;
  }
  else { printf("%s:%d:%d: bad unary/prefix expr: got unexpected token %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }

  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    i = ty;
    if (tk == Assign) { // assignment expr
      next();
      if (*e == LC || *e == LI) *e = PUSH;
      else { printf("%s:%d:%d: bad assign expr: unexpected opcode %s\n", fn, line, tp - lp + 1, &ops[*e * 8]); exit(-1); }
      expr(Assign);
      *++e = (i == CHAR) ? SC : SI; ty = i;
    }
    else if (tk == Cond) { // conditional expr
      next();
      *++e = BZ; pp = ++e;
      expr(Cond);
      if (tk == ':') next();
      else { printf("%s:%d:%d: bad cond expr: ':' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      *pp = (int)(e + 3); *++e = JMP; pp = ++e;
      expr(Cond);
      *pp = (int)(e + 1);
    }
    else if (tk == Lor)  { next(); *++e = BNZ; pp = ++e; expr(Land); *pp = (int)(e + 1); ty = INT; } // logical or expr
    else if (tk == Land) { next(); *++e = BZ;  pp = ++e; expr(Or);   *pp = (int)(e + 1); ty = INT; } // logical and expr
    else if (tk == Or)  { next(); *++e = PUSH; expr(Xor); *++e = OR;  ty = INT; } // binary or expr
    else if (tk == Xor) { next(); *++e = PUSH; expr(And); *++e = XOR; ty = INT; } // binary xor expr
    else if (tk == And) { next(); *++e = PUSH; expr(Eq);  *++e = AND; ty = INT; } // binary and expr
    else if (tk == Eq)  { next(); *++e = PUSH; expr(Lt);  *++e = EQ;  ty = INT; } // equal expr
    else if (tk == Ne)  { next(); *++e = PUSH; expr(Lt);  *++e = NE;  ty = INT; } // not-equal expr
    else if (tk == Lt)  { next(); *++e = PUSH; expr(Shl); *++e = LT;  ty = INT; } // less-than expr
    else if (tk == Gt)  { next(); *++e = PUSH; expr(Shl); *++e = GT;  ty = INT; } // greater-than expr
    else if (tk == Le)  { next(); *++e = PUSH; expr(Shl); *++e = LE;  ty = INT; } // less-or-equal expr
    else if (tk == Ge)  { next(); *++e = PUSH; expr(Shl); *++e = GE;  ty = INT; } // greater-or-equal expr
    else if (tk == Shl) { next(); *++e = PUSH; expr(Add); *++e = SHL; ty = INT; } // shift-left expr
    else if (tk == Shr) { next(); *++e = PUSH; expr(Add); *++e = SHR; ty = INT; } // shift-right expr
    else if (tk == Add) { // addition: num + num, ptr + num, num + ptr OK; ptr + ptr NOT ALLOWED
      next();
      *++e = PUSH;
      expr(Mul);
      if (i >= PTR && ty >= PTR) { printf("%s:%d:%d: bad add expr: ptr + ptr not allowed\n", fn, line, tp - lp + 1); exit(-1); }
      sz = i >= PTR2 ? sizeof(int) : i >= PTR ? tsize[i - PTR] : 1;
      if (sz > 1) { *++e = PUSH; *++e = IMM; *++e = sz; *++e = MUL;  }
      *++e = ADD;
      ty = i;
    }
    else if (tk == Sub) { // subtraction: num - num, ptr - num, ptr - ptr OK; num - ptr NOT ALLOWED
      next();
      *++e = PUSH;
      expr(Mul);
      if (i < PTR && ty >= PTR) { printf("%s:%d:%d: bad sub expr: num - ptr not allowed\n", fn, line, tp - lp + 1); exit(-1); }
      sz = i >= PTR2 ? sizeof(int) : i >= PTR ? tsize[i - PTR] : 1;
      if (i == ty && sz > 1) { *++e = SUB; *++e = PUSH; *++e = IMM; *++e = sz; *++e = DIV; ty = INT; }
      else if (sz > 1) { *++e = PUSH; *++e = IMM; *++e = sz; *++e = MUL; *++e = SUB; }
      else *++e = SUB;
      ty = i;
    }
    else if (tk == Mul) { next(); *++e = PUSH; expr(Inc); *++e = MUL; ty = INT; } // multiplication
    else if (tk == Div) { next(); *++e = PUSH; expr(Inc); *++e = DIV; ty = INT; } // division
    else if (tk == Mod) { next(); *++e = PUSH; expr(Inc); *++e = MOD; ty = INT; } // modulo
    else if (tk == Inc || tk == Dec) { // post-inc/-dec
      if (*e == LC) { *e = PUSH; *++e = LC; }
      else if (*e == LI) { *e = PUSH; *++e = LI; }
      else { printf("%s:%d:%d: bad post-inc/-dec expr: unexpected opcode %s\n", fn, line, tp - lp + 1, &ops[*e * 8]); exit(-1); }
      sz = ty >= PTR2 ? sizeof(int) : ty >= PTR ? tsize[ty - PTR] : 1;
      *++e = PUSH; *++e = IMM; *++e = sz;
      *++e = (tk == Inc) ? ADD : SUB;
      *++e = (ty == CHAR) ? SC : SI;
      *++e = PUSH; *++e = IMM; *++e = sz;
      *++e = (tk == Inc) ? SUB : ADD;
      next();
    }
    else if (tk == Dot || tk == Arrow) { // struct member expr
      if (tk == Dot) ty = ty + PTR;
      if (ty <= PTR + INT || ty >= PTR2) { printf("%s:%d:%d: bad struct member expr: expected lhs expr type #%d...#%d; got #%d\n", fn, line, tp - lp + 1, PTR + INT + 1, PTR2 - 1, ty); exit(-1); }
      next();
      if (tk != Id) { printf("%s:%d:%d: bad struct member expr: member id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      m = members[ty - PTR]; while (m && m->id != id) m = m->next;
      if (!m) { printf("%s:%d:%d: struct member not def'ed\n", fn, line, tp - lp + 1); exit(-1); }
      if (m->offset) { *++e = PUSH; *++e = IMM; *++e = m->offset; *++e = ADD; }
      ty = m->type;
      if (ty <= INT || ty >= PTR) *++e = (ty == CHAR) ? LC : LI;
      next();
    }
    else if (tk == Bracket) { // array element expr
      next();
      *++e = PUSH;
      expr(Cond);
      if (tk == ']') next(); else { printf("%s:%d:%d: bad array element expr: ']' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
      if (i < PTR) { printf("%s:%d:%d: bad array element expr: lhs pointer type expected; got type #%d\n", fn, line, tp - lp + 1, i); exit(-1); }
      i = i - PTR; sz = i >= PTR ? sizeof(int) : tsize[i];
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

  if (tk == If) {
    next();
    if (tk == '(') next();
    else { printf("%s:%d:%d: bad if stmt: '(' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    expr(Cond);
    if (tk == ')') next();
    else { printf("%s:%d:%d: bad if stmt: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
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
    if (tk == '(') next();
    else { printf("%s:%d:%d: bad while stmt: '(' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    expr(Cond);
    if (tk == ')') next();
    else { printf("%s:%d:%d: bad while stmt: ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
    *++e = BZ; b = ++e;
    stmt();
    *++e = JMP; *++e = (int)a;
    *b = (int)(e + 1);
  }
  else if (tk == Return) {
    next();
    if (tk != ';') expr(Cond);
    *++e = LEAVE;
    if (tk == ';') next();
    else { printf("%s:%d:%d: bad return stmt: ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
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
    if (tk == ';') next();
    else { printf("%s:%d:%d: bad assign stmt: ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); exit(-1); }
  }
}

int main(int argc, char **argv) {
  int *code, *stack;
  char *data;
  int fd, bt, mbt, poolsz;
  struct ident_s *idmain;
  struct member_s *m;
  int *pc, *sp, *bp, a, cycle; // vm registers
  int i, *pp; // temps
  char *t;

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { dbg = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c4 [-s] [-d] [-v] file ...\n"); return -1; }

  fn = *argv;
  fd = open(fn, 0); if (fd < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256 * 1024; // arbitrary size
  sym = malloc(poolsz); if (!sym) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; }
  le = e = code = malloc(poolsz); if (!e) { printf("could not malloc(%d) text area\n", poolsz); return -1; }
  d = data = malloc(poolsz); if (!d) { printf("could not malloc(%d) data area\n", poolsz); return -1; }
  sp = stack = malloc(poolsz); if (!sp) { printf("could not malloc(%d) stack area\n", poolsz); return -1; }
  tsize = malloc(PTR * sizeof(int)); if (!tsize) { printf("could not malloc() tsize area\n"); return -1; }
  members = malloc(PTR * sizeof(struct member_s *)); if (!members) { printf("could not malloc() members area\n"); return -1; }

  memset(sym,   0, poolsz);
  memset(code,  0, poolsz);
  memset(data,  0, poolsz);
  memset(stack, 0, poolsz);
  memset(tsize,   0, PTR * sizeof(int));
  memset(members, 0, PTR * sizeof(struct member_s *));

  ops = "IMM\0    LEA\0    JMP\0    JSR\0    BZ\0     BNZ\0    ENTER\0  ADJ\0    LEAVE\0  LI\0     LC\0     SI\0     SC\0     PUSH\0   "
        "OR\0     XOR\0    AND\0    EQ\0     NE\0     LT\0     GT\0     LE\0     GE\0     SHL\0    SHR\0    ADD\0    SUB\0    MUL\0    DIV\0    MOD\0    "
        "OPEN\0   READ\0   WRITE\0  CLOSE\0  PRINTF\0 SCANF\0  MALLOC\0 FREE\0   MEMSET\0 MEMCMP\0 MEMCPY\0 EXIT\0   ";

  p = "char else enum if int return sizeof struct while "
      "open read write close printf scanf malloc free memset memcmp memcpy exit "
      "void main";
  i = Char; while (i <= While) { next(); id->tk = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { next(); id->class = Sys; id->type = INT; id->val = i++; } // add library to symbol table
  next(); id->tk = Char; // handle void type
  next(); idmain = id; // keep track of main

  lp = p = malloc(poolsz); if (!p) { printf("could not malloc(%d) source area\n", poolsz); return -1; }
  i = read(fd, p, poolsz-1); if (i <= 0) { printf("%s: read() returned %d\n", fn, i); return -1; }
  p[i] = 0;
  close(fd);

  // add primitive types
  tsize[tnew++] = sizeof(char);
  tsize[tnew++] = sizeof(int);
  
  // parse decls
  line = 1;
  next();
  while (tk) {
    bt = INT; // basetype
    if (tk == Int) next();
    else if (tk == Char) { next(); bt = CHAR; }
    else if (tk == Enum) { // enum decl
      next(); // skip "enum"
      if (tk != '{') { printf("%s:%d:%d: bad enum decl: '{' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
      next(); //  skip '{'
      i = 0;
      while (tk != '}') {
        if (tk != Id) { printf("%s:%d:%d: bad enum decl: enum id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
        if (id->class) { printf("%s:%d:%d: duplicate enum '%.*s'; already decl'ed in line %d\n", fn, line, tp - lp + 1, id->len, id->name, id->line); return -1; }
        next();
        if (tk == Assign) {
          next();
          if (tk != Num) { printf("%s:%d:%d: bad enum decl: num initializer expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
          i = ival;
          next();
        }
        id->line = line; id->class = Num; id->type = INT; id->val = i++;
        if (tk == ',') next();
      }
      next();
    }
    else if (tk == Struct) { // struct decl
      next();
      t = tp;
      if (tk == Id) { // struct id
        if (!id->stype) id->stype = tnew++;
        bt = id->stype;
        next();
      } else { // anonymous struct
        bt = tnew++;
        if (tk != '{') { printf("%s:%d:%d: bad struct decl: '{' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
      }
      if (tk == '{') { // struct def
//      printf("DBG struct def: '%.*s', ty:#%d\n", id->len, id->name, id->stype);
        if (members[bt]) { printf("%s:%d:%d: duplicate struct '%.*s'; already def'ed in line %d\n", fn, line, t - lp + 1, id->len, id->name, id->sline); return -1; }
        id->sline = line;
        next();
        i = 0;
        while (tk != '}') { // struct member decl
          if (tk == Int) mbt = INT;
          else if (tk == Char) mbt = CHAR;
          else if (tk == Struct) {
            next(); 
            if (tk != Id) { printf("%s:%d:%d: bad struct member decl: struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
            if (!id->stype) { printf("%s:%d:%d: bad struct member decl: struct '%.*s' not def'ed\n", fn, line, tp - lp + 1, id->len, id->name); return -1; }
            mbt = id->stype;
          }
          else { printf("%s:%d:%d: bad struct member decl: member type expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
          next();
          m = 0;
          while (m == 0 || tk != ';') { // at least one member id required
            ty = mbt;
            while (tk == Mul) { next(); ty = ty + PTR; }
            if (tk != Id) { printf("%s:%d:%d: bad struct member decl: member id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
            if (ty > CHAR) i = (i + sizeof (int) - 1) & -sizeof (int); // align non-CHAR members
            m = malloc(sizeof(struct member_s));
            m->id = id;
            m->offset = i;
            m->type = ty;
            m->next = members[bt];
            members[bt] = m;
            i = i + (ty >= PTR ? sizeof(int) : tsize[ty]);
            next();
            if (tk == ',') next();
            else if (tk != ';') { printf("%s:%d:%d: bad struct member decl: ',' or ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
          }
          next();
        }
        next();
        tsize[bt] = (i + sizeof (int) - 1) & -sizeof (int);
//      printf("DBG struct def: sz:%d\n", tsize[bt]);
      }
    }
    else { printf("%s:%d:%d: bad decl: type expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }

    while (tk != ';') {
      ty = bt;
      while (tk == Mul) { next(); ty = ty + PTR; } // pointer declarator
      if (tk != Id) { printf("%s:%d:%d: bad decl: global id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
      if (id->class) { printf("%s:%d:%d: duplicate global '%.*s'; already decl'ed as %s in line %d\n", fn, line, tp - lp + 1, id->len, id->name, e2s(id->class), id->line); return -1; }
      id->type = ty;
      next();
      if (tk == '(') { // func decl
        id->line = line;
        id->class = Fun;
        id->val = (int)(e + 1);
        next(); i = 0;
        while (tk != ')') { // arg list decl
          if (tk == Int) ty = INT;
          else if (tk == Char) ty = CHAR;
          else if (tk == Struct) {
            next(); 
            if (tk != Id) { printf("%s:%d:%d: bad arg decl: struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
            ty = id->stype;
          }
          else { printf("%s:%d:%d: bad arg decl: arg type expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
          next();
          while (tk == Mul) { next(); ty = ty + PTR; }
          if (tk != Id) { printf("%s:%d:%d: bad arg decl: arg id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
          if (id->class == Loc) { printf("%s:%d:%d: duplicate arg '%.*s': already decl'ed in line %d\n", fn, line, tp - lp + 1, id->len, id->name, id->line); return -1; }
          id->line = line;
          id->hclass = id->class; id->class = Loc;
          id->htype = id->type; id->type = ty;
          id->hval = id->val; id->val = i++;
          next();
          if (tk == ',') next();
          else if (tk != ')') { printf("%s:%d:%d: bad arg decl: ',' or ')' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
        }
        next();
        if (tk != '{') { printf("%s:%d:%d: bad func def: '{' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
        loc = ++i;
        next();
        while (tk == Int || tk == Char || tk == Struct) { // locals
          if (tk == Int) bt = INT;
          else if (tk == Char) bt = CHAR;
          else { // struct
            next(); 
            if (tk != Id) { printf("%s:%d:%d: bad local decl; struct id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
            bt = id->stype;
          }
          next();
          while (tk != ';') {
            ty = bt;
            while (tk == Mul) { next(); ty = ty + PTR; }
            if (tk != Id) { printf("%s:%d:%d: bad local decl; local id expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
            if (id->class == Loc) { printf("%s:%d:%d: duplicate local '%.*s': already decl'ed in line %d\n", fn, line, tp - lp + 1, id->len, id->name, id->line); return -1; }
            id->line = line;
            id->hclass = id->class; id->class = Loc;
            id->htype = id->type; id->type = ty;
            id->hval = id->val; id->val = ++i;
            next();
            if (tk == ',') next();
            else if (tk != ';') { printf("%s:%d:%d: bad local decl: ',' or ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
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
          ++id;
        }
        tk = ';'; // leave inner while loop
      }
      else { // global
        if (id->type > CHAR) d = (char *)(((int)d + sizeof (int) - 1) & -sizeof (int)); // align non-CHAR globals
        id->class = Glo;
        id->val = (int)d;
//      printf("DBG global def: '%.*s' ty:#%d sz:%d @%p\n", id->len, id->name, id->type, tsize[id->type], d);
        d = d + (ty >= PTR ? sizeof(int) : tsize[id->type]);
      }
      if (tk == ',') next();
      else if (tk != ';') { printf("%s:%d:%d: bad decl: ',' or ';' expected; got %s\n", fn, line, tp - lp + 1, e2s(tk)); return -1; }
    }
    next();
  }

  pc = (int *)idmain->val; if (!pc) { printf("main() not def'ed\n"); return -1; }
  if (src) return 0;

  // call exit if main returns
  *++e = PUSH; pp = e;
  *++e = EXIT;
  // setup stack
  bp = sp = (int *)((int)sp + poolsz);
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
