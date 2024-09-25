#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(stm1->MaxArgs() , stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *table) const {
  // TODO: put your code here (lab1).
  Table *newTable = stm1->Interp(table);
  return stm2->Interp(newTable);
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *table) const {
  // TODO: put your code here (lab1).
  auto int_and_table = exp->Interp(table);
  return int_and_table->t->Update(id, int_and_table->i);
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(exps->NumExps(), exps->MaxArgs());
}

Table *A::PrintStm::Interp(Table *table) const {
  // TODO: put your code here (lab1).
  return exps->InterpPrint(table)->t;

}

int A::IdExp::MaxArgs() const {
  return 0;
}

IntAndTable *IdExp::Interp(Table *table) const {
  return new IntAndTable(table->Lookup(id), table);
}

int A::NumExp::MaxArgs() const {
  return 0;
}

IntAndTable *NumExp::Interp(Table *table) const {
  return new IntAndTable(num, table);
}

int OpExp::MaxArgs() const {
  return std::max(left->MaxArgs(), right->MaxArgs());
}

IntAndTable *OpExp::Interp(Table *table) const {
  auto left_result = left->Interp(table);
  auto right_result = right->Interp(table);

  int result = 0;

  switch (oper) {
  case PLUS:
    result = left_result->i + right_result->i;
    break;

  case MINUS:
    result = left_result->i - right_result->i;
    break;

  case TIMES:
    result = left_result->i * right_result->i;
    break;

  case DIV:
    if (right_result->i != 0) {
      result = left_result->i / right_result->i;
    }
    break;

    default:
      assert(false);
  }

  return new IntAndTable(result, right_result->t);

}

int EseqExp::MaxArgs() const {
  return std::max(stm->MaxArgs(), exp->MaxArgs());
}

IntAndTable *EseqExp::Interp(Table *table) const {
  return exp->Interp(stm->Interp(table));
}

int PairExpList::MaxArgs() const {
  return std::max(exp->MaxArgs(), tail->MaxArgs());
}

IntAndTable *PairExpList::InterpPrint(Table *table) const {
  auto int_and_table = exp->Interp(table);
  std::cout << int_and_table->i << " ";
  return tail->InterpPrint(int_and_table->t);
}

int PairExpList::NumExps() const {
  return 1 + tail->NumExps();
}

int LastExpList::MaxArgs() const {
  return exp->MaxArgs();
}

int LastExpList::NumExps() const {
  return 1;
}

IntAndTable *LastExpList::InterpPrint(Table *table) const {
  auto int_and_table = exp->Interp(table);
  std::cout << int_and_table->i << std::endl;
  return int_and_table;
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
}  // namespace A
