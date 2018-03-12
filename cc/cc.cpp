#include <stdlib.h>
#include <string.h>

enum Token {
  Num, Local, Global, Func, Id,
  Auto, Char, Double, Enum, Float, Int, Long, Struct
};

struct Identifier {
  Identifier* next;
  Token token; // Auto, Num, Local, Global, Func, Char, Int, Long, Float, Double, Enum, Struct
  int ptr; // level of indirection
  Identifier* type; // 0 for Auto, Char, Int, Float, Enum, Struct
  int ival; // Auto: ?; Num: value; Local, Global, Func: offset; Char, Int, Long, Float, Double, Enum, Struct: size in bytes
};

Identifier* identifiers;

struct member;

struct type {
  struct type* next;
  char* name;
  int basetype;            // Auto, Char, Int or Struct
  int size;                // size in bytes
  struct members* members; // only if basetype == Struct
};

struct member {
  struct members* next;
  char* name;
  struct type* type;
  int ptr;
};

int tok;
int tokival;
char* tokstr;

struct type* types;

type* gettype(char* name);

void next();

int decl()
{
  struct type* type;
  int ptr;
  switch (tok)
  {
  case Char:
    break;
  case Int:
    break;
  case Enum:
    next();
    bool named = false;
    if (tok == Id) { next(); named = true; }
    if (tok == '{')
    {
      next();
      int i = 0;
      while (tok !='}')
      {
        if (tok != Id) { printf("%d: bad enum identifier\n", line); exit(-1); }
        next();
        if (tok == '=')
        {
          next();
          if (tok != Num) { printf("%d: bad enum initializer\n", line); exit(-1); }
          i = tokival;
          next();
        }
        ++i;
      }
    }
    else if (!named && tok ==';') { printf("%d: bad forward declaration of enum without name\n", line); exit(-1); }
    break;
  case Struct:
    break;
  case Id: // Identifier
    type = gettype(tokstr);
    if (type == 0) return 0;
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