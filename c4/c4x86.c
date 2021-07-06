// c4.c - C in four functions

// char, int, pointer types and enums
// if, while, return and expression statements
// just enough features to allow self-compilation and a bit more

// Written by Robert Swierczek
// + x86 JIT compiler by Dmytro Sirenko

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#ifdef _WIN32
#include "w32.h"
#else
#include <sys/mman.h>
#endif

char *p, *lp, // current position in source code
     *jitmem, // executable memory for JIT-compiled native code
     *je,     // current position in emitted native code
     *data,   // data/bss pointer
     **linemap; // maps a line number into its source position

int *e, *le, *text, // current position in emitted code
    *id,      // currently parsed identifier
    *sym,     // symbol table (simple list of identifiers)
    tk,       // current token
    val,      // current token value
    ty,       // current expression type
    loc,      // local variable offset
    line,     // current line number
    *srcmap,  // maps a bytecode into its corresponding source line number
    src;      // print source, c4 assembly and JIT addresses

// tokens and classes (operators last and in precedence order)
enum Token { Num = 128, Fun, Sys, Glo, Loc, Id,
             Char, Else, Enum, If, Int, Return, Sizeof, While,
             Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Bracket };

// opcodes
enum Opcode { LEA, IMM, JMP, JSR, BZ, BNZ, ENTER, ADJ, LEAVE, LI, LC, SI, SC, PUSH,
              OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
              OPEN, READ, CLOS, PRTF, MALC, MSET, MCMP, MCPY, MMAP, DSYM, QSRT, EXIT };

// types
enum Ty { CHAR, INT, PTR };

// identifier offsets (since we can't create an ident struct)
enum IdOffset { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

void next() {
  char *pp; // previous position

  while (tk = *p) {
    ++p;
    if (tk == '\n') { // end of line
      //linemap[line] = lp; 
      lp = p;
      //while (le < e) { srcmap[le - text] = line; le++; };
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
  int t, *d;

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
      if (d[Class] == Sys) *++e = d[Val];
      else if (d[Class] == Fun) { *++e = JSR; *++e = d[Val]; }
      else { printf("%d: bad function call (class=%d)\n", line, d[Class]); exit(-1); }
      if (t) { *++e = ADJ; *++e = t; } // stack adjust after call
      ty = d[Type];
    }
    else if (d[Class] == Num) { *++e = IMM; *++e = d[Val]; ty = INT; } // enum identifier
    else { // variable
      if (d[Class] == Loc) { *++e = LEA; *++e = loc - d[Val]; } // local
      else if (d[Class] == Glo) { *++e = IMM; *++e = d[Val]; }  // global
      else { printf("%d: undefined variable (class=%d)\n", line, d[Class]); exit(-1); }
      *++e = ((ty = d[Type]) == CHAR) ? LC : LI;
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
      *++e = BZ; d = ++e;
      expr(Assign);
      if (tk == ':') next(); else { printf("%d: conditional missing colon (tk=%d)\n", line, tk); exit(-1); }
      *d = (int)(e + 3); *++e = JMP; d = ++e;
      expr(Cond);
      *d = (int)(e + 1);
    }
    else if (tk == Lor)  { next(); *++e = BNZ; d = ++e; expr(Land); *d = (int)(e + 1); ty = INT; }
    else if (tk == Land) { next(); *++e = BZ;  d = ++e; expr(Or);   *d = (int)(e + 1); ty = INT; }
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
      if ((ty = t) > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  }
      *++e = SUB;
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
  int fd, bt, ty, poolsz, *idmain;
  int *pc;
  int i, tmp; // temps
  char *jitmain, *tje;

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c4x86 [-s] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256*1024; // arbitrary size
  if (!(sym = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; }
  if (!(text = le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; }
  if (!(data = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; }

  memset(sym,  0, poolsz);
  memset(e,    0, poolsz);
  memset(data, 0, poolsz);
  
  p = "char else enum if int return sizeof while "
      "open read close printf malloc memset memcmp memcpy mmap dlsym qsort exit void main";
  i = Char; while (i <= While) { next(); id[Tk] = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { next(); id[Class] = Sys; id[Type] = INT; id[Val] = i++; } // add library to symbol table
  next(); id[Tk] = Char; // handle void type
  next(); idmain = id; // keep track of main

  if (!(lp = p = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }
  if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }
  close(fd);
  p[i] = 0;
  //linemap = (char **)(((int)(p + i + 1) & 0xffffff00) + 0x100);

  //srcmap = text + (poolsz / 8);

  // parse declarations
  line = 1;
  next();
  while (tk) {
    bt = INT; // basetype
    if (tk == Int) next();
    else if (tk == Char) { next(); bt = CHAR; }
    else if (tk == Enum) {
      next();
      if (tk != '{') next();
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
        next(); i = 0;
        while (tk != ')') { // parameter
          ty = INT;
          if (tk == Int) next();
          else if (tk == Char) { next(); ty = CHAR; }
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
        loc = ++i;
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
            id[HVal]   = id[Val];   id[Val] = ++i;
            next();
            if (tk == ',') next();
          }
          next();
        }
        *++e = ENTER; *++e = i - loc;
        while (tk != '}') stmt();
        *++e = LEAVE;
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

  // setup jit memory
//  jitmem = mmap(0, poolsz, 3, 34, -1, 0);
  jitmem = malloc(poolsz);
  if (!jitmem) { printf("could not mmap(%d) jit executable memory\n", poolsz); return -1; }

  // first pass: emit native code
  pc = text + 1; je = jitmem; line = 0;
  while (pc <= e) {
    i = *pc;
    if (src) {
//        while (line < srcmap[pc - text]) {
//            line++; printf("% 4d | %.*s", line, linemap[line + 1] - linemap[line], linemap[line]);
//        }
        printf("0x%05x (%p):\t%8.4s", pc - text, je,
                        &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENTER ,ADJ ,LEAVE ,LI  ,LC  ,SI  ,SC  ,PUSH ,"
                         "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                         "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,MCPY,MMAP,DSYM,QSRT,EXIT,"[i * 5]);
        if (i <= ADJ) printf(" 0x%x\n", *(pc + 1)); else printf("\n");
    }
    *pc++ = ((int)je << 8) | i; // for later relocation of JMP/JSR/BZ/BNZ
    if (i == LEA) {
      i = 4 * *pc++; if (i < -128 || i > 127) { printf("jit: LEA out of bounds\n"); return -1; }
      *(int*)je = 0x458d; je = je + 2; *je++ = i;  // leal $(4 * n)(%ebp), %eax
    }
    else if (i == ENTER) {
      i = 4 * *pc++; if (i < -128 || i > 127) { printf("jit: ENTER out of bounds\n"); return -1; }
      *(int *)je = 0xe58955; je = je + 3;  // push %ebp; movl %esp, %ebp
      if (i > 0) { *(int *)je = 0xec83; je = je + 2; *je++ = i; } // subl $(i*4), %esp
    }
    else if (i == IMM) { *je++ = 0xb8; *(int *)je = *pc++; je = je + 4; } // movl $imm, %eax
    else if (i == ADJ) { i = 4 * *pc++; *(int *)je = 0xc483; je = je + 2; *je++ = i; } // addl $(n * 4), %esp
    else if (i == PUSH)   *je++ = 0x50;                    // push %eax
    else if (i == LEAVE) { *(int *)je = 0xc35dec89; je = je + 4; } // mov %ebp, %esp; pop %ebp; ret
    else if (i == LI)  { *(int *)je = 0x008b;     je = je + 2; } // movl (%eax), %eax
    else if (i == LC)  { *(int *)je = 0x00be0f;   je = je + 3; } // movsbl (%eax), %eax
    else if (i == SI)  { *(int *)je = 0x018959;   je = je + 3; } // pop %ecx; movl %eax, (%ecx)
    else if (i == SC)  { *(int *)je = 0x018859;   je = je + 3; } // pop %ecx; movb %al, (%ecx)
    else if (i == OR)  { *(int *)je = 0xc80959;   je = je + 3; } // pop %ecx; orl %ecx, %eax
    else if (i == XOR) { *(int *)je = 0xc83159;   je = je + 3; } // pop %ecx; xorl %ecx, %eax
    else if (i == AND) { *(int *)je = 0xc82159;   je = je + 3; } // pop %ecx; andl %ecx, %eax
    else if (EQ <= i && i <= GE) {
        *(int*)je=0xb8c13959; je=je+4; *(int*)je=0; je=je+4;           // pop %ecx; cmp %ecx, %eax; mov $0, %eax
        if      (i == EQ)  { *(int*)je = 0xc0940f; } // sete %al
        else if (i == NE)  { *(int*)je = 0xc0950f; } // setne %al
        else if (i == LT)  { *(int*)je = 0xc09c0f; } // setl %al
        else if (i == GT)  { *(int*)je = 0xc09f0f; } // setg %al
        else if (i == LE)  { *(int*)je = 0xc09e0f; } // setle %al
        else               { *(int*)je = 0xc09d0f; } // setge %al
        je = je+3;
    }
    else if (i == SHL) { *(int*)je = 0xe0d39159; je = je + 4; } // pop %ecx; xchg %eax, %ecx; shl %cl, %eax
    else if (i == SHR) { *(int*)je = 0xf8d39159; je = je + 4; } // pop %ecx; xchg %eax, %ecx; sar %cl, %eax
    else if (i == ADD) { *(int*)je = 0xc80159;   je = je + 3; } // pop %ecx; addl %ecx, %eax
    else if (i == SUB) { *(int*)je = 0xc8299159; je = je + 4; } // pop %ecx; xchg %eax, %ecx; subl %ecx, %eax
    else if (i == MUL) { *(int*)je = 0xc1af0f59; je = je + 4; } // pop %ecx; imul %ecx, %eax
    else if (i == DIV) { *(int*)je = 0xf7999159; je = je + 4; *je++ = 0xf9; } // pop %ecx; xchg %eax, %ecx; cltd; idiv %ecx, %eax
    else if (i == MOD) { *(int*)je = 0xf7999159; je = je + 4; *(int *)je = 0x92f9; je = je + 2; } // pop %ecx; xchg %eax, %ecx; cltd; idiv %ecx, %eax; xchg %edx, %eax
    else if (i == JMP) { ++pc; *je       = 0xe9;     je = je + 5; } // jmp <off32>
    else if (i == JSR) { ++pc; *je       = 0xe8;     je = je + 5; } // call <off32>
    else if (i == BZ)  { ++pc; *(int*)je = 0x840fc085; je = je + 8; } // test %eax, %eax; jz <off32>
    else if (i == BNZ) { ++pc; *(int*)je = 0x850fc085; je = je + 8; } // test %eax, %eax; jnz <off32>
    else if (i >= OPEN) {
      if      (i == OPEN) tmp = (int)dlsym(0, "open");   else if (i == READ) tmp = (int)dlsym(0, "read");
      else if (i == CLOS) tmp = (int)dlsym(0, "close");  else if (i == PRTF) tmp = (int)dlsym(0, "printf");
      else if (i == MALC) tmp = (int)dlsym(0, "malloc"); else if (i == MSET) tmp = (int)dlsym(0, "memset");
      else if (i == MCMP) tmp = (int)dlsym(0, "memcmp"); else if (i == MCPY) tmp = (int)dlsym(0, "memcpy");
      else if (i == MMAP) tmp = (int)dlsym(0, "mmap");   else if (i == DSYM) tmp = (int)dlsym(0, "dlsym");
      else if (i == QSRT) tmp = (int)dlsym(0, "qsort");  else if (i == EXIT) tmp = (int)dlsym(0, "exit");
      if (*pc++ == ADJ) { i = *pc++; } else { printf("no ADJ after native proc!\n"); exit(2); }
      *je++ = 0xb9; *(int*)je = i << 2; je = je + 4;  // movl $(4 * n), %ecx;
      *(int*)je = 0xce29e689; je = je + 4; // mov %esp, %esi; sub %ecx, %esi;  -- %esi will adjust the stack
      *(int*)je = 0x8302e9c1; je = je + 4; // shr $2, %ecx; and                -- alignment of %esp for OS X
      *(int*)je = 0x895af0e6; je = je + 4; // $0xfffffff0, %esi; pop %edx; mov..
      *(int*)je = 0xe2fc8e54; je = je + 4; // ..%edx, -4(%esi,%ecx,4); loop..  -- reversing args order
      *(int*)je = 0xe8f487f9; je = je + 4; // ..<'pop' offset>; xchg %esi, %esp; call    -- saving old stack in %esi
      *(int*)je = tmp - (int)(je + 4); je = je + 4; // <*tmp offset>;
      *(int*)je = 0xf487; je = je + 2;     // xchg %esi, %esp  -- ADJ, back to old stack without arguments
    }
    else { printf("code generation failed for %d!\n", i); return -1; }
  }
  tje = je;
  
  // second pass, relocation
  pc = text + 1;
  while (pc <= e) {
    i = *pc & 0xff;
    je = (char*)(((*pc++ >> 8) & 0x00ffffff) | ((int)jitmem & 0xff000000)); // MSB is restored from jitmem
    if (i == JSR || i == JMP || i == BZ || i == BNZ) {
        tmp = ((*(int *)(*pc++) >> 8) & 0x00ffffff) | ((int)jitmem & 0xff000000); // extract address
        if      (i == JSR || i == JMP) { je = je + 1; *(int*)je = tmp - (int)(je + 4); }
        else if (i == BZ  || i == BNZ) { je = je + 4; *(int*)je = tmp - (int)(je + 4); }
    }
    else if (i < LEAVE) { ++pc; }
  }

  if (!src) {
    je = tje;
    jitmain = (char *)(((*(int *)(idmain[Val]) >> 8) & 0x00ffffff) | ((int)jitmem & 0xff000000));
    *je++ = 0x56;                                                     // push %esi
    *je++ = 0xb8; *(int *)   je = argc; je = je+4; *je++ = 0x50;      // movl $argc, %eax; push %eax
    *je++ = 0xb8; *(char ***)je = argv; je = je+4; *je++ = 0x50;      // movl $argv, %eax; push %eax
    *je++ = 0xe8; *(int *)je = (int)jitmain - (int)je - 4; je = je+4; // call main
    *je++ = 0x83; *je++ = 0xc4; *je++ = 8;                            // add $8, %esp
    *je++ = 0x5e;                                                     // pop %esi
    *je++ = 0xc3;                                                     // ret
    qsort(sym, 2, 1, (void *)tje); // hack to call a function pointer
  }
  return 0;
}
