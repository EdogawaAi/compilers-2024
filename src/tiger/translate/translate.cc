#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <stack>

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;
extern llvm::IRBuilder<> *ir_builder;
extern llvm::Module *ir_module;
std::stack<llvm::Function *> func_stack;
std::stack<llvm::BasicBlock *> loop_stack;
llvm::Function *alloc_record;
llvm::Function *init_array;
llvm::Function *string_equal;
std::vector<std::pair<std::string, frame::Frame *>> frame_info;

bool CheckBBTerminatorIsBranch(llvm::BasicBlock *bb) {
  auto inst = bb->getTerminator();
  if (inst) {
    llvm::BranchInst *branchInst = llvm::dyn_cast<llvm::BranchInst>(inst);
    if (branchInst && !branchInst->isConditional()) {
      return true;
    }
  }
  return false;
}

int getActualFramesize(tr::Level *level) {
  return level->frame_->calculateActualFramesize();
}

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  return new Access(level, level->frame_->AllocLocal(escape));
}

class ValAndTy {
public:
  type::Ty *ty_;
  llvm::Value *val_;

  ValAndTy(llvm::Value *val, type::Ty *ty) : val_(val), ty_(ty) {}
};

void ProgTr::OutputIR(std::string_view filename) {
  std::string llvmfile = std::string(filename) + ".ll";
  std::error_code ec;
  llvm::raw_fd_ostream out(llvmfile, ec, llvm::sys::fs::OpenFlags::OF_Text);
  ir_module->print(out, nullptr);
}

void ProgTr::Translate() {
  FillBaseVEnv();
  FillBaseTEnv();
  /* TODO: Put your lab5-part1 code here */
  auto *alloc_record_func_type = llvm::FunctionType::get(ir_builder->getInt64Ty(), {ir_builder->getInt32Ty()}, false);
  alloc_record = llvm::Function::Create(alloc_record_func_type, llvm::Function::ExternalLinkage, "alloc_record", ir_module);

  auto *init_arr_func_type = llvm::FunctionType::get(ir_builder->getInt64Ty(), {ir_builder->getInt32Ty(), ir_builder->getInt64Ty()}, false);
  init_array = llvm::Function::Create(init_arr_func_type, llvm::Function::ExternalLinkage, "init_array", ir_module);

  auto *str_eq_func_type = llvm::FunctionType::get(ir_builder->getInt1Ty(), {type::StringTy::Instance()->GetLLVMType(), type::StringTy::Instance()->GetLLVMType()}, false);
  string_equal = llvm::Function::Create(str_eq_func_type, llvm::Function::ExternalLinkage, "string_equal", ir_module);

  auto *tiget_mainfunc_type = llvm::FunctionType::get(ir_builder->getInt32Ty(), {ir_builder->getInt64Ty(), ir_builder->getInt64Ty()}, false);
  auto *tiger_main = llvm::Function::Create(tiget_mainfunc_type, llvm::Function::ExternalLinkage, "tigermain", ir_module);

  main_level_->set_sp(tiger_main->arg_begin());
  main_level_->frame_->framesize_global = new llvm::GlobalVariable(ir_builder->getInt64Ty(), true, llvm::GlobalValue::InternalLinkage, ir_builder->getInt64(0), "tigermain_framesize_global");
  ir_module->getGlobalList().push_back(main_level_->frame_->framesize_global);

  func_stack.push(tiger_main);
  auto *tigermain_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "tidgermain", tiger_main);

  ir_builder->SetInsertPoint(tigermain_bb);
  auto *main_result = absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(), errormsg_.get());

  main_level_->frame_->framesize_global->setInitializer(ir_builder->getInt64(getActualFramesize(main_level_.get())));
  if (dynamic_cast<type::VoidTy *>(main_result->ty_)) {
    ir_builder->CreateRet(ir_builder->getInt32(0));
  }
  else {
    ir_builder->CreateRet(main_result->val_);
  }
  func_stack.pop();
}

} // namespace tr

namespace absyn {

tr::ValAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return root_->Translate(venv, tenv, level, errormsg);
}

void TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                        err::ErrorMsg *errormsg) const {
 /* TODO: Put your lab5-part1 code here */
  for (auto *type : types_->GetList()) {
    tenv->Enter(type->name_, new type::NameTy(type->name_, nullptr));
  }
  for (auto *type : types_->GetList()) {
    auto *name_ty = dynamic_cast<type::NameTy *>(tenv->Look(type->name_));
    name_ty->ty_ = type->ty_->Translate(tenv, errormsg);
  }
}

void FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  for (auto *func : functions_->GetList()) {
    auto *formals = func->params_->MakeFormalTyList(tenv, errormsg);
    auto *result_ty = func->result_ ? tenv->Look(func->result_) : type::VoidTy::Instance();

    std::vector<llvm::Type *> llvm_formals = {ir_builder->getInt64Ty(), ir_builder->getInt64Ty()};

    for (auto *ty : formals->GetList()) {
      llvm_formals.push_back(ty->GetLLVMType());
    }

    std::list<bool> formals_escape;
    for (auto *field : func->params_->GetList()) {
      formals_escape.push_back(field->escape_);
    }

    auto *new_level = level->NewLevel(temp::LabelFactory::NamedLabel(func->name_->Name()), formals_escape);
    frame_info.push_back({func->name_->Name(), new_level->frame_});

    auto *func_type = llvm::FunctionType::get(result_ty->GetLLVMType(), llvm_formals, false);
    auto *llvm_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, func->name_->Name(), ir_module);
    venv->Enter(func->name_, new env::FunEntry(new_level, formals, result_ty, func_type, llvm_func));
  }

  auto *cur_bb = ir_builder->GetInsertBlock();

  for (auto *func : functions_->GetList()) {
    auto *func_entry = dynamic_cast<env::FunEntry *>(venv->Look(func->name_));
    func_entry->level_->frame_->framesize_global = new llvm::GlobalVariable(ir_builder->getInt64Ty(), false, llvm::GlobalValue::InternalLinkage, ir_builder->getInt64(0), func->name_->Name() + "_framesize_global");
    ir_module->getGlobalList().push_back(func_entry->level_->frame_->framesize_global);

    func_stack.push(func_entry->func_);
    auto *body_bb = llvm::BasicBlock::Create(ir_builder->getContext(), func->name_->Name(), func_entry->func_);

    ir_builder->SetInsertPoint(body_bb);
    venv->BeginScope();
    tenv->BeginScope();

    auto formal_ty_iter = func_entry->formals_->GetList().begin();
    auto param_iter = func->params_->GetList().begin();
    auto access_iter = func_entry->level_->frame_->Formals()->begin();
    auto args_iter = func_entry->func_->arg_begin();
    func_entry->level_->set_sp(args_iter++);
    auto *sl_addr = (*access_iter++)->ToLLVMVal(func_entry->level_->get_sp());
    auto *sl_ptr = ir_builder->CreateIntToPtr(sl_addr, ir_builder->getInt64Ty()->getPointerTo());
    ir_builder->CreateStore(args_iter++, sl_ptr);
    while (param_iter != func->params_->GetList().end()) {
      auto *param_addr = (*access_iter)->ToLLVMVal(func_entry->level_->get_sp());
      auto *param_ptr = ir_builder->CreateIntToPtr(param_addr, (*formal_ty_iter)->GetLLVMType()->getPointerTo());
      ir_builder->CreateStore(args_iter, param_ptr);

      auto *tr_access = new tr::Access(func_entry->level_, *access_iter);
      venv->Enter((*param_iter)->name_, new env::VarEntry(tr_access, (*formal_ty_iter)->ActualTy()));


      formal_ty_iter++;
      access_iter++;
      param_iter++;
      args_iter++;
    }

    auto *result = func->body_->Translate(venv, tenv, func_entry->level_, errormsg);
    if (result == nullptr || dynamic_cast<type::VoidTy *>(result->ty_)) {
      ir_builder->CreateRetVoid();
    }
    else if (result->val_ != nullptr && func_entry->func_->getReturnType() != result->val_->getType() && result->ty_->IsSameType(type::IntTy::Instance())) {
      auto *val = ir_builder->CreateZExt(result->val_, func_entry->func_->getReturnType());
      ir_builder->CreateRet(val);
    }
    else {
      ir_builder->CreateRet(result->val_);
    }

    venv->EndScope();
    tenv->EndScope();
    func_stack.pop();

    func_entry->level_->frame_->framesize_global->setInitializer(ir_builder->getInt64(getActualFramesize(func_entry->level_)));
  }

  ir_builder->SetInsertPoint(cur_bb);
}

void VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                       err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *init = init_->Translate(venv, tenv, level, errormsg);
  auto *access = tr::Access::AllocLocal(level, escape_);
  auto *var_addr = access->access_->ToLLVMVal(level->get_sp());
  auto *var_ptr = ir_builder->CreateIntToPtr(var_addr, init->ty_->GetLLVMType()->getPointerTo());
  ir_builder->CreateStore(init->val_, var_ptr);
  venv->Enter(var_, new env::VarEntry(access, init->ty_->ActualTy()));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return tenv->Look(name_)->ActualTy();
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return new type::RecordTy(record_->MakeFieldList(tenv, errormsg));
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return new type::ArrayTy(tenv->Look(array_)->ActualTy());
}

tr::ValAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *var_entry = dynamic_cast<env::VarEntry *>(venv->Look(sym_));
  auto *sp = level->get_sp();
  while (level != var_entry->access_->level_) {
    auto sl_formal = level->frame_->Formals()->begin();
    auto *static_link_addr = (*sl_formal)->ToLLVMVal(sp);
    auto *static_link_ptr = ir_builder->CreateIntToPtr(static_link_addr, ir_builder->getInt64Ty()->getPointerTo());
    sp = ir_builder->CreateLoad(ir_builder->getInt64Ty(), static_link_ptr);
    level = level->parent_;
  }

  auto *var_addr = var_entry->access_->access_->ToLLVMVal(sp);
  auto *var_ptr = ir_builder->CreateIntToPtr(var_addr, var_entry->ty_->ActualTy()->GetLLVMType()->getPointerTo());
  return new tr::ValAndTy(var_ptr, var_entry->ty_->ActualTy());
}

tr::ValAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *var = var_->Translate(venv, tenv, level, errormsg);
  auto *record_ty = dynamic_cast<type::RecordTy *>(var->ty_);
  auto *record_ptr = ir_builder->CreateLoad(var->ty_->GetLLVMType(), var->val_);

  int index = 0;
  for (auto *field : record_ty->fields_->GetList()) {
    if (field->name_->Name() == sym_->Name()) {
      auto *field_ptr = ir_builder->CreateStructGEP(var->ty_->GetLLVMType()->getPointerElementType(), record_ptr, index);
      return new tr::ValAndTy(field_ptr, field->ty_->ActualTy());
    }
    index++;
  }
  return new tr::ValAndTy(nullptr, type::NilTy::Instance());
}

tr::ValAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *var = var_->Translate(venv, tenv, level, errormsg);
  auto *subscript = subscript_->Translate(venv, tenv, level, errormsg);
  auto *arr_ptr = ir_builder->CreateLoad(var->ty_->GetLLVMType(), var->val_);
  auto *target_ptr = ir_builder->CreateGEP(var->ty_->GetLLVMType()->getPointerElementType(), arr_ptr, subscript->val_);
  auto *arr_ty = dynamic_cast<type::ArrayTy *>(var->ty_);
  return new tr::ValAndTy(target_ptr, arr_ty->ty_->ActualTy());
}

tr::ValAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *var = var_->Translate(venv, tenv, level, errormsg);
  auto *val = ir_builder->CreateLoad(var->ty_->GetLLVMType(), var->val_);
  return new tr::ValAndTy(val, var->ty_->ActualTy());
}

tr::ValAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return new tr::ValAndTy(nullptr, type::NilTy::Instance());
}

tr::ValAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return new tr::ValAndTy(ir_builder->getInt32(val_), type::IntTy::Instance());
}

tr::ValAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return new tr::ValAndTy(type::StringTy::CreateGlobalStringStructPtr(str_), type::StringTy::Instance());
}

tr::ValAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *func_entry = dynamic_cast<env::FunEntry *>(venv->Look(func_));
  std::vector<llvm::Value *> args;
  for (auto *arg : args_->GetList()) {
    auto *val_and_ty = arg->Translate(venv, tenv, level, errormsg);
    args.push_back(val_and_ty->val_);
  }

  if (func_entry->level_->parent_ == nullptr) {
    level->frame_->AllocOutgoSpace(func_entry->formals_->GetList().size() * reg_manager->WordSize());
  }
  else {
    level->frame_->AllocOutgoSpace((func_entry->formals_->GetList().size() + 1) * reg_manager->WordSize());
    auto *frame_size_global = ir_builder->CreateLoad(ir_builder->getInt64Ty(), level->frame_->framesize_global);
    auto *new_sp = ir_builder->CreateSub(level->get_sp(), frame_size_global);
    args.insert(args.begin(), new_sp);

    if (func_entry->level_->parent_ == level) {
      args.insert(std::next(args.begin()), level->get_sp());
    }
    else {
      auto *temp_level = func_entry->level_;
      auto *sl_val = level->get_sp();
      while (level->parent_ != temp_level) {
        auto sl_formal = level->frame_->Formals()->begin();
        auto *sl_addr = (*sl_formal)->ToLLVMVal(level->get_sp());
        auto *sl_ptr = ir_builder->CreateIntToPtr(sl_addr, ir_builder->getInt64Ty()->getPointerTo());
        sl_val = ir_builder->CreateLoad(ir_builder->getInt64Ty(), sl_ptr);
        level = level->parent_;
        temp_level = temp_level->parent_->parent_;
      }
      args.insert(std::next(args.begin()), sl_val);
    }
  }

  auto *result = ir_builder->CreateCall(func_entry->func_, args);
  if (func_entry->result_ == nullptr || dynamic_cast<type::VoidTy *>(func_entry->result_)) {
    return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  }
  return new tr::ValAndTy(result, func_entry->result_);
}

tr::ValAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *left = left_->Translate(venv, tenv, level, errormsg);
  auto *right = right_->Translate(venv, tenv, level, errormsg);
  llvm::Value *result = nullptr;

  if (oper_ == AND_OP || oper_ == OR_OP) {
    auto *func = func_stack.top();
    auto *left_test_bb = llvm::BasicBlock::Create(ir_builder->getContext(), oper_ == AND_OP ? "and_left_test" : "or_left_test", func);
    auto *right_test_bb = llvm::BasicBlock::Create(ir_builder->getContext(), oper_ == AND_OP ? "and_right_test" : "or_right_test", func);
    auto *next_bb = llvm::BasicBlock::Create(ir_builder->getContext(), oper_ == AND_OP ? "and_next" : "or_next", func);

    ir_builder->CreateBr(left_test_bb);
    ir_builder->SetInsertPoint(left_test_bb);
    auto *left_test = ir_builder->CreateICmpNE(left->val_, llvm::ConstantInt::get(left->val_->getType(), 0));
    if (oper_ == AND_OP) {
      ir_builder->CreateCondBr(left_test, right_test_bb, next_bb);
    }
    else {
      ir_builder->CreateCondBr(left_test, next_bb, right_test_bb);
    }

    ir_builder->SetInsertPoint(right_test_bb);
    auto *right_test = ir_builder->CreateICmpNE(right->val_, llvm::ConstantInt::get(right->val_->getType(), 0));
    ir_builder->CreateBr(next_bb);

    ir_builder->SetInsertPoint(next_bb);
    auto *phi = ir_builder->CreatePHI(ir_builder->getInt1Ty(), 2);
    phi->addIncoming(ir_builder->getInt1(oper_ == OR_OP), left_test_bb);
    phi->addIncoming(right_test, right_test_bb);
    result = phi;
    goto logical_op;
  }

  if (dynamic_cast<type::StringTy *>(left->ty_) && dynamic_cast<type::StringTy *>(right->ty_)) {
    result = ir_builder->CreateCall(string_equal, {left->val_, right->val_});
    result = ir_builder->CreateICmpEQ(result, ir_builder->getInt1(oper_ == EQ_OP));
    goto logical_op;
  }

  if (dynamic_cast<type::NilTy *>(left->ty_) || dynamic_cast<type::NilTy *>(right->ty_)) {
    if (dynamic_cast<type::NilTy *>(left->ty_) && dynamic_cast<type::NilTy *>(right->ty_)) {
      result = ir_builder->getInt1(oper_ == EQ_OP);
      goto logical_op;
    }
    if (dynamic_cast<type::NilTy *>(left->ty_)) {
      result = ir_builder->CreatePtrToInt(right->val_, ir_builder->getInt64Ty());
    }
    else {
      result = ir_builder->CreatePtrToInt(left->val_, ir_builder->getInt64Ty());
    }

    if (oper_ == EQ_OP) {
      result = ir_builder->CreateICmpEQ(result, ir_builder->getInt64(0));
    }
    else if (oper_ == NEQ_OP) {
      result = ir_builder->CreateICmpNE(result, ir_builder->getInt64(0));
    }
    goto logical_op;
  }

  if (oper_ == PLUS_OP) {
    result = ir_builder->CreateAdd(left->val_, right->val_);
  }
  else if (oper_ == MINUS_OP) {
    result = ir_builder->CreateSub(left->val_, right->val_);
  }
  else if (oper_ == TIMES_OP) {
    result = ir_builder->CreateMul(left->val_, right->val_);
  }
  else if (oper_ == DIVIDE_OP) {
    result = ir_builder->CreateSDiv(left->val_, right->val_);
  }
  else if (oper_ == LT_OP) {
    result = ir_builder->CreateICmpSLT(left->val_, right->val_);
    goto logical_op;
  }
  else if (oper_ == LE_OP) {
    result = ir_builder->CreateICmpSLE(left->val_, right->val_);
    goto logical_op;
  }
  else if (oper_ == GT_OP) {
    result = ir_builder->CreateICmpSGT(left->val_, right->val_);
    goto logical_op;
  }
  else if (oper_ == GE_OP) {
    result = ir_builder->CreateICmpSGE(left->val_, right->val_);
    goto logical_op;
  }
  else if (oper_ == EQ_OP) {
    result = ir_builder->CreateICmpEQ(left->val_, right->val_);
    goto logical_op;
  }
  else if (oper_ == NEQ_OP) {
    result = ir_builder->CreateICmpNE(left->val_, right->val_);
    goto logical_op;
  }
  return new tr::ValAndTy(result, type::IntTy::Instance());

  logical_op:
  auto *val = ir_builder->CreateZExt(result, ir_builder->getInt32Ty());
  return new tr::ValAndTy(val, type::IntTy::Instance());
}

tr::ValAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *record_ty = dynamic_cast<type::RecordTy *>(tenv->Look(typ_)->ActualTy());
  int record_size = record_ty->fields_->GetList().size() * reg_manager->WordSize();
  auto *record_addr = ir_builder->CreateCall(alloc_record, {ir_builder->getInt32(record_size)});
  auto *record_ptr = ir_builder->CreateIntToPtr(record_addr, record_ty->GetLLVMType());

  for (auto *efield : fields_->GetList()) {
    int index = 0;
    for (auto *field : record_ty->fields_->GetList()) {
      if (field->name_->Name() == efield->name_->Name()) {
        auto *exp = efield->exp_->Translate(venv, tenv, level, errormsg);
        auto *ptr = ir_builder->CreateStructGEP(record_ty->GetLLVMType()->getPointerElementType(), record_ptr, index);
        ir_builder->CreateStore(exp->val_, ptr);
        break;
      }
      index++;
    }
  }
  return new tr::ValAndTy(record_ptr, record_ty->ActualTy());
}

tr::ValAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  tr::ValAndTy *result = nullptr;
  for (auto *exp : seq_->GetList()) {
    result = exp->Translate(venv, tenv, level, errormsg);
  }
  return result;
}

tr::ValAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  tr::ValAndTy *var = var_->Translate(venv, tenv, level, errormsg);
  auto *exp = exp_->Translate(venv, tenv, level, errormsg);
  ir_builder->CreateStore(exp->val_, var->val_);
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *func = func_stack.top();
  auto *test_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "if_test", func);
  auto *then_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "if_then", func);
  auto *else_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "if_else", func);
  auto *next_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "if_next", func);

  ir_builder->CreateBr(test_bb);
  ir_builder->SetInsertPoint(test_bb);
  auto *test = test_->Translate(venv, tenv, level, errormsg);
  auto *test_val = ir_builder->CreateICmpNE(test->val_, llvm::ConstantInt::get(test->val_->getType(), 0));
  ir_builder->CreateCondBr(test_val, then_bb, else_bb);

  ir_builder->SetInsertPoint(then_bb);
  auto *then = then_->Translate(venv, tenv, level, errormsg);
  then_bb = ir_builder->GetInsertBlock();
  ir_builder->CreateBr(next_bb);

  ir_builder->SetInsertPoint(else_bb);
  if (elsee_ == nullptr) {
    ir_builder->CreateBr(next_bb);
    ir_builder->SetInsertPoint(next_bb);
    return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  }
  auto *elsee = elsee_->Translate(venv, tenv, level, errormsg);
  else_bb = ir_builder->GetInsertBlock();
  ir_builder->CreateBr(next_bb);

  ir_builder->SetInsertPoint(next_bb);
  if (then->val_ && elsee->val_) {
    auto *phi = ir_builder->CreatePHI(then->val_->getType(), 2);
    phi->addIncoming(then->val_, then_bb);
    phi->addIncoming(elsee->val_, else_bb);
    return new tr::ValAndTy(phi, elsee->ty_->ActualTy());
  }
  else if (then->ty_->IsSameType(elsee->ty_)) {
    if (then->val_ == nullptr && elsee->val_ == nullptr) {
      return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
    }
    auto *phi = ir_builder->CreatePHI(then->val_->getType(), 2);
    if (then->val_ == nullptr) {
      auto *ptr_ty = llvm::dyn_cast<llvm::PointerType>(elsee->ty_->GetLLVMType());
      auto *null = llvm::ConstantPointerNull::get(ptr_ty);
      phi->addIncoming(null, then_bb);
    }
    else {
      phi->addIncoming(then->val_, then_bb);
    }

    if (elsee->val_ == nullptr) {
      auto *ptr_ty = llvm::dyn_cast<llvm::PointerType>(then->ty_->GetLLVMType());
      auto *null = llvm::ConstantPointerNull::get(ptr_ty);
      phi->addIncoming(null, else_bb);
    }
    else {
      phi->addIncoming(elsee->val_, else_bb);
    }

    return new tr::ValAndTy(phi, elsee->ty_->ActualTy());
  }
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *func = func_stack.top();
  auto *test_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "while_test", func);
  auto *body_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "while_body", func);
  auto *done_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "while_done", func);

  ir_builder->CreateBr(test_bb);
  ir_builder->SetInsertPoint(test_bb);
  auto *test = test_->Translate(venv, tenv, level, errormsg);
  auto *test_val = ir_builder->CreateICmpNE(test->val_, ir_builder->getInt32(0));
  ir_builder->CreateCondBr(test_val, body_bb, done_bb);

  ir_builder->SetInsertPoint(body_bb);
  loop_stack.push(done_bb);
  venv->BeginScope();
  tenv->BeginScope();

  body_->Translate(venv, tenv, level, errormsg);

  venv->EndScope();
  tenv->EndScope();
  loop_stack.pop();
  ir_builder->CreateBr(test_bb);

  ir_builder->SetInsertPoint(done_bb);
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *func = func_stack.top();
  auto *test_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "for_test", func);
  auto *body_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "for_body", func);
  auto *done_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "for_done", func);
  auto *end_bb = llvm::BasicBlock::Create(ir_builder->getContext(), "for_end", func);

  venv->BeginScope();
  tenv->BeginScope();
  auto *tr_access = tr::Access::AllocLocal(level, escape_);
  auto *var_entry = new env::VarEntry(tr_access, type::IntTy::Instance(), true);
  venv->Enter(var_, var_entry);

  auto *var_addr = tr_access->access_->ToLLVMVal(level->get_sp());
  auto *var_ptr = ir_builder->CreateIntToPtr(var_addr, var_entry->ty_->GetLLVMType()->getPointerTo());
  auto *low = lo_->Translate(venv, tenv, level, errormsg);
  auto *high = hi_->Translate(venv, tenv, level, errormsg);
  ir_builder->CreateStore(low->val_, var_ptr);

  ir_builder->CreateBr(test_bb);
  ir_builder->SetInsertPoint(test_bb);
  auto *var_val = ir_builder->CreateLoad(ir_builder->getInt32Ty(), var_ptr);
  auto *test_val = ir_builder->CreateICmpSLE(var_val, high->val_);
  ir_builder->CreateCondBr(test_val, body_bb, done_bb);

  ir_builder->SetInsertPoint(body_bb);
  loop_stack.push(done_bb);
  auto *body = body_->Translate(venv, tenv, level, errormsg);
  loop_stack.pop();
  ir_builder->CreateBr(end_bb);

  ir_builder->SetInsertPoint(end_bb);
  var_val = ir_builder->CreateLoad(ir_builder->getInt32Ty(), var_ptr);
  auto *next_val = ir_builder->CreateAdd(var_val, ir_builder->getInt32(1));
  ir_builder->CreateStore(next_val, var_ptr);
  ir_builder->CreateBr(test_bb);

  ir_builder->SetInsertPoint(done_bb);
  venv->EndScope();
  tenv->EndScope();
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  ir_builder->CreateBr(loop_stack.top());
  loop_stack.pop();
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

tr::ValAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  venv->BeginScope();
  tenv->BeginScope();
  for (auto *dec : decs_->GetList()) {
    dec->Translate(venv, tenv, level, errormsg);
  }
  auto *body = body_->Translate(venv, tenv, level, errormsg);
  venv->EndScope();
  tenv->EndScope();
  return body;
}

tr::ValAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  auto *arr_ty = tenv->Look(typ_);
  auto *size = size_->Translate(venv, tenv, level, errormsg);
  auto *init = init_->Translate(venv, tenv, level, errormsg);
  auto arr_addr = ir_builder->CreateCall(init_array, {size->val_, ir_builder->CreateZExt(init->val_, ir_builder->getInt64Ty())});
  auto arr_ptr = ir_builder->CreateIntToPtr(arr_addr, arr_ty->GetLLVMType());
  return new tr::ValAndTy(arr_ptr, arr_ty->ActualTy());
}

tr::ValAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5-part1 code here */
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
}

} // namespace absyn