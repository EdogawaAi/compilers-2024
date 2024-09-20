#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(stm1->MaxArgs(), stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return stm2->Interp(stm1->Interp(t));
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable* int_and_table = exp->Interp(t);
  return int_and_table->t->Update(id, int_and_table->i);
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(exps->exp_num(), exps->MaxArgs());
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return exps->PrintSumInterp(t);
}


int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
}
// idExp
int A::IdExp::MaxArgs() const {
  return 0;
}

A::IntAndTable *A::IdExp::Interp(Table *t) const {
  return new IntAndTable(t->Lookup(id), t);
}

//NumExp
int A::NumExp::MaxArgs() const {
  return 0;
}

A::IntAndTable *A::NumExp::Interp(Table *t) const {
  return new IntAndTable(num, t);
}

//OpExp
int A::OpExp::MaxArgs() const {
  return std::max(left->MaxArgs(), right->MaxArgs());
}

A::IntAndTable *A::OpExp::Interp(Table *t) const {
  auto left_result = left->Interp(t);
  auto right_result = right->Interp(t);

  int result;
  switch (oper) {
  case A::PLUS:
    result = left_result->i + right_result->i;
    break;
  case A::MINUS:
    result = left_result->i - right_result->i;
    break;
  case A::TIMES:
    result = left_result->i * right_result->i;
    break;
  case A::DIV:
    result = left_result->i / right_result->i;
    break;
    default:
      assert(false);
  }

  return new IntAndTable(result, right_result->t);
}

// EseqExp
int A::EseqExp::MaxArgs() const {
  return std::max(exp->MaxArgs(), stm->MaxArgs());
}

A::IntAndTable *A::EseqExp::Interp(Table *t) const {
  return exp->Interp(stm->Interp(t));
}

//PairExpList
int A::PairExpList::MaxArgs() const {
  return std::max(exp->MaxArgs(), tail->MaxArgs());
}

A::Table *A::PairExpList::PrintSumInterp(Table *t) const {
  IntAndTable *int_and_table = exp->Interp(t);
  std::cout << int_and_table->i << '\x20';
  return tail->PrintSumInterp(int_and_table->t);
}

int A::PairExpList::exp_num() const {
  return 1 + tail->exp_num();
}

//LastExpList
int A::LastExpList::MaxArgs() const {
  return exp->MaxArgs();
}

int A::LastExpList::exp_num() const {
  return 1;
}

A::Table *A::LastExpList::PrintSumInterp(Table *t) const {
  IntAndTable *int_and_table = exp->Interp(t);
  std::cout << int_and_table->i << std::endl;
  return int_and_table->t;
}
// namespace A
