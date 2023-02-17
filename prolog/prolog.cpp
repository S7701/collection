#include <cassert>
#include <iostream>
#include <list>
#include <string>

using namespace std;

bool debug = true;

class Atom;
class CompoundTerm;
class Variable;

class Term {
public: // statics
  static void Print(const list<Term*> &l, const string &s) {
    for (auto it = l.begin();;) {
      (*it)->print();
      if (++it == l.end()) break;
      cout << s;
    }
  }
public:
  virtual void print() const = 0;
  virtual bool unify(Term *t) = 0;
  virtual bool unifyAtom(Atom *a) { return false; }
  virtual bool unifyCompoundTerm(CompoundTerm *ct) { return false; }
  virtual bool unifyVariable(Variable *v) { return false; }
};

class Atom: public Term { // name.
private:
  string name;
public:
  Atom(const string &n): name(n) {}
  void print() const { cout << name; }
  bool unify(Term *t) { return t->unifyAtom(this); }
  virtual bool unifyAtom(Atom *a) {
    if (name == a->name)
      return true;
    return false;
  }
  bool operator ==(Atom* other) { return name == other->name; }
  const string &getName() const { return name; }
};

class CompoundTerm: public Term { // functor(arg0, arg1 ... argN).
private:
  Atom *functor;
  unsigned int arity;
  list<Term*> args;
public:
  CompoundTerm(Atom *f): functor(f), arity(0) {}
  void add(Term *arg) { args.push_back(arg); ++arity; }
  void print() const {
    functor->print();
    cout << "(";
    if (!args.empty()) {
      Term::Print(args, ", ");
    }
    cout << ")";
  };
  bool unify(Term *t) { return t->unifyCompoundTerm(this); }
  bool unifyCompoundTerm(CompoundTerm *ct) {
    if ((functor != ct->functor) || (arity != ct->arity))
      return false;
    auto it1 = args.begin();
    auto it2 = ct->args.begin();
    for (unsigned int i = 0; i < arity; ++i)
      if (!(*it1++)->unify(*it2++))
        return false;
    return true;
  }
};

class Variable: public Term {
private: // statics
  static unsigned int count;
  static list<Variable*> trail;
public: // statics
  static Variable *GetTrailhead() { return trail.back(); }
  static void Rewind(Variable *to) {
    Variable *v = trail.back();
    while (v != to) {
      v->reset();
      trail.pop_back();
      v = trail.back();
    }
  }
private:
  Atom *name;
  unsigned int id;
  Term *term;
public:
  Variable(): name(NULL), id(count++), term(NULL) {}
  Variable(Atom *n): name(n), id(count++), term(NULL) {}
  const string &getName() const { return name->getName(); }
  void print() const {
    const string &n = name->getName();
    if (term != NULL)
      term->print();
    else if (!n.empty())
      cout << "?" << n;
    else
      cout << "?" << id;
  };
  bool unify(Term *t) {
    if (term != NULL)
      return term->unify(t);
    trail.push_back(this);
    term = t;
    return true;
  }
  bool unifyAtom(Atom *a) { return unify(a); }
  bool unifyCompoundTerm(CompoundTerm *ct) { return unify(ct); }
  bool unifyVariable(Variable *v) { return unify(v); }
  void reset() { term = NULL; }
};

unsigned int Variable::count = 0;
list<Variable*> Variable::trail;

class Clause { // head :- body
private:
  Term *head;
  list<Term*> body; // empty for facts
public:
  Clause(Term *h) : head(h) {}
  Term *getHead() const { return head; }
  list<Term*> &getBody() { return body; }
  void add(Term *t) { body.push_back(t); }
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
  list<Clause*> clauses;
public:
  void addClause(Clause *c) { clauses.push_back(c); }
  bool solve(Term *q, list<Variable*> &vl, unsigned int level = 0) {
    bool solved = false;
    indent(level);
    if (debug) {
      cout << "solve@"  << level << ": ";
      q->print();
      cout << "\n";
    }
    for (auto it = clauses.begin(); it != clauses.end(); ++it) {
      Variable *trailhead = Variable::GetTrailhead();
      Clause *c = (*it);
      Term *head = c->getHead();
      if (debug) {
        indent(level);
        cout << "  try: ";
        c->print();
        cout << "\n";
      }
      if (q->unify(head)) {
        list<Term*> &body = c->getBody();
        if (body.empty()) {
          solved = true;
          if (debug) {
            indent(level);
            cout << "  match\n";
          }
          if (vl.empty()) {
            if (level == 0)
              cout << "true";
          }
          else for (auto it = vl.begin(); it != vl.end(); ++it) {
            if (it != vl.begin()) cout << ", ";
            Variable* v = (*it);
            cout << v->getName() << " = ";
            v->print();
          }
          cout << "\n";
        }
        else if(solve(body, vl, level+1))
          solved = true;
      }
      else if (debug) {
        indent(level);
        cout << "  no match\n";
      }
      Variable::Rewind(trailhead);
    }
    if (level == 0)
      cout << (solved ? "true\n" : "false\n");
    return solved;
  }
  bool solve(list<Term*> &ql, list<Variable*> &vl, unsigned int level = 0) {
    bool solved = false;
    indent(level);
    cout << "solve@"  << level << ": ";
    Term::Print(ql, "; ");
    cout << "\n";
    for (auto it = ql.begin(); it != ql.end(); ++it) {
      Term* q = (*it);
      if (solve(q, vl, level+1))
        solved = true;
    }
    if (level == 0 && !solved)
      cout << (solved ? "true\n" : "false\n");
    return solved;
  }
};

// program: (clause | query)+.
// clause: term [":-" term (',' term)*] '.'.
// term: atom | variable | compound-term.
// atom: (printable)+ | '\'' (printable | whitespcae)+ '\''.
// variable: '?' atom.
// compound-term: atom '(' [term (',' term)*] ')'.
// query: "?-" compound-term '.'.

int main(int argc, const char* const argv[]) {
  Atom *aGerd = new Atom("Gerd");
  Atom *aLaura = new Atom("Laura");
  Atom *aKH = new Atom("Karl-Heinz");
  Atom *aDoris = new Atom("Doris");
  Atom *aMike = new Atom("Mike");
  Atom *aNicki = new Atom("Nicki");
  Atom *aNico = new Atom("Nico");
  Atom *aLuca = new Atom("Luca");
  Atom *aGrandchildGrandparent = new Atom("grandchild-grandparent");
  Atom *aChildParent = new Atom("child-parent");
  Atom *aGrandchild = new Atom("grandchild");
  Atom *aChild = new Atom("child");
  Atom *aParent = new Atom("parent");
  Atom *aGrandparent = new Atom("grandparent");
  Atom *a = new Atom("");

  Program *p = new Program();
  Clause *c;
  CompoundTerm *ct;
  Variable *v1, *v2, *v3;
  list<Variable*> vl;

  // Facts:

  // child-parent(Mike, Gerd).
  ct = new CompoundTerm(aChildParent); ct->add(aMike); ct->add(aGerd);
  p->addClause(new Clause(ct));

  // child-parent(Mike, Laura).
  ct = new CompoundTerm(aChildParent); ct->add(aMike); ct->add(aLaura);
  p->addClause(new Clause(ct));

  // child-parent(Nicki, Karl-Heinz).
  ct = new CompoundTerm(aChildParent); ct->add(aNicki); ct->add(aKH);
  p->addClause(new Clause(ct));

  // child-parent(Nicki, Doris).
  ct = new CompoundTerm(aChildParent); ct->add(aNicki); ct->add(aDoris);
  p->addClause(new Clause(ct));

  // child-parent(Nico, Mike).
  ct = new CompoundTerm(aChildParent); ct->add(aNico); ct->add(aMike);
  p->addClause(new Clause(ct));

  // child-parent(Luca, Mike).
  ct = new CompoundTerm(aChildParent); ct->add(aLuca); ct->add(aMike);
  p->addClause(new Clause(ct));

  // child-parent(Nico, Nicki).
  ct = new CompoundTerm(aChildParent); ct->add(aNico); ct->add(aNicki);
  p->addClause(new Clause(ct));

  // child-parent(Luca, Nicki).
  ct = new CompoundTerm(aChildParent); ct->add(aLuca); ct->add(aNicki);
  p->addClause(new Clause(ct));

  // Rules:

  // grandchild-grandparent(?grandchild, ?grandparent) :- child-parent(?grandchild, ?), child-parent(?, ?grandparent).
  v1 = new Variable(aGrandchild);
  v2 = new Variable(aGrandparent);
  v3 = new Variable(a);
  ct = new CompoundTerm(aGrandchildGrandparent); ct->add(v1); ct->add(v2);
  c = new Clause(ct);
  ct = new CompoundTerm(aChildParent); ct->add(v1); ct->add(v3);
  c->add(ct);
  ct = new CompoundTerm(aChildParent); ct->add(v3); ct->add(v2);
  c->add(ct);
  p->addClause(c);

  // Queries:

  vl.clear();
  cout << "?- child-parent(?child, Nicki).\n";
  v1 = new Variable(aChild);
  ct = new CompoundTerm(aChildParent); ct->add(v1); ct->add(aNicki);
  vl.push_back(v1); p->solve(ct, vl);

  vl.clear();
  cout << "\n?- child-parent(?child, Luca).\n";
  v1 = new Variable(aChild);
  ct = new CompoundTerm(aChildParent); ct->add(v1); ct->add(aLuca);
  vl.push_back(v1); p->solve(ct, vl);

  vl.clear();
  cout << "\n?- child-parent(?child, ?parent).\n";
  v1 = new Variable(aChild);
  v2 = new Variable(aParent);
  ct = new CompoundTerm(aChildParent); ct->add(v1); ct->add(v2);
  vl.push_back(v1); vl.push_back(v2); p->solve(ct, vl);

  cout << "\n?- child-parent(Nico, Mike).\n";
  vl.clear();
  ct = new CompoundTerm(aChildParent); ct->add(aNico); ct->add(aMike);
  p->solve(ct, vl);

  cout << "\n?- child-parent(Luca, Gerd).\n";
  vl.clear();
  ct = new CompoundTerm(aChildParent); ct->add(aLuca); ct->add(aGerd);
  p->solve(ct, vl);

  cout << "\n?- grandchild-grandparent(Luca, Gerd).\n";
  vl.clear();
  ct = new CompoundTerm(aGrandchildGrandparent); ct->add(aLuca); ct->add(aGerd);
  p->solve(ct, vl);

  vl.clear();
  cout << "\n?- grandchild-grandparent(?grandchild, ?grandparent).\n";
  v1 = new Variable(aGrandchild);
  v2 = new Variable(aGrandparent);
  ct = new CompoundTerm(aGrandchildGrandparent); ct->add(v1); ct->add(v2);
  vl.push_back(v1); vl.push_back(v2); p->solve(ct, vl);

  return 0;
}
