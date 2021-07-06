#include <stdio.h>  // for printf()
#include <stdlib.h> // for malloc(), free(), exit()
#include <memory.h> // for memset(), memcmp()
#include <fcntl.h>  // for open()
#include <unistd.h> // for read(), write(), close()

char *p, *lp, // current position in source code
     *d,      // data/bss pointer
     *ops;    // opcodes

int *e, *le,  // current position in emitted code
    *id,      // currently parsed identifier
    *sym,     // symbol table (simple list of identifiers)
    tk,       // current token
    val,      // current token value
    ty,       // current expression type
    loc,      // local variable offset
    line,     // current line number
    src,      // print source and assembly flag
    debug;    // print executed instructions

// tokens and classes (operators last and in precedence order)
enum { Num = 128, Global, Fun, Sys, Local, Id,
       Char, Else, Enum, If, Int, Return, Sizeof, While,
       Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Bracket };

// opcodes (IMM...ADJ have parameter)
enum { IMM, LEA, JMP, JSR, BZ, BNZ, ENTER, ADJ, LEAVE, LI, LC, SI, SC, PUSH, SWAP,
       OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
       OPEN, READ, WRITE, CLOSE, PRINTF, MALLOC, FREE, MEMSET, MEMCPY, MEMCMP, EXIT };

// types
enum { CHAR, INT, PTR };

// identifier offsets (since we can't create an ident struct)
enum { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

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
          if (*le <= ADJ) { ++le; printf(" %d (0x%08x)\n", *le, *le); } else printf("\n");
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
      pp = d;
      while (*p != 0 && *p != tk) {
        if ((val = *p++) == '\\') {
          if ((val = *p++) == '0') val = '\0';
          else if (val == 't') val = '\t';
          else if (val == 'v') val = '\v';
          else if (val == 'f') val = '\f';
          else if (val == 'r') val = '\r';
          else if (val == 'n') val = '\n';
        }
        if (tk == '"') *d++ = (char)val;
      } ++p; // skip delimiter
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
  int *pp, i, x;

  if (!tk) { printf("%d: unexpected eof in expression\n", line); exit(-1); }
  else if (tk == Num) { *++e = IMM; *++e = val; next(); ty = INT; } // number or character literal
  else if (tk == '"') { // string literal
    *++e = IMM; *++e = val; next();
    while (tk == '"') next(); // concatenate string literals
    d = (char *)((int)d + sizeof(int) & -sizeof(int)); // align
    ty = PTR;
  }
  else if (tk == Sizeof) { // sizeof operator
    next(); if (tk == '(') next(); else { printf("%d: '(' expected in sizeof (tk=%d)\n", line, tk); exit(-1); }
    ty = INT; if (tk == Int) next(); else if (tk == Char) { next(); ty = CHAR; }
    while (tk == Mul) { next(); ty = ty + PTR; }
    if (tk == ')') next(); else { printf("%d: ')' expected in sizeof (tk=%d)\n", line, tk); exit(-1); }
    *++e = IMM; *++e = (ty == CHAR) ? sizeof(char) : sizeof(int);
    ty = INT;
  }
  else if (tk == Id) { // identifier
    pp = id; next();
    if (tk == '(') { // function call
      i = 0; next();
      while (tk != ')') { // parameter
        expr(Assign); *++e = PUSH; ++i;
        if (tk == ',') next(); else if (tk != ')') { printf("%d: bad function call (tk=%d)\n", line, tk); exit(-1); }
      } next(); // skip ')'
      if (pp[Class] == Sys) *++e = pp[Val];
      else if (pp[Class] == Fun) { *++e = JSR; *++e = pp[Val]; }
      else { printf("%d: bad function call (class=%d)\n", line, pp[Class]); exit(-1); }
      if (i) { *++e = ADJ; *++e = i; } // stack adjust after call
      ty = pp[Type];
    }
    else if (pp[Class] == Num) { *++e = IMM; *++e = pp[Val]; ty = INT; } // enum identifier
    else { // variable
      if (pp[Class] == Local) { *++e = LEA; *++e = loc - pp[Val]; } // local
      else if (pp[Class] == Global) { *++e = IMM; *++e = pp[Val]; }  // global
      else { printf("%d: undefined variable (class=%d)\n", line, pp[Class]); exit(-1); }
      *++e = ((ty = pp[Type]) == CHAR) ? LC : LI;
    }
  }
  else if (tk == '(') { // type cast or group
    next();
    if (tk == Int || tk == Char) { // type cast
      i = (tk == Int) ? INT : CHAR; next();
      while (tk == Mul) { next(); i = i + PTR; }
      if (tk == ')') next(); else { printf("%d: bad cast (tk=%d)\n", line, tk); exit(-1); }
      expr(Inc);
      ty = i;
    }
    else { // group
      expr(Assign);
      if (tk == ')') next(); else { printf("%d: ')' expected (tk=%d)\n", line, tk); exit(-1); }
    }
  }
  else if (tk == Mul) { // unary dereference operator
    next(); expr(Inc);
    if (ty > INT) ty = ty - PTR; else { printf("%d: bad dereference (ty=%d)\n", line, ty); exit(-1); }
    *++e = (ty == CHAR) ? LC : LI;
  }
  else if (tk == And) { // unary address-of operator
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
    i = tk; next(); expr(Inc);
    if (*e == LC) { *e = PUSH; *++e = LC; }
    else if (*e == LI) { *e = PUSH; *++e = LI; }
    else { printf("%d: bad lvalue in pre-increment/-decrement (instruction=%d)\n", line, *e); exit(-1); }
    *++e = PUSH;
    *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
    *++e = (i == Inc) ? ADD : SUB;
    *++e = (ty == CHAR) ? SC : SI;
  }
  else { printf("%d: bad expression (tk=%d)\n", line, tk); exit(-1); }

  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    i = ty;
    if (tk == Assign) {
      next();
      if (*e == LC || *e == LI) *e = PUSH; else { printf("%d: bad lvalue in assignment (instruction=%d)\n", line, *e); exit(-1); }
      expr(Assign); *++e = ((ty = i) == CHAR) ? SC : SI;
    }
    else if (tk == Cond) {
      next(); *++e = BZ; pp = ++e; expr(Assign);
      if (tk == ':') next(); else { printf("%d: conditional missing colon (tk=%d)\n", line, tk); exit(-1); }
      *pp = (int)(e + 3); *++e = JMP; pp = ++e; expr(Cond); *pp = (int)(e + 1);
    }
    else if (tk == Lor)  { next(); *++e = BNZ;  pp = ++e;  expr(Land); *pp = (int)(e + 1); ty = INT; }
    else if (tk == Land) { next(); *++e = BZ;   pp = ++e;  expr(Or);   *pp = (int)(e + 1); ty = INT; }
    else if (tk == Or)   { next(); *++e = PUSH; expr(Xor); *++e = OR;  ty = INT; }
    else if (tk == Xor)  { next(); *++e = PUSH; expr(And); *++e = XOR; ty = INT; }
    else if (tk == And)  { next(); *++e = PUSH; expr(Eq);  *++e = AND; ty = INT; }
    else if (tk == Eq)   { next(); *++e = PUSH; expr(Lt);  *++e = EQ;  ty = INT; }
    else if (tk == Ne)   { next(); *++e = PUSH; expr(Lt);  *++e = NE;  ty = INT; }
    else if (tk == Lt)   { next(); *++e = PUSH; expr(Shl); *++e = LT;  ty = INT; }
    else if (tk == Gt)   { next(); *++e = PUSH; expr(Shl); *++e = GT;  ty = INT; }
    else if (tk == Le)   { next(); *++e = PUSH; expr(Shl); *++e = LE;  ty = INT; }
    else if (tk == Ge)   { next(); *++e = PUSH; expr(Shl); *++e = GE;  ty = INT; }
    else if (tk == Shl)  { next(); *++e = PUSH; expr(Add); *++e = SHL; ty = INT; }
    else if (tk == Shr)  { next(); *++e = PUSH; expr(Add); *++e = SHR; ty = INT; }
    else if (tk == Add)  {
      next(); *++e = PUSH; expr(Mul);
      if (i >= PTR && ty >= PTR) { printf("%d: bad pointer addition\n", line); exit(-1); }
      if (i < PTR && ty >= PTR) { *++e = SWAP; x = i; i = ty; ty = x; } // value on left side, pointer on right side -> swap
      if ((ty = i) > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof(int); *++e = MUL; } // add value to int pointer or pointer pointer
      *++e = ADD; // add value to value, value to char pointer, char pointer to char pointer
    }
    else if (tk == Sub) {
      next(); *++e = PUSH; expr(Mul);
      if (i > PTR && i == ty) { *++e = SUB; *++e = PUSH; *++e = IMM; *++e = sizeof(int); *++e = DIV; ty = INT; } // subtract int pointer or pointer pointer from int pointer or pointer pointer
      else if ((ty = i) > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof(int); *++e = MUL; *++e = SUB; } // subtract value from int pointer or pointer pointer
      else *++e = SUB; // subtract value from value, value from char pointer, char pointer from char pointer
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
      next(); // skip "++" or "--"
    }
    else if (tk == Bracket) {
      next(); *++e = PUSH; expr(Assign);
      if (tk == ']') next(); else { printf("%d: ']' expected (tk=%d)\n", line, tk); exit(-1); }
      if (ty >= PTR) { printf("%d: bad array subscript\n", line); exit(-1); }
      if (i < PTR) { printf("%d: pointer type expected (i=%d)\n", line, i); exit(-1); }
      if (i > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  }
      *++e = ADD;
      *++e = ((ty = i - PTR) == CHAR) ? LC : LI;
    }
    else { printf("%d: compiler error (tk=%d)\n", line, tk); exit(-1); }
  }
}

void stmt() {
  int *a, *b;

  if (tk == If) { // if
    next(); if (tk != '(') { printf("%d: '(' expected in if (tk=%d)\n", line, tk); exit(-1); }
    next(); expr(Assign);
    if (tk != ')') { printf("%d: ')' expected in if (tk=%d)\n", line, tk); exit(-1); }
    *++e = BZ; b = ++e;
    next(); stmt();
    if (tk == Else) { // else
      *b = (int)(e + 3); *++e = JMP; b = ++e;
      next(); stmt();
    }
    *b = (int)(e + 1);
  }
  else if (tk == While) { // while
    a = e + 1;
    next(); if (tk != '(') { printf("%d: '(' expected in while (tk=%d)\n", line, tk); exit(-1); }
    next(); expr(Assign);
    if (tk != ')') { printf("%d: ')' expected in while (tk=%d)\n", line, tk); exit(-1); }
    *++e = BZ; b = ++e;
    next(); stmt();
    *++e = JMP; *++e = (int)a;
    *b = (int)(e + 1);
  }
  else if (tk == Return) { // return
    next(); if (tk != ';') expr(Assign);
    *++e = LEAVE;
    if (tk != ';') { printf("%d: ';' expected in return (tk=%d)\n", line, tk); exit(-1); }
    next(); // skip ';'
  }
  else if (tk == '{') { // block
    next(); // skip '{'
    while (tk != '}') stmt();
    next(); // skip '}'
  }
  else if (tk == ';') { // empty stmt
    next(); // skip ';'
  }
  else { // expr stmt
    expr(Assign);
    if (tk != ';') { printf("%d: ';' expected (tk=%d)\n", line, tk); exit(-1); }
    next(); // skip ';'
  }
}

int main(int argc, char **argv) {
  int fd, bt, poolsz, *idmain;
  int *pc, *sp, *bp, a, cycle; // vm registers
  int i, *pp, x; // temps

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: cc [-s] [-d] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256 * 1024; // arbitrary size
  if (!(sym = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; }
  if (!(le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; }
  if (!(d = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; }
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%d) stack area\n", poolsz); return -1; }

  memset(sym, 0, poolsz);
  memset(e,   0, poolsz);
  memset(d,   0, poolsz);
  memset(sp,  0, poolsz);

  ops = "IMM\0    LEA\0    JMP\0    JSR\0    BZ\0     BNZ\0    ENTER\0  ADJ\0    LEAVE\0  LI\0     LC\0     SI\0     SC\0     PUSH\0   SWAP\0   "
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
  line = 1; next();
  while (tk) {
    bt = INT; // basetype
    if (tk == Int) next();
    else if (tk == Char) { next(); bt = CHAR; }
    else if (tk == Enum) {
      next(); if (tk != '{') { printf("%d: bad enum declaration (tk=%d)\n", line, tk); return -1; }
      i = 0; next();
      while (tk != '}') {
        if (tk != Id) { printf("%d: bad enum identifier (tk=%d)\n", line, tk); return -1; }
        next();
        if (tk == Assign) {
          next(); if (tk != Num) { printf("%d: bad enum initializer (tk=%d)\n", line, tk); return -1; }
          i = val; next();
        }
        id[Class] = Num; id[Type] = INT; id[Val] = i++;
        if (tk == ',') next(); else if (tk != '}') { printf("%d: bad enum definition (tk=%d)\n", line, tk); return -1; }
      } next(); // skip '}'
    }
    while (tk != ';') {
      ty = bt;
      while (tk == Mul) { next(); ty = ty + PTR; }
      if (tk != Id) { printf("%d: bad global declaration (tk=%d)\n", line, tk); return -1; }
      if (id[Class]) { printf("%d: duplicate global definition '%s' (class=%d)\n", line, (char *)id[Name], id[Class]); return -1; }
      id[Type] = ty; next();
      if (tk == '(') { // function
        id[Class] = Fun;
        id[Val] = (int)(e + 1);
        i = 0; next();
        while (tk != ')') { // parameter
          ty = INT;
          if (tk == Int) next();
          else if (tk == Char) { next(); ty = CHAR; }
          else ; // TODO: error handling
          while (tk == Mul) { next(); ty = ty + PTR; }
          if (tk != Id) { printf("%d: bad parameter declaration (tk=%d)\n", line, tk); return -1; }
          if (id[Class] == Local) { printf("%d: duplicate parameter definition '%s'\n", line, (char *)id[Name]); return -1; }
          id[HClass] = id[Class]; id[Class] = Local;
          id[HType]  = id[Type];  id[Type]  = ty;
          id[HVal]   = id[Val];   id[Val]   = i++;
          next(); if (tk == ',') next(); else if (tk != ')') { printf("%d: bad function declaration (tk=%d)\n", line, tk); exit(-1); }
        } next(); // skip ')'
        if (tk != '{') { printf("%d: bad function definition (tk=%d)\n", line, tk); return -1; }
        loc = ++i; next();
        while (tk == Int || tk == Char) { // locals
          bt = (tk == Int) ? INT : CHAR; next();
          while (tk != ';') {
            ty = bt;
            while (tk == Mul) { next(); ty = ty + PTR; }
            if (tk != Id) { printf("%d: bad local declaration (tk=%d)\n", line, tk); return -1; }
            if (id[Class] == Local) { printf("%d: duplicate local definition '%s'\n", line, (char *)id[Name]); return -1; }
            id[HClass] = id[Class]; id[Class] = Local;
            id[HType]  = id[Type];  id[Type]  = ty;
            id[HVal]   = id[Val];   id[Val]   = ++i;
            next(); if (tk == ',') next(); else if (tk != ';') { printf("%d: bad local definition (tk=%d)\n", line, tk); exit(-1); }
          } next(); // skip ';'
        }
        *++e = ENTER; *++e = i - loc;
        while (tk != '}') stmt();
        *++e = LEAVE;
        id = sym; // unwind symbol table locals
        while (id[Tk]) {
          if (id[Class] == Local) {
            id[Class] = id[HClass];
            id[Type]  = id[HType];
            id[Val]   = id[HVal];
          }
          id = id + Idsz;
        }
        tk = ';'; // break inner while loop
      }
      else { // global
        id[Class] = Global;
        id[Val] = (int)d;
        d = d + sizeof(int);
      }
      if (tk == ',') next(); else if (tk != ';') { printf("%d: bad global definition (tk=%d)\n", line, tk); exit(-1); }
    } next(); // skip ';'
  }

  if (!(pc = (int *)idmain[Val])) { printf("main() not defined\n"); return -1; }
  if (src) return 0;

  // call exit if main returns
  *++e = PUSH; pp = e; *++e = EXIT;
  // setup stack
  bp = sp = (int *)((int)sp + poolsz);
  *--sp = argc; *--sp = (int)argv; *--sp = (int)pp;

  // run...
  a = cycle = 0;
  while (1) {
    i = *pc++; ++cycle;
    if (debug) { printf("%d> %s", cycle, &ops[i * 8]); if (i <= ADJ) printf(" %d (0x%08x)\n", *pc, *pc); else printf("\n"); }
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
    else if (i == SWAP)  { x = a; a = *sp; *sp = x; }                       // swap

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
    else if (i == PRINTF) { pp = sp + pc[1]; a = printf((char *)pp[-1], pp[-2], pp[-3], pp[-4], pp[-5], pp[-6]); }
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