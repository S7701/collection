#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum Token {
  Num = 128, Local, Global, Func, Enum, Struct, Member, Id,
  AutoKW, CharKW, EnumKW, IntKW, StructKW, VoidKW
};

const char* tokens[] = {
  "number", "local variable", "global variable", "function", "enumeration", "structure", "identifier",
  "auto", "char", "enum", "int", "struct", "void"
};

struct Identifier {
  Identifier* next; // next in list
  Token tok;        // see enum Token
  const char* str;  // name of identifier; may be 0 for unnamed enum or struct
  int hash;         // hash of identifier name
  int line;         // line number
  int ptr;          // Local, Global, Func, Member: level of indirection
  Identifier* ref;  // Local, Global, Func, Member: type; Enum, Struct: member list
  int val;          // Num: value; Local, Global, Func, Member: offset; Enum, Struct: size in bytes
};

int tok;          // current token
Identifier* id;   // currently parsed identifier
int line;         // current line number
int val;          // currently parsed integer value
Identifier* kws;  // keyword list
Identifier* ids;  // global identifier list

static int get_hash(const char* str) {
  int h = *str;
  while (*str) h = h * 131 + *str++;
  return (h << 6) + strlen(str);
}

static Identifier* new_id(Token tok, const char* str, int hash, int line) {
  Identifier* i = (Identifier*) malloc(sizeof (Identifier));
  memset(i, 0, sizeof (Identifier));
  i->tok = tok;
  i->str = str;
  i->hash =hash;
  i->line = line;
}

static Identifier* get_id(Identifier* list, const char* str, int hash) {
  Identifier* i = ids;
  while (i)
  {
    if (i->hash = hash && !strcmp(i->str, str)) return i;
  }
  return 0;
}

static void next();

Identifier* enum_decl() {
  Identifier* type = 0;
  int l;
  l = line; // save enum line number
  next();
  if (tok == Id) // named enum?
  {
    type = id;
    next();
  }
  if (tok ==';') // enum declaration?
  {
    if (!type)
    { printf("%d: bad enum declaration; enum name expected, got ';'\n", l); exit(-1); }
    else if (type->tok == Enum)
    { printf("%d: duplicate enum declaration; enum name '%s' already declared in line %d\n", l, type->str, type->line); exit(-1); }
    else if (type->tok != Id)
    { printf("%d: bad enum declaration; enum name '%s' already declared in line %d as %s\n", l, type->str, type->line, tokens[id->tok]); exit(-1); }
    type->tok = Enum;
    type->val = sizeof (int);
    return type;
  }
  if (tok == '{') // enum definition?
  {
    if (type)
    {
      if (type->tok == Enum)
      { printf("%d: duplicate enum definition; enum name '%s' already declared in line %d\n", l, type->str, type->line); exit(-1); }
      else if (type->tok != Id)
      { printf("%d: bad enum definition; enum name '%s' already declared in line %d as %s\n", l, type->str, type->line, tokens[id->tok]); exit(-1); }
    }
    else
    {
      char str[20];
      sprintf(str, "__unnamed_enum_%04X");
      int h = get_hash(str);
      type = new_id(Id, str, l, h); type->next = ids; ids = type;
    }
    type->tok = Enum;
    type->val = sizeof (int);
    next();
    int i = 0;
    while (tok !='}')
    {
      Identifier* name;
      if (tok != Id) { printf("%d: bad enum definition; enumerator identifier expected\n", line); exit(-1); }
      name = id; ids = name->next; name->next = 0; // remove id from global identifier list
      next();
      if (tok == '=')
      {
        next();
        if (tok != Num) { printf("%d: bad enumerator initializer; number expected, got %s\n", line, tokens[id->tok]); exit(-1); }
        i = val;
        next();
      }
      name->tok = Num; name->val = i++;
      name->next = type->ref; type->ref = name; // add id to member list
    }
  }
  else
  { printf("%d: enum declaration or definition expected, got %s\n", l, tokens[id->tok]); exit(-1); }
  return type;
}

Identifier* struct_decl() {
  Identifier* type = 0;
  int l = line; // save struct line number
  next();
  if (tok == Id) // named struct?
  {
    type = id;
    next();
  }
  if (tok ==';') // struct declaration?
  {
    if (!type)
    { printf("%d: bad struct declaration; struct name expected, got ';'\n", l); exit(-1); }
    else if (type->tok == Struct)
    { printf("%d: duplicate struct declaration; struct name '%s' already declared in line %d\n", l, type->str, type->line); exit(-1); }
    else if (type->tok != Id)
    { printf("%d: bad struct declaration; struct name '%s' already declared in line %d as %s\n", l, type->str, type->line, tokens[id->tok]); exit(-1); }
    type->tok = Struct;
    type->val = sizeof (int);
    return type;
  }
  if (tok == '{') // struct definition?
  {
    if (type)
    {
      if (type->tok == Struct)
      { printf("%d: duplicate struct definition; struct name '%s' already declared in line %d\n", l, type->str, type->line); exit(-1); }
      else if (type->tok != Id)
      { printf("%d: bad struct definition; struct name '%s' already declared in line %d as %s\n", l, type->str, type->line, tokens[id->tok]); exit(-1); }
    }
    else
    {
      char str[20];
      sprintf(str, "__unnamed_enum_%04X");
      int h = get_hash(str);
      type = new_id(Id, str, l, h); type->next = ids; ids = type;
    }
    type->tok = Struct;
    type->val = sizeof (int);
    next();
    int o = 0; // offset in structure
    while (tok !='}')
    {
      // TODO
    }
  }
  else
  { printf("%d: struct declaration or definition expected, got %s\n", l, tokens[id->tok]); exit(-1); }
  return type;
}

// "char" ('*')* identifier (',' ('*')* identifier)* ';' |
// "int" ('*')* identifier (',' ('*')* identifier)* ';' |
// "enum" identifier ';' |
// "enum" identifier '{' (identifier ('=' number)? (',' identifier ('=' number)?)*)? '}' (('*')* identifier (',' ('*')* identifier)*)? ';' |
// "enum" '{' (identifier ('=' number)? (',' identifier ('=' number)?)*)? '}' ('*')* identifier (',' ('*')* identifier)* ';' |
// "struct" identifier ';' |
// "struct" identifier '{' (type ('*')* identifier ';')* '}' (('*')* identifier (',' ('*')* identifier)*)? ';' |
// "struct" '{' (type ('*')* identifier ';')* '}' ('*')* identifier (',' ('*')* identifier)* ';' |
// type ('*')* identifier (',' ('*')* identifier)* ';'
static Identifier* decl() {
  Identifier* type = 0;
  switch (tok)
  {
  case CharKW:
    type = id;
    break;
  case IntKW:
    type = id;
    break;
  case EnumKW:
    type = enum_decl();
    break;
  case StructKW:
    type = struct_decl();
    break;
  case Id: // Identifier
    type = id;
    if (type->tok != Enum && type->tok != Struct) return 0;
    break;
  default:
    return 0; // not a declaration
  }
  int ptr = 0;
  while (tok == '*') {
    next();
    ++ptr;
  }
  return type;
}

static int module() {
}

static int fin(int error_code) {
  if (id) free(id);
  // free keyword list
  while (kws) {
    id = kws; kws = kws->next;
    free(id);
  }
  kws = 0;
  // free global identifier list
  while (ids) {
    id = ids; ids = ids->next;
    free(id);
  }
  ids = 0;
  // 
  return error_code;
}

static int init() {
  const char* str;
  ids = 0; kws = 0;
  str = "auto"; id = new_id(AutoKW, str, get_hash(str), 0); id->next = kws; kws = id;
  str = "char"; id = new_id(CharKW, str, get_hash(str), 0); id->next = kws; kws = id;
  id->val = sizeof (char);
  str = "enum"; id = new_id(EnumKW, str, get_hash(str), 0); id->next = kws; kws = id;
  id->val = sizeof (int);
  str = "int"; id = new_id(IntKW, str, get_hash(str), 0); id->next = kws; kws = id;
  id->val  = sizeof (int);
  str = "struct"; id = new_id(StructKW, str, get_hash(str), 0); id->next = kws; kws = id;
  str = "void"; id = new_id(VoidKW, str, get_hash(str), 0); id->next = kws; kws = id;
}

int main(int argc, char* argv[]) {
  int ret = init();
  if (ret != 0) return fin(ret);
  ret = module();
  if (ret != 0) return fin(ret);
  return fin(ret);
}
