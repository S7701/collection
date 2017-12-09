/* prolog.cpp: a simple Prolog interpreter written in C++,              */
/*             including an example test run as main().                 */
/* Copyright (c) Alan Mycroft, University of Cambridge, 2000.           */

#include <iostream>
using namespace std;
#include <string.h>

void indent(int n) {
  for (int i = 0; i < n; ++i) cout << "    ";
}

class Atom {
private:
  const char *name;
public:
  Atom(const char *n) : name(n) {}
  void print() { cout << name; }
  bool eq(Atom *t) { return strcmp(name, t->name) == 0; }
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
  CompoundTerm(CompoundTerm *p)
  : fsym(p->fsym), arity(p->arity), args(p->arity == 0 ? NULL : new Term*[p->arity]) {
    for (int i = 0; i < arity; ++i) args[i] = p->args[i]->copy();
  }
  bool unify2(CompoundTerm *t) { 
    if (!(fsym->eq(t->fsym) && arity == t->arity))
      return false;
    for (int i = 0; i<arity; i++)
      if (!args[i]->unify(t->args[i])) return false;
    return true;
  }
};

class Variable : public Term {
private:
  Term *instance;
  int varno;
  static int varcount;
public:
  Variable() : instance(this), varno(++varcount) {}
  void print() {
    if (instance != this)
      instance->print();
    else
      cout << "_" << varno;
  };
  bool unify(Term *t);
  Term *copy();
  void reset() { instance = this; }
private:
  bool unify2(CompoundTerm *t) { return this->unify(t); }
};
int Variable::varcount = 0;

class Program;
class TermVarMapping;

class Goal {
private:
  CompoundTerm *car;
  Goal *cdr;
public:
  Goal(CompoundTerm *h, Goal *t) : car(h), cdr(t) {}
  Goal *copy() {
    return new Goal(car->copy2(), cdr == NULL ? NULL : cdr->copy());
  }
  Goal *append(Goal *l) {
    return new Goal(car, cdr == NULL ? NULL : cdr->append(l));
  }
  void print() {
    car->print();
    if (cdr != NULL) {
      cout << "; ";
      cdr->print();
    }
  }
  void solve(Program *p, int level, TermVarMapping *map);
};

class Clause {
public:
  CompoundTerm *head;
  Goal *body;
  Clause(CompoundTerm *h, Goal *t) : head(h), body(t) {}
  Clause *copy() {
    return new Clause(head->copy2(), body==NULL ? NULL : body->copy());
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
  Clause *pcar;
  Program *pcdr;
  Program(Clause *h, Program *t) : pcar(h), pcdr(t) {}
};

class Trail {
private:
 Variable *tcar;
 Trail *tcdr;
 static Trail *sofar;
 Trail(Variable *h, Trail *t) : tcar(h), tcdr(t) {}
public:
  static Trail *Note() { return sofar; }
  static void Push(Variable *x) { sofar = new Trail(x, sofar); }
  static void Undo(Trail *whereto) {
    for (; sofar != whereto; sofar = sofar->tcdr)
      sofar->tcar->reset();
  }
};
Trail *Trail::sofar = NULL;

bool Variable::unify(Term *t) {
  if (instance != this) return instance->unify(t);
  Trail::Push(this);
  instance = t;
  return true;
}
Term *Variable::copy() {
  if (instance == this)
  {
    Trail::Push(this);
    instance = new Variable();
  }
  return instance;
}

class TermVarMapping {
private:
  Variable **varvar;
  const char **vartext;
  int size;
public:
  TermVarMapping(Variable *vv[], const char *vt[], int vs) :varvar(vv), vartext(vt), size(vs) {}
  void showanswer() {
    if (size == 0)
      cout << "yes\n";
    else {
      for (int i = 0; i < size; i++) {
        cout << vartext[i] << " = ";
        varvar[i]->print();
        cout << "\n";
      }
    }
  }
};

void Goal::solve(Program *p, int level, TermVarMapping *map)
{
  indent(level);
  cout << "solve@"  << level << ": ";
  this->print();
  cout << "\n";
  for (Program *q = p; q != NULL; q = q->pcdr) { 
    Trail *t = Trail::Note();
    Clause *c = q->pcar->copy();
    Trail::Undo(t);
    indent(level); cout << "  try:"; c->print(); cout << "\n";
    if (car->unify(c->head)) {
      Goal *gdash = c->body == NULL ? cdr : c->body->append(cdr);
      if (gdash == NULL)
        map->showanswer();
      else
        gdash->solve(p, level + 1, map);
    }
    else {
      indent(level);
      cout << "  nomatch.\n";
    }
    Trail::Undo(t);
  }
}

/* A sample test program: append */

Atom *at_app = new Atom("app");
Atom *at_cons = new Atom("cons");
CompoundTerm *f_nil = new CompoundTerm(new Atom("nil"));
CompoundTerm *f_1 = new CompoundTerm(new Atom("1"));
CompoundTerm *f_2 = new CompoundTerm(new Atom("2"));
CompoundTerm *f_3 = new CompoundTerm(new Atom("3"));

// app(nil, X, X).
Term *v_x = new Variable();
CompoundTerm *lhs1 = new CompoundTerm(at_app, f_nil, v_x, v_x);
Clause *c1 = new Clause(lhs1, NULL);

// app(cons(X, L), M, cons(X, N) :- app(L, M, N).
Term *v_l = new Variable();
Term *v_m = new Variable();
Term *v_n = new Variable();
CompoundTerm *rhs2 = new CompoundTerm(at_app, v_l, v_m, v_n);
CompoundTerm *lhs2 = new CompoundTerm(at_app, new CompoundTerm(at_cons, v_x, v_l), v_m, new CompoundTerm(at_cons, v_x, v_n));
Clause *c2 = new Clause(lhs2, new Goal(rhs2,NULL));

// app(I, J, cons(1, cons(2, cons(3, nil)))).
Variable *v_i = new Variable();
Variable *v_j = new Variable();
CompoundTerm *rhs3 = new CompoundTerm(at_app, v_i, v_j,  new CompoundTerm(at_cons, f_1, new CompoundTerm(at_cons, f_2, new CompoundTerm(at_cons, f_3, f_nil))));

Goal *g1 = new Goal(rhs3, NULL);

Program *test_p = new Program(c1, new Program(c2, NULL));
Program *test_p2 = new Program(c2, new Program(c1, NULL));

Variable *varvar[] = {v_i, v_j};
const char *varname[] =  {"I", "J"};
TermVarMapping *var_name_map = new TermVarMapping(varvar, varname, 2);

int main(int argc, const char *argv[])
{
   cout << "=======Append with normal clause order:\n";
   g1->solve(test_p, 0, var_name_map);
   cout << "\n=======Append with reversed normal clause order:\n";
   g1->solve(test_p2, 0, var_name_map);
   return 0;
}
