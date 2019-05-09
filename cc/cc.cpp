#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum Token {
  Num = 128, Local, Global, Func, Enum, Struct, Member, Id,
  AutoKW, CharKW, EnumKW, IntKW, StructKW
};

const char* tokens[] = {
  "number", "local variable", "global variable", "function", "enumeration", "structure", "identifier",
  "auto", "char", "enum", "int", "struct"
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

int tok;         // current token
Identifier* id;  // currently parsed identifier
int ival;        // currently parsed integer value
int line;        // current line number
Identifier* ids;      // global identifier list
Identifier* idAuto;   // identifier for keyword 'auto'
Identifier* idChar;   // identifier for keyword 'char'
Identifier* idEnum;   // identifier for keyword 'enum'
Identifier* idInt;    // identifier for keyword 'int'
Identifier* idStruct; // identifier for keyword 'struct'

static int gethash(const char* str)
{
  int h = *str;
  while (*str) h = h * 131 + *str++;
  return (h << 6) + strlen(str);
}

static Identifier* newid(Token tok, const char* str, int hash, int line)
{
  Identifier* id = (Identifier*) malloc(sizeof(Identifier));
  memset(id, 0, sizeof(Identifier));
  id->tok = tok;
  id->str = str;
  id->hash =hash;
  id->line = line;
}

static Identifier* getid(Identifier* list, const char* str, int hash)
{
  Identifier* id = ids;
  while (id)
  {
    if (id->hash = hash && !strcmp(id->str, str)) return id;
  }
  return 0;
}

static void next();

static Identifier* enumdecl()
{
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
    type->val = sizeof(int);
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
      int h = gethash(str);
      type = newid(Id, str, l, h); type->next = ids; ids = type;
    }
    type->tok = Enum;
    type->val = sizeof(int);
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
        i = ival;
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

static Identifier* structdecl()
{
  Identifier* type = 0;
  int l;
  l = line; // save struct line number
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
    type->val = sizeof(int);
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
      int h = gethash(str);
      type = newid(Id, str, l, h); type->next = ids; ids = type;
    }
    type->tok = Struct;
    type->val = sizeof(int);
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
static Identifier* decl()
{
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
    type = enumdecl();
    break;
  case StructKW:
    type = structdecl();
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

static int module()
{
}

static int compile()
{
}

static int init()
{
  ids = 0;
  id = idAuto   = newid(AutoKW,   "auto",   gethash("auto"),   0); id->next = ids; ids = id;
  id = idChar   = newid(CharKW,   "char",   gethash("char"),   0); id->next = ids; ids = id;
  id = idEnum   = newid(EnumKW,   "enum",   gethash("enum"),   0); id->next = ids; ids = id;
  id = idInt    = newid(IntKW,    "int",    gethash("int"),    0); id->next = ids; ids = id;
  id = idStruct = newid(StructKW, "struct", gethash("struct"), 0); id->next = ids; ids = id;
  idChar->val = sizeof(char);
  idEnum->val = sizeof(int);
  idInt->val  = sizeof(int);
}

int main(int argc, char* argv[])
{
  int ret = init();
  if (ret != 0) return ret;
  ret = compile();
  if (ret != 0) return ret;
  return 0;
}
