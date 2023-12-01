// char, int, pointer types and enums
// if, while, return and expression statements
// no enum names allowed
// no assignments in expressions allowed

#include <stdio.h>  // for printf()
#include <stdlib.h> // for malloc(), free(), exit()
#include <memory.h> // for memset(), memcmp()
#include <fcntl.h>  // for open()
#include <unistd.h> // for read(), write(), close()

#define int long // for 64 bit

char *p, *lp, // current/last position in source code
     *d,      // data/bss pointer
     *ops;    // opcodes

int *e, *le,  // current/last position in emitted code
    *id,      // last parsed identifier
    *sym,     // symbol table (simple list of identifiers)
    tk,       // current token
    val,      // last parsed token value for number, character or string
    ty,       // current expression type
    loc,      // local variable offset
    line,     // current line number
    src,      // print source and assembly flag
    dbg;      // print executed instructions

// tokens and classes (operators last and in precedence order)
enum { Num = 128, Fun, Sys, Global, Local, Id,
       Char, Else, Enum, If, Int, Return, Sizeof, While,
       Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Bracket };

// opcodes (IMM...ENTER have parameter)
enum { IMM, LEA, JMP, BZ, BNZ, JSR, ADJ, ENTER, LEAVE, LI, LC, SI, SC, PUSH, SWAP,
       OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
       OPEN, READ, WRITE, CLOSE, PRINTF, MALLOC, FREE, MEMSET, MEMCPY, MEMCMP, EXIT };

// types
enum { CHAR, INT, PTR };

// identifier offsets (since we can't create an ident struct)
enum { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, IdSz };

void next() {
  char *pp; // previous position

  while (*p) {
    tk = *p++;
    if (tk == '\n') { // end of line
      if (src) {
        pp = p - 1;
        *pp = '\0'; // terminate current line
        printf("%ld: %s\n", line, lp);
        lp = p;
        while (le < e) {
          printf("    %s", &ops[*++le * 8]);
          if (*le <= ENTER) { ++le; printf(" %ld (0x%lX)\n", *le, *le); }
          else printf("\n");
        }
      }
      ++line;
    }
    else if (tk == '#') { // treat preprocessor lines as line comments
      while (*p != 0 && *p != '\n')
        ++p;
    }
    else if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_') { // identifier
      pp = p - 1;
      while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        tk = tk * 147 + *p++;
      tk = (tk << 6) + (p - pp);
      id = sym;
      while (id[Tk]) {
        if (tk == id[Hash] && !memcmp((char*)id[Name], pp, p - pp)) { tk = id[Tk]; return; }
        id = id + IdSz;
      }
      id[Name] = (int)pp;
      id[Hash] = tk;
      tk = id[Tk] = Id;
      return;
    }
    else if (tk >= '0' && tk <= '9') { // number
      val = tk - '0';
      if (val) { // decimal number
        while (*p >= '0' && *p <= '9')
          val = val * 10 + *p++ - '0';
      }
      else if (*p == 'x' || *p == 'X') { // hexadecimal number
        tk = *++p;
        while ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F')) {
          val = val * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
          tk = *++p;
        }
      }
      else { // octal number
        while (*p >= '0' && *p <= '7')
          val = val * 8 + *p++ - '0';
      }
      tk = Num;
      return;
    }
    else if (tk == '/') { // line comment or operator /
      if (*p == '/') {
        ++p;
        while (*p != 0 && *p != '\n')
          ++p;
      }
      else {
        tk = Div;
        return;
      }
    }
    else if (tk == '\'' || tk == '"') { // character or string literal
      pp = d;
      while (*p != 0 && *p != tk) {
        val = *p++;
        if (val == '\\') {
          val = *p++;
          if (val == '0') val = '\0';
          else if (val == 't') val = '\t';
          else if (val == 'v') val = '\v';
          else if (val == 'f') val = '\f';
          else if (val == 'r') val = '\r';
          else if (val == 'n') val = '\n';
        }
        if (tk == '"') *d++ = (char)val;
      }
      ++p; // skip delimiter
      if (tk == '"') val = (int)pp;
      else tk = Num;
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
  tk = 0;
}

void expr(int lev) {
  int *pp, t;

  // unary expression
  if (!tk) { printf("%ld: unexpected eof in expression\n", line); exit(-1); }
  else if (tk == Num) { *++e = IMM; *++e = val; next(); ty = INT; } // number or character literal
  else if (tk == '"') { // string literal
    *++e = IMM; *++e = val;
    next(); // skip string literal
    while (tk == '"')
      next(); // concatenate string literals
    d = (char*)((int)d + sizeof (int) & -sizeof (int)); // align data pointer; reserve at least 1 byte for '\0'
    ty = PTR; // char pointer
  }
  else if (tk == Sizeof) { // sizeof operator
    next(); // skip "sizeof"
    if (tk == '(') next(); // '(' expected
    else { printf("%ld: '(' expected in sizeof expression (tk=%ld)\n", line, tk); exit(-1); }
    ty = INT;
    if (tk == Int) next(); // integer
    else if (tk == Char) { next(); ty = CHAR; } // character
    while (tk == Mul) { // pointer
      next(); // skip '*'
      ty = ty + PTR;
    }
    if (tk == ')') next(); // ')' expected
    else { printf("%ld: ')' expected in sizeof expression (tk=%ld)\n", line, tk); exit(-1); }
    *++e = IMM; *++e = (ty == CHAR) ? sizeof (char) : sizeof (int);
    ty = INT;
  }
  else if (tk == Id) { // identifier
    pp = id;
    next(); // skip identifier
    if (tk == '(') { // function call
      next(); // skip '('
      t = 0; // parameter counter
      while (tk != ')') { // parameter list
        expr(Cond);
        *++e = PUSH;
        ++t;
        if (tk == ',') next(); // ',' or ')' expected
        else if (tk != ')') { printf("%ld: bad function call (tk=%ld)\n", line, tk); exit(-1); }
      }
      next(); // skip ')'
      if (pp[Class] == Sys) *++e = pp[Val];
      else if (pp[Class] == Fun) { *++e = JSR; *++e = pp[Val]; }
      else { printf("%ld: bad function call (class=%ld)\n", line, pp[Class]); exit(-1); }
      if (t) { *++e = ADJ; *++e = t; } // adjust stack after call
      ty = pp[Type];
    }
    else if (pp[Class] == Num) { *++e = IMM; *++e = pp[Val]; ty = INT; } // enum identifier
    else { // variable
      if (pp[Class] == Local) { *++e = LEA; *++e = loc - pp[Val]; } // local variable
      else if (pp[Class] == Global || pp[Class] == Fun) { *++e = IMM; *++e = pp[Val]; }  // global variable or function pointer
      else { printf("%ld: undefined variable (class=%ld)\n", line, pp[Class]); exit(-1); }
      ty = pp[Type];
      *++e = (ty == CHAR) ? LC : LI;
    }
  }
  else if (tk == '(') { // type cast or group
    next(); // skip '('
    if (tk == Int || tk == Char) { // type cast
      t = (tk == Int) ? INT : CHAR;
      next(); // skip type
      while (tk == Mul) {
        next();
        t = t + PTR;
      }
      if (tk == ')') next(); // ')' expected
      else { printf("%ld: bad cast (tk=%ld)\n", line, tk); exit(-1); }
      expr(Inc);
      ty = t;
    }
    else { // group
      expr(Cond);
      if (tk == ')') next(); // ')' expected
      else { printf("%ld: ')' expected (tk=%ld)\n", line, tk); exit(-1); }
    }
  }
  else if (tk == Mul) { // unary dereference operator
    next(); // skip '*'
    expr(Inc);
    if (ty > INT) ty = ty - PTR; // 
    else { printf("%ld: bad dereference (ty=%ld)\n", line, ty); exit(-1); }
    *++e = (ty == CHAR) ? LC : LI;
  }
  else if (tk == And) { // unary address-of operator
    next(); // skip '&'
    expr(Inc);
    if (*e == LC || *e == LI) --e; // 
    else { printf("%ld: bad address-of (instruction=%ld)\n", line, *e); exit(-1); }
    ty = ty + PTR;
  }
  else if (tk == '!') { next(); expr(Inc); *++e = PUSH; *++e = IMM; *++e =  0; *++e = EQ;  ty = INT; } // logical not operator
  else if (tk == '~') { next(); expr(Inc); *++e = PUSH; *++e = IMM; *++e = -1; *++e = XOR; ty = INT; } // binary not operator
  else if (tk == Add) { next(); expr(Inc); ty = INT; } // unary plus operator
  else if (tk == Sub) { // unary minus operator
    next(); // skip '-'
    *++e = IMM;
    if (tk == Num) { *++e = -val; next(); } // negative number
    else { *++e = 0; *++e = PUSH; expr(Inc); *++e = SUB; } // 0 SUB <expr>
    ty = INT;
  }
  else if (tk == Inc || tk == Dec) { // pre-increment/-decrement
    t = tk;
    next(); // skip "++" or "--"
    expr(Inc);
    if (*e == LC) { *e = PUSH; *++e = LC; }
    else if (*e == LI) { *e = PUSH; *++e = LI; }
    else { printf("%ld: bad lvalue in pre-increment/-decrement (instruction=%ld)\n", line, *e); exit(-1); }
    *++e = PUSH;
    *++e = IMM; *++e = (ty > PTR) ? sizeof (int) : sizeof (char);
    *++e = (t == Inc) ? ADD : SUB;
    *++e = (ty == CHAR) ? SC : SI;
  }
  else { printf("%ld: bad expression (tk=%ld)\n", line, tk); exit(-1); }

  // binary or postfix expression
  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    t = ty;
    if (tk == Assign) {
      next(); // skip '='
      if (*e == LC || *e == LI) *e = PUSH;
      else { printf("%ld: bad lvalue in assignment (instruction=%ld)\n", line, *e); exit(-1); }
      expr(Assign);
      *++e = (t == CHAR) ? SC : SI;
      ty = t;
    }
    else if (tk == Cond) {
      next(); // skip '?'
      *++e = BZ; pp = ++e;
      expr(Cond);
      if (tk == ':') next();
      else { printf("%ld: conditional missing colon (tk=%ld)\n", line, tk); exit(-1); }
      *pp = (int)(e + 3);
      *++e = JMP;
      pp = ++e;
      expr(Cond);
      *pp = (int)(e + 1);
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
    else if (tk == Add)  { // imm + imm -> imm; ptr + imm -> ptr; imm + ptr -> ptr; ptr + ptr (not allowed)
      next(); // skip '+'
      *++e = PUSH;
      expr(Mul);
      if (ty >= PTR) { // pointer on right side
        if (t < PTR) { *++e = SWAP; t = ty; } // value on left side, pointer on right side -> swap
        else { printf("%ld: bad pointer addition\n", line); exit(-1); }
      }
      if (t > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof (int); *++e = MUL; } // adjust value for int pointer or pointer pointer
      *++e = ADD; // add value to value, value to pointer, value to pointer pointer
      ty = t;
    }
    else if (tk == Sub) { // imm - imm -> imm; ptr - imm -> ptr; ptr - ptr -> imm; imm - ptr (not allowed)
      next(); // skip '-'
      *++e = PUSH;
      expr(Mul);
      if (t < PTR && ty >= PTR) { printf("%ld: bad pointer subtraction\n", line); exit(-1); }
      if (t > PTR && t == ty) { *++e = SUB; *++e = PUSH; *++e = IMM; *++e = sizeof (int); *++e = DIV; ty = INT; } // subtract two int pointers or two pointer pointers
      else if (t > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof (int); *++e = MUL; *++e = SUB; } // subtract value from int pointer or pointer pointer
      else *++e = SUB; // subtract two values, value from char pointer or two char pointers
      ty = t;
    }
    else if (tk == Mul) { next(); *++e = PUSH; expr(Inc); *++e = MUL; ty = INT; }
    else if (tk == Div) { next(); *++e = PUSH; expr(Inc); *++e = DIV; ty = INT; }
    else if (tk == Mod) { next(); *++e = PUSH; expr(Inc); *++e = MOD; ty = INT; }
    else if (tk == Inc || tk == Dec) { // post-increment/-decrement
      if (*e == LC) { *e = PUSH; *++e = LC; }
      else if (*e == LI) { *e = PUSH; *++e = LI; }
      else { printf("%ld: bad lvalue in post-increment/-decrement (instruction=%ld)\n", line, *e); exit(-1); }
      *++e = PUSH;
      *++e = IMM; *++e = (ty > PTR) ? sizeof (int) : sizeof (char);
      *++e = (tk == Inc) ? ADD : SUB;
      *++e = (ty == CHAR) ? SC : SI;
      *++e = PUSH;
      *++e = IMM; *++e = (ty > PTR) ? sizeof (int) : sizeof (char);
      *++e = (tk == Inc) ? SUB : ADD;
      next(); // skip "++" or "--"
    }
    else if (tk == Bracket) {
      next();
      *++e = PUSH;
      expr(Cond);
      if (tk == ']') next();
      else { printf("%ld: ']' expected (tk=%ld)\n", line, tk); exit(-1); }
      if (ty >= PTR) { printf("%ld: bad array subscript; non pointer type expected (ty=%ld)\n", line, ty); exit(-1); }
      if (t < PTR) { printf("%ld: bad lvalue in array subscript; pointer type expected (t=%ld)\n", line, t); exit(-1); }
      if (t > PTR) { *++e = PUSH; *++e = IMM; *++e = sizeof (int); *++e = MUL;  }
      ty = t - PTR;
      *++e = ADD;
      *++e = (ty == CHAR) ? LC : LI;
    }
    else { printf("%ld: bad expression (tk=%ld)\n", line, tk); exit(-1); }
  }
}

void stmt() {
  int *a, *b;

  if (tk == If) { // if statement
    next(); // skip "if"
    if (tk != '(') { printf("%ld: '(' expected in if statement (tk=%ld)\n", line, tk); exit(-1); }
    next(); // skip '('
    expr(Cond);
    if (tk != ')') { printf("%ld: ')' expected in if statement (tk=%ld)\n", line, tk); exit(-1); }
    next(); // skip ')'
    *++e = BZ; b = ++e;
    stmt();
    if (tk == Else) { // else
      next(); // skip "else"
      *b = (int)(e + 3);
      *++e = JMP; b = ++e;
      stmt();
    }
    *b = (int)(e + 1);
  }
  else if (tk == While) { // while statement
    next(); // skip "while"
    a = e + 1;
    if (tk != '(') { printf("%ld: '(' expected in while statement (tk=%ld)\n", line, tk); exit(-1); }
    next(); // skip '('
    expr(Cond);
    if (tk != ')') { printf("%ld: ')' expected in while statement (tk=%ld)\n", line, tk); exit(-1); }
    next(); // skip ')'
    *++e = BZ; b = ++e;
    stmt();
    *++e = JMP; *++e = (int)a;
    *b = (int)(e + 1);
  }
  else if (tk == Return) { // return statement
    next(); // skip "return"
    if (tk != ';')
      expr(Cond);
    if (tk != ';') { printf("%ld: ';' expected in return statement (tk=%ld)\n", line, tk); exit(-1); }
    next(); // skip ';'
    *++e = LEAVE;
  }
  else if (tk == '{') { // block statement
    next(); // skip '{'
    while (tk != '}')
      stmt();
    next(); // skip '}'
  }
  else if (tk == ';') { // empty statement
    next(); // skip ';'
  }
  else { // expr stmt
    expr(Assign);
    if (tk != ';') { printf("%ld: ';' expected in statement (tk=%ld)\n", line, tk); exit(-1); }
    next(); // skip ';'
  }
}

int main(int argc, char **argv) {
  int fd, bt, poolsz, *idmain;
  int *pc, *sp, *bp, a, cycle; // vm registers
  int i, *pp; // temps

  if (sizeof (int) != sizeof (int*)) { printf("FATAL: (sizeof (int): %ld) != (sizeof (int*): %ld)\n", sizeof (int), sizeof (int*)); return -1; }

  --argc; ++argv;
  while (argc > 0 && (*argv)[0] == '-') {
    if ((*argv)[1] == 's') src = 1;
    else if ((*argv)[1] == 'd') dbg = 1;
    --argc; ++argv;
  }
  if (argc < 1) { printf("usage: cc [-s] [-d] file ...\n"); return -1; }

  poolsz = 256 * 1024; // arbitrary size
  sym = malloc(poolsz);
  if (!sym) { printf("could not malloc(%ld) symbol area\n", poolsz); return -1; }
  e = le = malloc(poolsz);
  if (!e) { printf("could not malloc(%ld) text area\n", poolsz); return -1; }
  d = malloc(poolsz);
  if (!d) { printf("could not malloc(%ld) data area\n", poolsz); return -1; }
  sp = malloc(poolsz);
  if (!sp) { printf("could not malloc(%ld) stack area\n", poolsz); return -1; }

  memset(sym, 0, poolsz);
  memset(e,   0, poolsz);
  memset(d,   0, poolsz);
  memset(sp,  0, poolsz);

  ops = "IMM\0    LEA\0    JMP\0    BZ\0     BNZ\0    JSR\0    ADJ\0    ENTER\0  LEAVE\0  LI\0     LC\0     SI\0     SC\0     PUSH\0   SWAP\0   "
        "OR\0     XOR\0    AND\0    EQ\0     NE\0     LT\0     GT\0     LE\0     GE\0     SHL\0    SHR\0    ADD\0    SUB\0    MUL\0    DIV\0    MOD\0    "
        "OPEN\0   READ\0   WRITE\0  CLOSE\0  PRINTF\0 MALLOC\0 FREE\0   MEMSET\0 MEMCPY\0 MEMCMP\0 EXIT\0   ";

  p = "char else enum if int return sizeof while " // keywords
      "open read write close printf malloc free memset memcpy memcmp exit " // library functions
      "void main";

  // add keywords to symbol table
  i = Char;
  while (i <= While) {
    next();
    id[Tk] = i++;
  }
  // add library functions to symbol table
  i = OPEN;
  while (i <= EXIT) {
    next();
    id[Class] = Sys;
    id[Type] = INT;
    id[Val] = i++;
  }
  // handle void type
  next();
  id[Tk] = Char;
  // keep track of main
  next();
  idmain = id;

  p = lp = malloc(poolsz); // must be allocated after initialization of keywords and library functions
  if (!p) { printf("could not malloc(%ld) source area\n", poolsz); return -1; }

  fd = open(*argv, 0);
  if (fd < 0) { printf("could not open(%s)\n", *argv); return -1; }
  i = read(fd, p, poolsz-1);
  if (i <= 0) { printf("read() returned %ld\n", i); return -1; }
  if (i >= poolsz) { printf("source too large; limit: %ld\n", poolsz-1); return -1; }
  p[i] = 0;
  close(fd);

  // parse declarations
  line = 1;
  next();
  while (tk) {
    bt = INT; // basetype
    if (tk == Int) next();
    else if (tk == Char) { next(); bt = CHAR; }
    else if (tk == Enum) { // enum declaration
      next(); // skip "enum"
      if (tk != '{') { printf("%ld: bad enum declaration; '{' expected (tk=%ld)\n", line, tk); return -1; }
      next(); // skip '{'
      i = 0;
      while (tk != '}') {
        if (tk != Id) { printf("%ld: bad enum declaration; enumerator expected (tk=%ld)\n", line, tk); return -1; }
        pp = id;
        next();
        if (tk == Assign) {
          next(); // skip '='
          if (tk == Id) {
            if(id[Class] == Num) i = id[Val];
            else { printf("%ld: bad enum initializer; invalid enumerator (class=%ld)\n", line, id[Class]); return -1; }
          }
          else if (tk == Num) i = val;
          else { printf("%ld: bad enum initializer; number or enumerator expected (tk=%ld)\n", line, tk); return -1; }
          next();
        }
        pp[Class] = Num;
        pp[Type] = INT;
        pp[Val] = i++;
        if (tk == ',') next();
        else if (tk != '}') { printf("%ld: bad enum declaration; ',' or '}' expected (tk=%ld)\n", line, tk); return -1; }
      }
      next(); // skip '}'
    }
    else { printf("%ld: global declaration expected (tk=%ld)\n", line, tk); return -1; }

    while (tk != ';') {
      ty = bt;
      while (tk == Mul) {
        next();
        ty = ty + PTR;
      }
      if (tk != Id) { printf("%ld: bad global declaration; identifier expected (tk=%ld)\n", line, tk); return -1; }
      if (id[Class]) { printf("%ld: duplicate global declaration (class=%ld)\n", line, id[Class]); return -1; }
      id[Type] = ty;
      next();
      if (tk == '(') { // function declaration
        id[Class] = Fun;
        id[Val] = (int)(e + 1);
        next();
        i = 0; // offset
        while (tk != ')') { // parameter list
          ty = INT;
          if (tk == Int) next();
          else if (tk == Char) { next(); ty = CHAR; }
          else { printf("%ld: bad parameter type (tk=%ld)\n", line, tk); return -1; }
          while (tk == Mul) {
            next();
            ty = ty + PTR;
          }
          if (tk != Id) { printf("%ld: bad parameter declaration; identifier expected (tk=%ld)\n", line, tk); return -1; }
          if (id[Class] == Local) { printf("%ld: duplicate parameter declaration\n", line); return -1; }
          id[HClass] = id[Class];
          id[Class] = Local;
          id[HType] = id[Type];
          id[Type] = ty;
          id[HVal] = id[Val];
          id[Val] = i++;
          next();
          if (tk == ',') next();
          else if (tk != ')') { printf("%ld: bad function declaration; ',' or '}' expected (tk=%ld)\n", line, tk); return -1; }
        }
        next(); // skip ')'
        if (tk != '{') { printf("%ld: bad function declaration; '{' expected (tk=%ld)\n", line, tk); return -1; }
        next();
        loc = ++i;
        while (tk == Int || tk == Char) { // local variable declarations
          bt = (tk == Int) ? INT : CHAR;
          next();
          while (tk != ';') {
            ty = bt;
            while (tk == Mul) {
              next();
              ty = ty + PTR;
            }
            if (tk != Id) { printf("%ld: bad local declaration; identifier expected (tk=%ld)\n", line, tk); return -1; }
            if (id[Class] == Local) { *p = '\0'; printf("%ld: duplicate local declaration\n", line); return -1; }
            id[HClass] = id[Class];
            id[Class] = Local;
            id[HType] = id[Type];
            id[Type] = ty;
            id[HVal] = id[Val];
            id[Val] = ++i;
            next();
            if (tk == ',') next();
            else if (tk != ';') { printf("%ld: bad local declaration; ',' or ';' expected (tk=%ld)\n", line, tk); return -1; }
          }
          next(); // skip ';'
        }
        *++e = ENTER; *++e = i - loc;
        while (tk != '}')
          stmt();
        *++e = LEAVE;
        // unwind symbol table locals
        id = sym;
        while (id[Tk]) {
          if (id[Class] == Local) {
            id[Class] = id[HClass];
            id[Type]  = id[HType];
            id[Val]   = id[HVal];
          }
          id = id + IdSz;
        }
        tk = ';'; // break inner while loop after function declaration
      }
      else { // global variable declaration
        id[Class] = Global;
        id[Val] = (int)d;
        d = d + sizeof (int);
      }

      if (tk == ',') next();
      else if (tk != ';') { printf("%ld: bad global declaration; ',' or ';' expected (tk=%ld)\n", line, tk); return -1; }
    }
    next(); // skip ';'
  }

  pc = (int*)idmain[Val];
  if (!pc) { printf("main() not defined\n"); return -1; }
  if (src) return 0;

  // call exit when main returns
  *++e = PUSH;
  pp = e;
  *++e = EXIT;

  // setup stack
  sp = bp = (int*)((int)sp + poolsz);
  *--sp = argc;
  *--sp = (int)argv;
  *--sp = (int)pp;

  // run...
  a = cycle = 0;
  while (1) {
    ++cycle;
    i = *pc++;

    if (dbg) {
      printf("%ld> %s", cycle, &ops[i * 8]);
      if (i <= ENTER) printf(" %ld (0x%lX)\n", *pc, *pc);
      else printf("\n");
    }

    if      (i == IMM)   a = *pc++;                                       // load global address or immediate
    else if (i == LEA)   a = (int)(bp + *pc++);                           // load local address
    else if (i == JMP)   pc = (int*)*pc;                                  // jump
    else if (i == BZ)    pc = a ? pc + 1 : (int*)*pc;                     // branch if zero
    else if (i == BNZ)   pc = a ? (int*)*pc : pc + 1;                     // branch if not zero
    else if (i == JSR)   { *--sp = (int)(pc + 1); pc = (int*)*pc; }       // jump to subroutine
    else if (i == ADJ)   sp = sp + *pc++;                                 // stack adjust
    else if (i == ENTER) { *--sp = (int)bp; bp = sp; sp = sp - *pc++; }   // enter subroutine
    else if (i == LEAVE) { sp = bp; bp = (int*)*sp++; pc = (int*)*sp++; } // leave subroutine
    else if (i == LI)    a = *(int*)a;                                    // load int
    else if (i == LC)    a = *(char*)a;                                   // load char
    else if (i == SI)    *(int*)*sp++ = a;                                // store int
    else if (i == SC)    a = *(char*)*sp++ = (char)a;                     // store char
    else if (i == PUSH)  *--sp = a;                                       // push
    else if (i == SWAP)  { i = *sp; *sp = a; a = i; }                     // swap

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

    else if (i == OPEN)   a = open((char*)sp[1], *sp);
    else if (i == READ)   a = read(sp[2], (char*)sp[1], *sp);
    else if (i == WRITE)  a = write(sp[2], (char*)sp[1], *sp);
    else if (i == CLOSE)  a = close(*sp);
    else if (i == PRINTF) {
      if (*pc == ADJ) pp = sp + pc[1]; else { printf("bad instruction sequence; expected ADJ after PRINTF (instruction=%ld)\n", *pc); return -1; }
      a = printf((char*)pp[-1], pp[-2], pp[-3], pp[-4], pp[-5], pp[-6], pp[-7], pp[-8]);
    }
    else if (i == MALLOC) a = (int)malloc(*sp);
    else if (i == FREE)   { a = *sp; free((void *)a); }
    else if (i == MEMSET) a = (int)memset((char*)sp[2], sp[1], *sp);
    else if (i == MEMCPY) a = (int)memcpy((char*)sp[2], (char*)sp[1], *sp);
    else if (i == MEMCMP) a = memcmp((char*)sp[2], (char*)sp[1], *sp);
    else if (i == EXIT)   { printf("exit(%ld), cycle = %ld\n", *sp, cycle); return *sp; }
    else { printf("unknown instruction %ld! cycle = %ld\n", i, cycle); return -1; }
  }

  return -1;
}
