#include <stdlib.h>
#include <string.h>

enum { Auto, Char, Int, Float, Struct };

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

struct type* types;

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