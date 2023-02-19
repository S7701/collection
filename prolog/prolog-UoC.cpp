// prolog.cpp: a simple Prolog interpreter written in C++,
//             including an example test run as main().
// Copyright (c) Alan Mycroft, University of Cambridge, 2000.

#include <iostream>
#include <cstring>

using namespace std;

bool debug_solve(true);
bool debug_copy(false);

void indent(int n) {
  for (int i = 0; i < n; ++i) cout << "    ";
}

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
    const char *fsym;
    int arity;
    Term **args;
public:
  CompoundTerm(const char *f) : fsym(f), arity(0), args(NULL) {}
  CompoundTerm(const char *f, Term *a1) : fsym(f), arity(1), args(new Term*[1]) { args[0] = a1; };
  CompoundTerm(const char *f, Term *a1, Term *a2) : fsym(f), arity(2), args(new Term*[2]) { args[0] = a1, args[1] = a2; };
  CompoundTerm(const char *f, Term *a1, Term *a2, Term *a3) : fsym(f), arity(3), args(new Term*[3]) { args[0] = a1, args[1] = a2, args[2] = a3; };
  void print() {
    cout << fsym;
    if (arity > 0) {
      cout << "(";
      for (int i = 0; i < arity; ) {
        args[i]->print();
        if (++i < arity) cout << ", ";
      }
      cout << ")";
    }
  }
  bool unify(Term *t) { return t->unify2(this); }
  Term *copy() {
    if (debug_copy) { cout << "  CompoundTerm::copy()" << endl; }
    return copy2();
  }
  CompoundTerm *copy2() {
    if (debug_copy) { cout << "  CompoundTerm::copy2()" << endl; }
    return new CompoundTerm(this);
  }
private:
  CompoundTerm(CompoundTerm *other)
  : fsym(other->fsym), arity(other->arity), args(other->arity ? new Term*[other->arity] : NULL) {
    for (int i = 0; i < arity; ++i)
      args[i] = other->args[i]->copy();
  }
  bool unify2(CompoundTerm *other) { 
    if (!(fsym == other->fsym && arity == other->arity))
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
  int id;
private:
  static int count;
public:
  Variable(const char *n = "") : instance(NULL), varname(n), id(++count) {}
  const char *name() const { return varname; }
  void print() {
    if (instance)
      instance->print();
    else {
      cout << varname << "#" << id;
    }
  };
  bool unify(Term *t);
  Term *copy();
  void reset() { instance = NULL; }
private:
  bool unify2(CompoundTerm *t) { return this->unify(t); }
};

int Variable::count = 0;

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
  if (instance) return instance->unify(t);
  Trail::Push(this);
  instance = t;
  return true;
}

Term *Variable::copy() {
  if (debug_copy) { cout << "  Variable::copy()" << endl; }
  if (!instance) {
    Trail::Push(this);
    instance = new Variable(varname);
  }
  return instance;
}

class Program;

class Goal {
private:
  CompoundTerm *term;
  Goal *next;
public:
  Goal(CompoundTerm *h, Goal *t = NULL) : term(h), next(t) {}
  Goal *copy() {
    if (debug_copy) { cout << "  Goal::copy()" << endl; }
    return new Goal(term->copy2(), next ? next->copy() : NULL);
  }
  Goal *append(Goal *l) {
    if (debug_copy) { cout << "  Goal::append()" << endl; }
    return new Goal(term, next ? next->append(l) : NULL);
  }
  void print() {
    term->print();
    if (next) {
      cout << "; ";
      next->print(); // recursive print rest of goal
    }
  }
  int solve(Program *p, Variable *vars[], unsigned int numvars, unsigned int level = 0);
};

class Clause {
public:
  CompoundTerm *head;
  Goal *body;
  Clause(CompoundTerm *h, Goal *b = NULL) : head(h), body(b) {}
  Clause *copy() {
    if (debug_copy) { cout << "  Clause::copy()" << endl; }
    return new Clause(head->copy2(), body ? body->copy() : NULL);
  }
  void print() {
    head->print();
    cout << " :- ";
    if (body)
      body->print();
    else
      cout << "true";
  }
};

class Program {
public:
  Clause *clause;
  Program *next;
  Program(Clause *c, Program *n = NULL) : clause(c), next(n) {}
};

int Goal::solve(Program *p, Variable *vars[], unsigned int numvars, unsigned int level)
{
  int solved = 0;
  if (debug_solve) { indent(level); cout << "solve@"  << level << ": "; this->print(); cout << endl; }
  for (Program *q = p; q != NULL; q = q->next) { 
    Trail *t = Trail::Top();
    Clause *c = q->clause->copy();
    Trail::Undo(t);
    if (debug_solve) { indent(level); cout << "  try: "; c->print(); cout << endl; }
    if (term->unify(c->head)) {
      if (debug_solve) { indent(level); cout << "  match." << endl; }
      Goal *g = c->body ? c->body->append(next) : next;
      if (g) {
        solved += g->solve(p, vars, numvars, level + 1);
      }
      else {
        ++solved;
        if (numvars) {
          for (unsigned int i = 0; i < numvars; i++) {
            cout << vars[i]->name() << " = ";
            vars[i]->print();
            cout << endl;
          }
          cout << endl;
        }
      }
    }
    else {
      if (debug_solve) { indent(level); cout << "  no match." << endl; }
    }
    Trail::Undo(t);
  }
  if (level == 0) {
    if (solved)
      cout << "true: " << solved << " solutions" << endl;
    else
      cout << "false" << endl;
  }
  return solved;
}

void original_sample_test_program() {
  /* A sample test program: append */

  const char *a_append = "append";
  const char *a_pair = "pair";
  CompoundTerm *f_nil = new CompoundTerm("nil");
  CompoundTerm *f_1 = new CompoundTerm("1");
  CompoundTerm *f_2 = new CompoundTerm("2");
  CompoundTerm *f_3 = new CompoundTerm("3");
  Variable *v_i = new Variable("I");
  Variable *v_j = new Variable("J");
  Term *v_x = new Variable("x");
  Term *v_l = new Variable("l");
  Term *v_m = new Variable("m");
  Term *v_n = new Variable("n");

  // append(nil, x, x).
  CompoundTerm *lhs1 = new CompoundTerm(a_append, f_nil, v_x, v_x);
  Clause *c1 = new Clause(lhs1);

  // append(pair(x, l), m, pair(x, n)) :- append(l, m, n).
  CompoundTerm *lhs2 = new CompoundTerm(a_append, new CompoundTerm(a_pair, v_x, v_l), v_m, new CompoundTerm(a_pair, v_x, v_n));
  CompoundTerm *rhs2 = new CompoundTerm(a_append, v_l, v_m, v_n);
  Clause *c2 = new Clause(lhs2, new Goal(rhs2));

  // program
  Program *p1 = new Program(c1, new Program(c2));
  Program *p2 = new Program(c2, new Program(c1));

  // ?- append(I, J, pair(1, pair(2, pair(3, nil)))).
  CompoundTerm *rhs3 = new CompoundTerm(a_append, v_i, v_j,  new CompoundTerm(a_pair, f_1, new CompoundTerm(a_pair, f_2, new CompoundTerm(a_pair, f_3, f_nil))));
  Goal *g = new Goal(rhs3);

  Variable *vars[] = {v_i, v_j};

  cout << "======= Append with normal clause order:\n";
  g->solve(p1, vars, 2);

  cout << "\n======= Append with reversed clause order:\n";
  g->solve(p2, vars, 2);
}

void sample_test_program() {
  const char *a_grandparent_grandchild = "grandparent-grandchild";
  const char *a_parent_child = "parent-child";
  CompoundTerm *f_fathers_father = new CompoundTerm("fathers-father");
  CompoundTerm *f_fathers_mother = new CompoundTerm("fathers-mother");
  CompoundTerm *f_mothers_father = new CompoundTerm("mothers-father");
  CompoundTerm *f_mothers_mother = new CompoundTerm("mothers-mother");
  CompoundTerm *f_father = new CompoundTerm("father");
  CompoundTerm *f_mother = new CompoundTerm("mother");
  CompoundTerm *f_son = new CompoundTerm("son");
  CompoundTerm *f_daughter = new CompoundTerm("daughter");
  Variable *v_grandparent = new Variable("grandparent");
  Variable *v_grandchild = new Variable("grandchild");
  Variable *v = new Variable();

  // Facts:

  // parent-child("fathers-father", "father").
  Clause *c1 = new Clause(new CompoundTerm(a_parent_child, f_fathers_father, f_father));

  // parent-child("fathers-mother", "father").
  Clause *c2 = new Clause(new CompoundTerm(a_parent_child, f_fathers_mother, f_father));

  // parent-child("mothers-father", "mother").
  Clause *c3 = new Clause(new CompoundTerm(a_parent_child, f_mothers_father, f_mother));

  // parent-child("smother-mother", "mother").
  Clause *c4 = new Clause(new CompoundTerm(a_parent_child, f_mothers_mother, f_mother));

  // parent-child("father", "son").
  Clause *c5 = new Clause(new CompoundTerm(a_parent_child, f_father, f_son));

  // parent-child("mother", "son").
  Clause *c6 = new Clause(new CompoundTerm(a_parent_child, f_mother, f_son));

  // parent-child("father", "daughter").
  Clause *c7 = new Clause(new CompoundTerm(a_parent_child, f_father, f_daughter));

  // parent-child("mother", "daughter").
  Clause *c8 = new Clause(new CompoundTerm(a_parent_child, f_mother, f_daughter));

  // Rules:

  // grandparent-grandchild(?grandparent, ?grandchild) :- parent-child(?grandparent, ?), parent-child(?, ?grandchild).
  CompoundTerm *lhs9 = new CompoundTerm(a_grandparent_grandchild, v_grandparent, v_grandchild);
  CompoundTerm *rhs9_1 = new CompoundTerm(a_parent_child, v_grandparent, v);
  CompoundTerm *rhs9_2 = new CompoundTerm(a_parent_child, v, v_grandchild);
  Clause *c9 = new Clause(lhs9, new Goal(rhs9_1, new Goal(rhs9_2)));

  // program
  Program *p = new Program(c1, 
               new Program(c2, 
               new Program(c3, 
               new Program(c4, 
               new Program(c5, 
               new Program(c6, 
               new Program(c7, 
               new Program(c8, 
               new Program(c9
               )))))))));

  // Queries:

  cout << "\n?- grandparent-grandchild(fathers-mother, dauther).\n";
  Goal *g1 = new Goal(new CompoundTerm(a_grandparent_grandchild, f_fathers_mother, f_daughter));
  g1->solve(p, NULL, 0);

  cout << "\n?- grandparent-grandchild(father, dauther).\n";
  Goal *g2 = new Goal(new CompoundTerm(a_grandparent_grandchild, f_father, f_daughter));
  g2->solve(p, NULL, 0);

  cout << "\n?- grandparent-grandchild(?grandparent, ?grandchild).\n";
  Goal *g3 = new Goal(new CompoundTerm(a_grandparent_grandchild, v_grandparent, v_grandchild));
  Variable *vars3[] = {v_grandparent, v_grandchild};
  g3->solve(p, vars3, 2);
}

int main(int argc, const char *argv[]) {
  original_sample_test_program();

  sample_test_program();

  return 0;
}
