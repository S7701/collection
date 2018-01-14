#include <cassert>
#include <iostream>
#include <list>
#include <string>

using namespace std;

class Atom;
class Number;
class CompoundTerm;
class Variable;

class Term {
public:
  typedef list<Term*> List;
  static void Print(const List& l, const string& s) {
    for (auto it = l.begin();;) {
      (*it)->print();
      if (++it == l.end()) break;
      cout << s;
    }
  }
public:
  virtual void print() const = 0;
  virtual bool unify(Term* t) = 0;
  virtual bool unifyAtom(Atom* a) { return false; }
  virtual bool unifyNumber(Number* n) { return false; }
  virtual bool unifyCompoundTerm(CompoundTerm* ct) { return false; }
  virtual bool unifyVariable(Variable* v) { return false; }
  virtual Term* copy() = 0;
};

class Atom: public Term {
private:
  string name;
public:
  Atom(const string& n): name(n) {}
  Atom(Atom* a) : name(a->name) {}
  void print() const { cout << name; }
  bool unify(Term* t) { return t->unifyAtom(this); }
  virtual bool unifyAtom(Atom* a) {
    if (name == a->name)
      return true;
    return false;
  }
  Term* copy() {
    return new Atom(this);
  }
};

class Number: public Term {
private:
  double value;
public:
  Number(double v): value(v) {}
  Number(Number* n) : value(n->value) {}
  void print() const { cout << value; }
  bool unify(Term* t) { return t->unifyNumber(this); }
  virtual bool unifyNumber(Number* n) {
    if (value == n->value)
      return true;
    return false;
  }
  Term* copy() {
    return new Number(this);
  }
};

class CompoundTerm: public Term {
private:
  string functor;
  unsigned int arity;
  Term::List args;
public:
  CompoundTerm(string f): functor(f), arity(0) {}
  CompoundTerm(CompoundTerm* ct): functor(ct->functor), arity(ct->arity) {
    for (auto it = ct->args.begin(); it != ct->args.end(); ++it)
      args.push_back((*it)->copy());
  }
  void add(Term* arg) { args.push_back(arg); ++arity; }
  void print() const {
    cout << functor;
    if (!args.empty()) {
      cout << "(";
      Term::Print(args, ", ");
      cout << ")";
    }
  };
  bool unify(Term* t) { return t->unifyCompoundTerm(this); }
  bool unifyCompoundTerm(CompoundTerm* ct) {
    if ((functor != ct->functor) || (arity != ct->arity))
      return false;
    auto it1 = args.begin();
    auto it2 = ct->args.begin();
    for (unsigned int i = 0; i < arity; ++i)
      if (!(*it1++)->unify(*it2++))
        return false;
    return true;
  }
  Term* copy() { return new CompoundTerm(this); }
};

class Variable: public Term {
private:
  static unsigned int count;
  static list<Variable*> trail;
public:
  static Variable* Trailhead() { return trail.front(); }
  static void Rewind(Variable* to) {
    Variable* v = trail.front();
    while (v != to) {
      v->reset();
      trail.pop_front();
      v = trail.front();
    }
  }
private:
  string name;
  Term* term;
  unsigned int nr;
public:
  Variable()
  : term(NULL), nr(count++) {}
  Variable(const string& n)
  : name(n), term(NULL), nr(count++) {}
  void print() const {
    if (term != NULL)
      term->print();
    else if (!name.empty())
      cout << "#" << name;
    else
      cout << "#" << nr;
  };
  bool unify(Term* t) {
    if (term != NULL)
      return term->unify(t);
    trail.push_back(this);
    term = t;
    return true;
  }
  bool unifyAtom(Atom* a) { return unify(a); }
  bool unifyNumber(Number* n) { return unify(n); }
  bool unifyCompoundTerm(CompoundTerm* ct) { return unify(ct); }
  bool unifyVariable(Variable* v) { return unify(v); }
  Term* copy() {
    if (term == NULL) {
      trail.push_back(this);
      term = new Variable();
    }
    return term;
  }
  void reset() { term = NULL; }
};

unsigned int Variable::count = 0;
list<Variable*> Variable::trail;

class Clause {
private:
  Term* head;
  Term::List body; // empty for facts
public:
  typedef list<Clause*> List;
  Clause(Term* h) : head(h) {}
  void add(Term* t) { body.push_back(t); }
  Clause* copy() {
    Clause* c = new Clause(head->copy());
    for (auto it = body.begin(); it != body.end(); ++it)
      c->add((*it)->copy());
    return c;
  }
  void print() const {
    head->print();
    cout << " :- ";
    if (body.empty())
      cout << "true";
    else
      Term::Print(body, "; ");
  }
};

void indent(unsigned int n) {
  for (unsigned int i = 0; i < n; ++i) cout << "    ";
}

class Program {
private:
  Clause::List clauses;
  ;
public:
  void addClause(Clause* c) { clauses.push_back(c); }
  void solve(Term::List query, unsigned int level = 0) {
    indent(level);
    cout << "solve@"  << level << ": ";
    Term::Print(query, "; ");
    cout << "\n";
    for (auto q = clauses.begin(); q != clauses.end(); ++q) { 
    }
  }
};

int main(int argc, const char* const argv[]) {
  CompoundTerm* ct;
  Program* p = new Program();
  // "ist kind von"(Nico, Nicki)
  ct = new CompoundTerm("ist kind von");
  ct->add(new Atom("Nico"));
  ct->add(new Atom("Nicki"));
  p->addClause(new Clause(ct));
  // "ist kind von"(Luca, Nicki)
  ct = new CompoundTerm("ist kind von");
  ct->add(new Atom("Luca"));
  ct->add(new Atom("Nicki"));
  p->addClause(new Clause(ct));
  // "ist kind von"(#X, Nicki)
  ct = new CompoundTerm("ist kind von");
  ct->add(new Variable("X"));
  ct->add(new Atom("Nicki"));
  // 
  Term::List q;
  q.push_back(ct);
  p->solve(q);
  return 0;
}
