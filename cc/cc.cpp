#include <stdlib.h>
#include <string.h>

enum Token {
  Num = 128, Local, Global, Func, Id,
  Auto, Char, Enum, Int, Struct
};

const char* tokens[] = {
  "number", "local variable", "global variable", "function", "identifier",
  "auto", "char", "enum", "int", "struct"
};

struct Identifier {
  Identifier* next; // next in list
  Token tok;        // Auto, Num, Local, Global, Func, Char, Int, Enum, Struct
  const char* str;  // name of identifier; may be 0 for unnamed enum or struct
  int line;         // line number
  int ptr;          // Local, Global, Func: level of indirection
  Identifier* ref;  // Num, Local, Global, Func: type; Enum, Struct: member list
  int val;          // Num: value; Local, Global, Func: offset; Char, Int, Enum, Struct: size in bytes
};

int tok;         // current token
Identifier* id;  // currently parsed identifier
int ival;        // currently parsed integer value
int line;        // current line number
Identifier* ids; // global identifier list

Identifier* newid(Token tok, const char* str, int line);
Identifier* getid(Identifier* list, const char* str);

void next();

// "char" ('*')* identifier (',' ('*')* identifier)* ';' |
// "int" ('*')* identifier (',' ('*')* identifier)* ';' |
// "enum" identifier ';' |
// "enum" identifier '{' (identifier ('=' number)? (',' identifier ('=' number)?)*)? '}' (('*')* identifier (',' ('*')* identifier)*)? ';' |
// "enum" '{' (identifier ('=' number)? (',' identifier ('=' number)?)*)? '}' ('*')* identifier (',' ('*')* identifier)* ';' |
// "struct" identifier ';' |
// "struct" identifier '{' (type ('*')* identifier ';')* '}' (('*')* identifier (',' ('*')* identifier)*)? ';' |
// "struct" '{' (type ('*')* identifier ';')* '}' ('*')* identifier (',' ('*')* identifier)* ';' |
// type ('*')* identifier (',' ('*')* identifier)* ';'
Identifier* decl()
{
  Identifier* type = 0;
  Identifier* name = 0;
  int l;
  int i = 0;
  int ptr = 0;
  switch (tok)
  {
  case Char:
    type = id;
    break;
  case Int:
    type = id;
    break;
  case Enum:
    l = line; // save enum line number
    next();
    if (tok == Id) // named enum?
    {
      name = id;
      next();
    }
    if (tok ==';') // enum declaration?
    {
      if (!name)
      { printf("%d: bad enum declaration; missing enum name after keyword 'enum'\n", l); exit(-1); }
      else if (name->tok == Enum)
      { printf("%d: duplicate enum declaration; enum name '%s' already declared in line %d\n", l, name->str, name->line); exit(-1); }
      else if (name->tok != Id)
      { printf("%d: bad enum declaration; enum name '%s' already declared in line %d as %s\n", l, name->str, name->line, tokens[id->tok]); exit(-1); }
      name->tok = Enum;
      name->val = sizeof (int);
      return name;
    }
    if (tok == '{') // enum definition?
    {
      if (name)
      {
        if (name->tok == Enum)
        { printf("%d: duplicate enum definition; enum name '%s' already declared in line %d\n", l, name->str, name->line); exit(-1); }
        else if (name->tok != Id)
        { printf("%d: bad enum definition; enum name '%s' already declared in line %d as %s\n", l, name->str, name->line, tokens[id->tok]); exit(-1); }
      }
      else
      {
        char str[20];
        sprintf(str, "__unnamed_enum_%04X");
        name = newid(Id, str, l); name->next = ids; ids = name;
      }
      type = newid(Enum, name->name, name->line); type->next = ids; ids = type;
      type->val = sizeof (int);
      next();
      while (tok !='}')
      {
        if (tok != Id) { printf("%d: bad enum definition; enumerator identifier expected\n", line); exit(-1); }
        name = id; ids = name->next; name->next = 0; // remove id from global identifier list
        next();
        if (tok == '=')
        {
          next();
          if (tok != Num) { printf("%d: bad enumerator initializer; number expected, got %s\n", line, tokens[id->tok]); exit(-1); }
          i = tokival;
          next();
        }
        name->tok = Num; name->ref = getid(ids, "int"); name->val = i++;
        name->next = type->ref; type->ref = name; // add id to member list
      }
    }
    else
    { printf("%d: enum declaration or definition expected\n", l); exit(-1); }
    break;
  case Struct:
    break;
  case Id: // Identifier
    type = id;
    if (type->tok != Enum && type->tok != Struct) return 0;
    break;
  default:
    return 0;
  }
  while (tok == '*') {
    next();
    ++ptr;
  }
  return 1;
}

int module()
{
}

struct type* newtype(char* name, int basetype, int size)
{
  struct type* type = (struct type*)malloc(sizeof(struct type));
  type->next = types;
  type->name = name;
  type->basetype = basetype;
  type->size = size;
  type->members = 0;
  types = type;
}

bool unifytypes(struct type* left, struct type* right)
{
  return false;
}

struct type* gettype(char* name)
{
  struct type* type = types;
  while (type) {
    if (!strcmp(type->name, name))
      return type;
    type = type->next;
  }
}

int init()
{
  newtype("auto", Auto, 0);
  newtype("char", Char, sizeof(char));
  newtype("int", Int, sizeof(int));
  newtype("float", Float, sizeof(double));
}

int main(int argc, char* argv[])
{
  int ret = module();
  if (ret != 0) return ret;
  return 0;
}
