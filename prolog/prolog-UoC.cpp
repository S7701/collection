// prolog.cpp: a simple Prolog interpreter written in C++,
//             including an example test run as main().
// Copyright (c) Alan Mycroft, University of Cambridge, 2000.

#include <iostream>
#include <cstring>
using namespace std;

void indent(int n) {
  for (int i = 0; i < n; ++i) cout << "    ";
}

class Atom {
private:
  const char *name;
	int len;
public:
  Atom(const char *n) : name(n), len(strlen(n)) {}
  void print() { cout << name; }
  bool eq(Atom *other) { return ((len == other->len) && (strcmp(name, other->name) == 0)); }
};

class CompoundTerm;

class Term {
public:
    virtual void print() = 0;
    virtual bool unify(Term *) = 0;
    virtual bool unify2(CompoundTerm *) = 0;
    virtual Term *copy() = 0;
};

class CompoundTerm : public Term {
private:
    Atom *fsym;
    int arity;
    Term **args;
public:
  CompoundTerm(Atom *f) : fsym(f), arity(0), args(NULL) {}
  CompoundTerm(Atom *f, Term *a1) : fsym(f), arity(1), args(new Term*[1]) { args[0] = a1; };
  CompoundTerm(Atom *f, Term *a1, Term *a2) : fsym(f), arity(2), args(new Term*[2]) { args[0] = a1, args[1] = a2; };
  CompoundTerm(Atom *f, Term *a1, Term *a2, Term *a3) : fsym(f), arity(3), args(new Term*[3]) { args[0] = a1, args[1] = a2, args[2] = a3; };
  void print() {
    fsym->print();
    if (arity > 0) {
      cout << "(";
      for (int i = 0; i<arity; ) {
        args[i]->print();
        if (++i < arity) cout << ", ";
      }
      cout << ")";
    }
  }
  bool unify(Term *t) { return t->unify2(this); }
  Term *copy() { return copy2(); }
  CompoundTerm *copy2() { return new CompoundTerm(this); }
private:
  CompoundTerm(CompoundTerm *other)
  : fsym(other->fsym), arity(other->arity), args(other->arity == 0 ? NULL : new Term*[other->arity]) {
    for (int i = 0; i < arity; ++i)
      args[i] = other->args[i]->copy();
  }
  bool unify2(CompoundTerm *other) { 
    if (!(fsym->eq(other->fsym) && arity == other->arity))
      return false;
    for (int i = 0; i < arity; i++)
      if (!args[i]->unify(other->args[i]))
        return false;
    return true;
  }
};

class Variable : public Term {
private:
  Term *instance;
  const char *varname;
  int varno;
private:
  static int varcount;
public:
  Variable(const char *n = NULL) : instance(NULL), varname(n), varno(++varcount) {}
  const char *name() const { return varname; }
  void print() {
    if (instance != NULL)
      instance->print();
    else {
      if (varname != NULL)
        cout << varname;
      cout << "#" << varno;
    }
  };
  bool unify(Term *t);
  Term *copy();
  void reset() { instance = NULL; }
private:
  bool unify2(CompoundTerm *t) { return this->unify(t); }
};
int Variable::varcount = 0;

class Trail {
private:
 Variable *var;
 Trail *next;
 Trail(Variable *v, Trail *n) : var(v), next(n) {}
private:
 static Trail *top;
public:
  static Trail *Top() { return top; }
  static void Push(Variable *v) { top = new Trail(v, top); }
  static void Undo(Trail *t) {
    for (; top != t; top = top->next)
      top->var->reset();
  }
};
Trail *Trail::top = NULL;

bool Variable::unify(Term *t) {
  if (instance != NULL) return instance->unify(t);
  Trail::Push(this);
  instance = t;
  return true;
}
Term *Variable::copy() {
  if (instance == NULL)
  {
    Trail::Push(this);
    instance = new Variable(/*varname?*/);
  }
  return instance;
}

class Program;

class Goal {
private:
  CompoundTerm *term;
  Goal *next;
public:
  Goal(CompoundTerm *h, Goal *t) : term(h), next(t) {}
  Goal *copy() {
    return new Goal(term->copy2(), next == NULL ? NULL : next->copy());
  }
  Goal *append(Goal *l) {
    return new Goal(term, next == NULL ? NULL : next->append(l));
  }
  void print() {
    term->print();
    if (next != NULL) {
      cout << "; ";
      next->print(); // recursive print rest of goal
    }
  }
  void solve(Program *p, Variable *vars[], unsigned int numvars, unsigned int level = 0);
};

class Clause {
public:
  CompoundTerm *head;
  Goal *body;
  Clause(CompoundTerm *h, Goal *b) : head(h), body(b) {}
  Clause *copy() {
    return new Clause(head->copy2(), body == NULL ? NULL : body->copy());
  }
  void print() {
    head->print();
    cout << " :- ";
    if (body == NULL)
      cout << "true";
    else
      body->print();
  }
};

class Program {
public:
  Clause *clause;
  Program *next;
  Program(Clause *c, Program *n) : clause(c), next(n) {}
};

void Goal::solve(Program *p, Variable *vars[], unsigned int numvars, unsigned int level)
{
  indent(level);
  cout << "solve@"  << level << ": ";
  this->print();
  cout << "\n";
  for (Program *q = p; q != NULL; q = q->next) { 
    Trail *t = Trail::Top();
    Clause *c = q->clause->copy();
    Trail::Undo(t);
    indent(level); cout << "  try: "; c->print(); cout << "\n";
    if (term->unify(c->head)) {
      Goal *gdash = c->body == NULL ? next : c->body->append(next);
      if (gdash == NULL)
      {
        if (numvars == 0)
          cout << "yes\n";
        else
        {
          for (unsigned int i = 0; i < numvars; i++)
          {
            cout << "@" << vars[i]->name() << " = ";
            vars[i]->print();
            cout << "\n";
          }
        }
      }
      else
        gdash->solve(p, vars, numvars, level + 1);
    }
    else {
      indent(level);
      cout << "  nomatch.\n";
    }
    Trail::Undo(t);
  }
}

/* A sample test program: append */

Atom *at_append = new Atom("append");
Atom *at_pair = new Atom("pair");
CompoundTerm *f_nil = new CompoundTerm(new Atom("nil"));
CompoundTerm *f_1 = new CompoundTerm(new Atom("1"));
CompoundTerm *f_2 = new CompoundTerm(new Atom("2"));
CompoundTerm *f_3 = new CompoundTerm(new Atom("3"));

Term *v_x = new Variable("x");
CompoundTerm *lhs1 = new CompoundTerm(at_append, f_nil, v_x, v_x);

Clause *c1 = new Clause(lhs1, NULL); // append(nil, x, x).

Term *v_l = new Variable("l");
Term *v_m = new Variable("m");
Term *v_n = new Variable("n");
CompoundTerm *rhs2 = new CompoundTerm(at_append, v_l, v_m, v_n);
CompoundTerm *lhs2 = new CompoundTerm(at_append, new CompoundTerm(at_pair, v_x, v_l), v_m, new CompoundTerm(at_pair, v_x, v_n));

Clause *c2 = new Clause(lhs2, new Goal(rhs2,NULL)); // append(pair(x, l), m, pair(x, n)) :- append(l, m, n).

Variable *v_i = new Variable("I");
Variable *v_j = new Variable("J");
CompoundTerm *rhs3 = new CompoundTerm(at_append, v_i, v_j,  new CompoundTerm(at_pair, f_1, new CompoundTerm(at_pair, f_2, new CompoundTerm(at_pair, f_3, f_nil))));

Goal *g1 = new Goal(rhs3, NULL); // ?- append(I, J, pair(1, pair(2, pair(3, nil)))).

Program *test_p1 = new Program(c1, new Program(c2, NULL));
Program *test_p2 = new Program(c2, new Program(c1, NULL));

Variable *vars[] = {v_i, v_j};

int main(int argc, const char *argv[])
{
   cout << "=======Append with normal clause order:\n";
   g1->solve(test_p1, vars, 2);
   cout << "\n=======Append with reversed normal clause order:\n";
   g1->solve(test_p2, vars, 2);
   return 0;
}
