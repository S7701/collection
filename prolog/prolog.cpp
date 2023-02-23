#include <cassert>
#include <cstring>
#include <iostream>
#include <list>

using namespace std;

bool debug = false;

void indent(unsigned int n) {
  for (unsigned int i = 0; i < n; ++i) cout << "    ";
}

class CompoundTerm;
class Variable;

class Term {
public: // statics
  static void Print(const list<Term*> &l, const char *s) {
    for (auto it = l.begin();;) {
      (*it)->print();
      if (++it == l.end()) break;
      cout << s;
    }
  }
public:
  virtual void print() const = 0;
  virtual bool unify(Term *t) = 0;
  virtual bool unifyCompoundTerm(CompoundTerm *ct) { return false; }
  virtual bool unifyVariable(Variable *v) { return false; }
};

class CompoundTerm: public Term { // functor(arg0, arg1 ... argN).
private:
  const char *functor;
  unsigned int arity;
  list<Term*> args;
public:
  CompoundTerm(const char *f): functor(f), arity(0) {}
  void addArg(Term *arg) { args.push_back(arg); ++arity; }
  void print() const {
    cout << functor;
    if (!args.empty()) {
      cout << "(";
      Term::Print(args, ", ");
      cout << ")";
    }
  };
  bool unify(Term *t) { return t->unifyCompoundTerm(this); }
  bool unifyCompoundTerm(CompoundTerm *ct) {
    if (functor != ct->functor || arity != ct->arity)
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
  static Variable *GetTrailhead() { return trail.front(); }
  static void Rewind(Variable *to) {
    Variable *v = trail.front();
    while (v != to) {
      v->reset();
      trail.pop_front();
      v = trail.front();
    }
  }
private:
  const char *name;
  unsigned int id;
  Term *term;
public:
  Variable(): name(""), id(++count), term(NULL) {}
  Variable(const char *n): name(n), id(++count), term(NULL) {}
  const char *getName() const { return name; }
  void print() const {
    if (term != NULL)
      term->print();
    else
      cout << "?" << name << "#" << id;
  };
  bool unify(Term *t) {
    if (term != NULL)
      return term->unify(t);
    trail.push_front(this);
    term = t;
    return true;
  }
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
  void addGoal(Term *t) { body.push_back(t); }
  void print() const {
    head->print();
    cout << " :- ";
    if (body.empty())
      cout << "true";
    else
      Term::Print(body, ", ");
  }
};

class Program {
private:
  list<Clause*> clauses;
public:
  void addClause(Clause *c) { clauses.push_back(c); }
  int solve(list<Term*> &ql, list<Variable*> &vl, unsigned int level = 0) {
    int solved = 0;
    Term *q = ql.front();
    ql.pop_front();
    if (debug) { indent(level); cout << "solve@"  << level << ": "; q->print(); cout << endl; }
    for (auto it = clauses.begin(); it != clauses.end(); ++it) {
      auto trailhead = Variable::GetTrailhead();
      Clause *c = (*it);
      Term *head = c->getHead();
      if (debug) { indent(level); cout << "  try: "; c->print(); cout << endl; }
      if (q->unify(head)) {
        if (debug) { indent(level); cout << "  MATCH" << endl; }
        list<Term*> goals(c->getBody()); // copy goals from clause
        goals.insert(goals.end(), ql.begin(), ql.end()); // append ql to goals
        if (goals.empty()) {
          ++solved;
          if (!vl.empty()) {
            for (auto it = vl.begin(); it != vl.end(); ++it) {
              Variable* v = (*it);
              cout << v->getName() << " = ";
              v->print();
              cout << endl;
            }
            cout << endl;
          }
        }
        else {
          solved += solve(goals, vl, level+1);
        }
      }
      else {
        if (debug) { indent(level); cout << "  no match" << endl; }
      }
      Variable::Rewind(trailhead);
    }
    if (level == 0) {
      if (solved) {
        cout << "true";
        if (solved > 1) cout << " (" << solved << " solutions)";
        cout << endl;
      }
      else
        cout << "false" << endl;
    }
    return solved;
  }
  int solve(Term *q, list<Variable*> &vl) {
    list<Term*> ql;
    ql.push_back(q);
    return solve(ql, vl);
  }
};

// program: (clause | query)+.
// clause: head [":-" body] '.'.
// query: "?-" goals '.'.
// head: term.
// body: goals.
// goals: term (',' term)*.
// term: variable | compound-term.
// variable: '?' name.
// compound-term: name '(' [term (',' term)*] ')'.
// name: (printable)+ | '\'' (printable | whitespcae)+ '\''.

void sample_test_program() {
  Term *tGerd = new CompoundTerm("Gerd");
  Term *tLaura = new CompoundTerm("Laura");
  Term *tKarlHeinz = new CompoundTerm("Karl-Heinz");
  Term *tDoris = new CompoundTerm("Doris");
  Term *tMike = new CompoundTerm("Mike");
  Term *tNicki = new CompoundTerm("Nicki");
  Term *tNico = new CompoundTerm("Nico");
  Term *tLuca = new CompoundTerm("Luca");
  const char *aGrandchildGrandparent = "grandchild-grandparent";
  const char *aChildParent = "child-parent";
  const char *aGrandchild = "grandchild";
  const char *aChild = "child";
  const char *aParent = "parent";
  const char *aGrandparent = "grandparent";
  const char *aX = "x";

  Program *p = new Program();
  Clause *c;
  CompoundTerm *ct;
  Variable *v1, *v2, *v3;
  list<Variable*> vl;

  // Facts:

  // child-parent(Mike, Gerd).
  ct = new CompoundTerm(aChildParent); ct->addArg(tMike); ct->addArg(tGerd);
  p->addClause(new Clause(ct));

  // child-parent(Mike, Laura).
  ct = new CompoundTerm(aChildParent); ct->addArg(tMike); ct->addArg(tLaura);
  p->addClause(new Clause(ct));

  // child-parent(Nicki, Karl-Heinz).
  ct = new CompoundTerm(aChildParent); ct->addArg(tNicki); ct->addArg(tKarlHeinz);
  p->addClause(new Clause(ct));

  // child-parent(Nicki, Doris).
  ct = new CompoundTerm(aChildParent); ct->addArg(tNicki); ct->addArg(tDoris);
  p->addClause(new Clause(ct));

  // child-parent(Nico, Mike).
  ct = new CompoundTerm(aChildParent); ct->addArg(tNico); ct->addArg(tMike);
  p->addClause(new Clause(ct));

  // child-parent(Luca, Mike).
  ct = new CompoundTerm(aChildParent); ct->addArg(tLuca); ct->addArg(tMike);
  p->addClause(new Clause(ct));

  // child-parent(Nico, Nicki).
  ct = new CompoundTerm(aChildParent); ct->addArg(tNico); ct->addArg(tNicki);
  p->addClause(new Clause(ct));

  // child-parent(Luca, Nicki).
  ct = new CompoundTerm(aChildParent); ct->addArg(tLuca); ct->addArg(tNicki);
  p->addClause(new Clause(ct));

  // Rules:

  // grandchild-grandparent(?grandchild, ?grandparent) :- child-parent(?grandchild, ?x), child-parent(?x, ?grandparent).
  v1 = new Variable(aGrandchild);
  v2 = new Variable(aGrandparent);
  v3 = new Variable(aX);
  ct = new CompoundTerm(aGrandchildGrandparent); ct->addArg(v1); ct->addArg(v2);
  c = new Clause(ct);
  ct = new CompoundTerm(aChildParent); ct->addArg(v1); ct->addArg(v3);
  c->addGoal(ct);
  ct = new CompoundTerm(aChildParent); ct->addArg(v3); ct->addArg(v2);
  c->addGoal(ct);
  p->addClause(c);

  // Queries:

  vl.clear();
  cout << "?- child-parent(?child, Nicki)." << endl;
  v1 = new Variable(aChild); vl.push_back(v1);
  ct = new CompoundTerm(aChildParent); ct->addArg(v1); ct->addArg(tNicki);
  p->solve(ct, vl);

  vl.clear();
  cout << "\n?- child-parent(?child, Luca)." << endl;
  v1 = new Variable(aChild); vl.push_back(v1);
  ct = new CompoundTerm(aChildParent); ct->addArg(v1); ct->addArg(tLuca);
  p->solve(ct, vl);

  vl.clear();
  cout << "\n?- child-parent(?child, ?parent)." << endl;
  v1 = new Variable(aChild); vl.push_back(v1);
  v2 = new Variable(aParent); vl.push_back(v2);
  ct = new CompoundTerm(aChildParent); ct->addArg(v1); ct->addArg(v2);
  p->solve(ct, vl);

  cout << "\n?- child-parent(Nico, Mike)." << endl;
  vl.clear();
  ct = new CompoundTerm(aChildParent); ct->addArg(tNico); ct->addArg(tMike);
  p->solve(ct, vl);

  cout << "\n?- child-parent(Luca, Gerd)." << endl;
  vl.clear();
  ct = new CompoundTerm(aChildParent); ct->addArg(tLuca); ct->addArg(tGerd);
  p->solve(ct, vl);

  cout << "\n?- grandchild-grandparent(Luca, Gerd)." << endl;
  vl.clear();
  ct = new CompoundTerm(aGrandchildGrandparent); ct->addArg(tLuca); ct->addArg(tGerd);
  p->solve(ct, vl);

  vl.clear();
  cout << "\n?- grandchild-grandparent(?grandchild, ?grandparent)." << endl;
  v1 = new Variable(aGrandchild); vl.push_back(v1);
  v2 = new Variable(aGrandparent); vl.push_back(v2);
  ct = new CompoundTerm(aGrandchildGrandparent); ct->addArg(v1); ct->addArg(v2);
  p->solve(ct, vl);
}

int main(int argc, const char* const argv[]) {
  if (argc > 1 && strcmp(argv[1], "-d") == 0) debug = true;

  sample_test_program();

  return 0;
}
