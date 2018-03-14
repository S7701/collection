#include <stdlib.h>
#include <string.h>

enum Token {
  Num, Local, Global, Func, Id,
  Auto, Char, Enum, Int, Struct
};

const char* tokens[] = {
  "Num", "Local", "Global", "Func", "Id",
  "Auto", "Char", "Enum", "Int", "Struct"
};

struct Identifier {
  Identifier* next; // next in list
  const char* str; // name of identifier; may be 0 for unnamed enum or struct
  Token tok; // Auto, Num, Local, Global, Func, Char, Int, Enum, Struct
  int ptr; // level of indirection
  Identifier* ref; // Num, Local, Global, Func: type; Enum, Struct: member list
  int val; // Num: value; Local, Global, Func: offset; Char, Int, Enum, Struct: size in bytes
};

Identifier* identifiers;

int tok;
int tokival;
char* tokstr;

struct type* types;

Identifier* getIdentifier(char* str);

void next();

int decl()
{
  bool named = false;
  int i = 0;
  Identifier* id = 0;
  int ptr = 0;
  int l;
  switch (tok)
  {
  case Char:
    break;
  case Int:
    break;
  case Enum:
    l = line;
    next();
    if (tok == Id)
    {
      id = getIdentifier(tokstr);
      next();
      named = true;
    }
    if (tok == '{')
    {
      next();
      if (id)
      {
        if (id->tok == Enum)
        { printf("%d: duplicate enum declaration\n", l); exit(-1); }
        else
        { printf("%d: bad enum declaration; name already used %s\n", l, tokens[id->tok]); exit(-1); }
      }
      id = newIdentifier(Enum, name);
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
    else if (tok ==';')
    {
      if (!named) {printf("%d: missing name in enum forward declaration\n", line); exit(-1); }
      return 1;
    }
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
