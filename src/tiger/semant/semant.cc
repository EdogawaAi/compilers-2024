#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"
#include <iostream>

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto* entry = venv->Look(sym_);
  if(entry && typeid(*entry) == typeid(env::VarEntry)){
    return (static_cast<env::VarEntry*>(entry))->ty_->ActualTy();
  }
  else{
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().c_str());
  }
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*var_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  auto fieldList = static_cast<type::RecordTy *>(var_ty)->fields_;
  for (auto field : fieldList->GetList()) {
    if (field->name_->Name() == sym_->Name()) {
      return field->ty_;
    }
  }

  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());
  return type::IntTy::Instance();
}


type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto var_actual_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  auto subscript_ty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (typeid(var_actual_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }

  if (typeid(subscript_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "integer required");
    return type::IntTy::Instance();
  }

  return static_cast<type::ArrayTy *>(var_actual_ty)->ty_;

}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}


type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto *fun_entry = venv->Look(func_);
  if (fun_entry == nullptr || typeid(*fun_entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
    return type::IntTy::Instance();
  }

  auto fun_enrty_ptr = static_cast<env::FunEntry *>(fun_entry);

  auto &argList = args_->GetList();
  auto &formatList = fun_enrty_ptr->formals_->GetList();

  auto argIter = argList.begin();
  auto formatIter = formatList.begin();

  while (argIter != argList.end() && formatIter != formatList.end()) {
    auto *arg_ty = (*argIter)->SemAnalyze(venv, tenv, labelcount, errormsg);
    auto *format_ty = (*formatIter);
    if (!arg_ty->IsSameType(format_ty)) {
      errormsg->Error(pos_, "para type mismatch");
      return type::IntTy::Instance();
    }
    ++argIter;
    ++formatIter;
  }

  if (argIter != argList.end() || formatIter != formatList.end()) {
    errormsg->Error(pos_, "too many params in function %s", func_->Name().c_str());
    return type::IntTy::Instance();
  }

  if (!fun_enrty_ptr->result_) {
    return type::VoidTy::Instance();
  }

  return fun_enrty_ptr->result_;

}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto left_ty = left_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto right_ty = right_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (oper_ == PLUS_OP || oper_ == MINUS_OP || oper_ == TIMES_OP || oper_ == DIVIDE_OP) {
    if (!left_ty->IsSameType(type::IntTy::Instance()) || !right_ty->IsSameType(type::IntTy::Instance())) {
      errormsg->Error(pos_, "integer required");
    }
    return type::IntTy::Instance();
  } else {
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
      return type::IntTy::Instance();
    }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto record_ty = tenv->Look(typ_);
  if (!record_ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
    return type::IntTy::Instance();
  }
  return record_ty;
}


type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *result_ty; //不能从 type::Ty * 赋值到 std::nullptr_t 类型
  for (auto &exp : seq_->GetList()) {
    result_ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return result_ty;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto *exp_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (!var_ty->IsSameType(exp_ty)) {
    errormsg->Error(pos_, "unmatched assign exp");
    return type::VoidTy::Instance();
  }

  if (typeid(*var_) == typeid(SimpleVar)) {
    auto *simple_var = static_cast<SimpleVar *>(var_);
    if (venv->Look(simple_var->sym_)->readonly_) {
      errormsg->Error(pos_, "loop variable can't be assigned");
      return type::VoidTy::Instance();
    }
  }

  return type::VoidTy::Instance();
}


type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  auto *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto *then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto *else_ty = elsee_ ? elsee_->SemAnalyze(venv, tenv, labelcount, errormsg) : nullptr;

  if (!test_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(test_->pos_, "if test exp should be int"); // test10
    return type::VoidTy::Instance();
  }

  if (!elsee_ && typeid(*then_ty) != typeid(type::VoidTy)) {
    errormsg->Error(then_->pos_,
                    "if-then exp's body must produce no value"); // test15
    return type::VoidTy::Instance();
  }

  if (elsee_ && !then_ty->IsSameType(else_ty)) {
    errormsg->Error(elsee_->pos_,
                    "then exp and else exp type mismatch"); // test9
    return then_ty;
  }

  return then_ty;
}


type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope(); tenv->BeginScope();

  auto body_ty = body_->SemAnalyze(venv, tenv, -1, errormsg);
  if (typeid(body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "while body must produce no value");
    return type::VoidTy::Instance();
  }

  venv->EndScope(); tenv->EndScope();

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  auto *low_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto *high_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!low_ty->IsSameType(type::IntTy::Instance()) || !high_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(pos_, "for exp's range type is not integer");
  }

  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
  auto *body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  if (!body_ty->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "for body must produce no value");
  }
  venv->EndScope();
  return type::VoidTy::Instance();
}



type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (labelcount == 0) {
    errormsg->Error(pos_, "break statement not within loop");
  }

  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope(); tenv->BeginScope();

  for (auto decl : decs_->GetList()) {
    decl->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  auto body_ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);

  venv->EndScope(); tenv->EndScope();

  return body_ty;
}
type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto array_ty = tenv->Look(typ_)->ActualTy();
  if (!array_ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
  }
  if (typeid(array_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "not array type", typ_->Name().c_str());
  }

  auto *arr_ele_ty = static_cast<type::ArrayTy *>(array_ty)->ty_;
  auto *size_ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  auto *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (!size_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(pos_, "array size should be integer");
  }
  if (arr_ele_ty->IsSameType(init_ty) == false) {
    errormsg->Error(pos_, "type mismatch");
  }

  return array_ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto fun_list = functions_->GetList();
  auto fun_list_iter = fun_list.begin();

  while (fun_list_iter != fun_list.end()) {
    if (venv->Look((*fun_list_iter)->name_) != nullptr) {
      errormsg->Error(pos_, "two functions have the same name");
      return;
    }
    auto formals = (*fun_list_iter)->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *result_ty = type::VoidTy::Instance();
    if ((*fun_list_iter)->result_) {
      result_ty = tenv->Look((*fun_list_iter)->result_);
      if (!result_ty) {
        errormsg->Error(pos_, "undefined type %s", (*fun_list_iter)->result_->Name().c_str());
        return;
      }
    }
    venv->Enter((*fun_list_iter)->name_, new env::FunEntry(formals, result_ty));
    fun_list_iter++;
  }

  fun_list_iter = fun_list.begin();
  while (fun_list_iter != fun_list.end()) {
    venv->BeginScope();

    auto fun_entry = static_cast<env::FunEntry *>(venv->Look((*fun_list_iter)->name_));
    auto &formals = fun_entry->formals_->GetList();
    auto &params = (*fun_list_iter)->params_->GetList();

    auto format_iter = formals.begin();
    auto param_iter = params.begin();
    while (param_iter != params.end()) {
      venv->Enter((*param_iter)->name_, new env::VarEntry(*format_iter, false));
      param_iter++;
    }

    auto body_ty = (*fun_list_iter)->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!body_ty->IsSameType(fun_entry->result_)) {
      errormsg->Error(pos_, "procedure returns value");
    }

    venv->EndScope();

    ++fun_list_iter;
  }
}


void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typ_ == nullptr) {
    if (init_ty->IsSameType(type::NilTy::Instance())) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
    venv->Enter(var_, new env::VarEntry(init_ty));
  } else {
    auto typ_ty = tenv->Look(typ_);
    if (!typ_ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
      venv->Enter(var_, new env::VarEntry(init_ty));
      return;
    }
    if (!init_ty->IsSameType(typ_ty)) {
      errormsg->Error(pos_, "type mismatch");
    }
    venv->Enter(var_, new env::VarEntry(typ_ty));
  }
}


void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto type_list = types_->GetList();
  auto type_list_iter = type_list.begin();
  while (type_list_iter != type_list.end()) {
    if (tenv->Look((*type_list_iter)->name_) != nullptr) {
      errormsg->Error(pos_, "two types have the same name");
      return;
    }
    tenv->Enter((*type_list_iter)->name_, new type::NameTy((*type_list_iter)->name_, nullptr));
    type_list_iter++;
  }

  type_list_iter = type_list.begin();
  while (type_list_iter != type_list.end()) {
    auto name_ty = static_cast<type::NameTy *>(tenv->Look((*type_list_iter)->name_));
    name_ty->ty_ = (*type_list_iter)->ty_->SemAnalyze(tenv, errormsg);
    type_list_iter++;
  }

  bool loopness = false;
  type_list_iter = type_list.begin();
  while (type_list_iter != type_list.end()) {
    auto name_ty = static_cast<type::NameTy *>(tenv->Look((*type_list_iter)->name_));
    auto name_ty_iter = name_ty;

    while (true) {
      auto* next_ty = name_ty_iter->ty_;
      if (typeid(*next_ty) != typeid(type::NameTy)) {
        break;
      } else {
        if (static_cast<type::NameTy *>(next_ty)->sym_ == name_ty->sym_) {
          loopness = true;
          break;
        }
      }
      name_ty_iter = static_cast<type::NameTy *>(next_ty);
    }
    if (loopness) {
      errormsg->Error(pos_, "illegal type cycle");
      break;
    }

    ++type_list_iter;
  }
}


type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto name_ty = tenv->Look(name_);
  if (!name_ty) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().c_str());
    return type::IntTy::Instance();
  }
  return new type::NameTy(name_, name_ty);
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  auto array_ty = tenv->Look(array_);
  if (!array_ty) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().c_str());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(array_ty);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}
} // namespace sem