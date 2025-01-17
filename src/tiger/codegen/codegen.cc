#include "tiger/codegen/codegen.h"

#include <cassert>
#include <iostream>
#include <sstream>

extern frame::RegManager *reg_manager;
extern frame::Frags *frags;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  temp_map_ = new std::unordered_map<llvm::Value *, temp::Temp *>();
  bb_map_ = new std::unordered_map<llvm::BasicBlock *, int>();
  auto *list = new assem::InstrList();

  // firstly get all global string's location
  for (auto &&frag : frags->GetList()) {
    if (auto *str_frag = dynamic_cast<frame::StringFrag *>(frag)) {
      auto tmp = temp::TempFactory::NewTemp();
      list->Append(new assem::OperInstr(
          "leaq " + std::string(str_frag->str_val_->getName()) + "(%rip),`d0",
          new temp::TempList(tmp), new temp::TempList(), nullptr));
      temp_map_->insert({str_frag->str_val_, tmp});
    }
  }

  // move arguments to temp
  auto arg_iter = traces_->GetBody()->arg_begin();
  auto regs = reg_manager->ArgRegs();
  auto tmp_iter = regs->GetList().begin();

  // first arguement is rsp, we need to skip it
  ++arg_iter;

  for (; arg_iter != traces_->GetBody()->arg_end() &&
         tmp_iter != regs->GetList().end();
       ++arg_iter, ++tmp_iter) {
    auto tmp = temp::TempFactory::NewTemp();
    list->Append(new assem::OperInstr("movq `s0,`d0", new temp::TempList(tmp),
                                      new temp::TempList(*tmp_iter), nullptr));
    temp_map_->insert({static_cast<llvm::Value *>(arg_iter), tmp});
  }

  // pass-by-stack parameters
  if (arg_iter != traces_->GetBody()->arg_end()) {
    auto last_sp = temp::TempFactory::NewTemp();
    list->Append(
        new assem::OperInstr("movq %rsp,`d0", new temp::TempList(last_sp),
                             new temp::TempList(reg_manager->GetRegister(
                                 frame::X64RegManager::Reg::RSP)),
                             nullptr));
    list->Append(new assem::OperInstr(
        "addq $" + std::string(traces_->GetFunctionName()) +
            "_framesize_local,`s0",
        new temp::TempList(last_sp),
        new temp::TempList({last_sp, reg_manager->GetRegister(
                                         frame::X64RegManager::Reg::RSP)}),
        nullptr));
    while (arg_iter != traces_->GetBody()->arg_end()) {
      auto tmp = temp::TempFactory::NewTemp();
      list->Append(new assem::OperInstr(
          "movq " +
              std::to_string(8 * (arg_iter - traces_->GetBody()->arg_begin())) +
              "(`s0),`d0",
          new temp::TempList(tmp), new temp::TempList(last_sp), nullptr));
      temp_map_->insert({static_cast<llvm::Value *>(arg_iter), tmp});
      ++arg_iter;
    }
  }

  // construct bb_map
  int bb_index = 0;
  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    bb_map_->insert({bb, bb_index++});
  }

  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    // record every return value from llvm instruction
    for (auto &&inst : bb->getInstList())
      temp_map_->insert({&inst, temp::TempFactory::NewTemp()});
  }

  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    // Generate label for basic block
    list->Append(new assem::LabelInstr(std::string(bb->getName())));

    // Generate instructions for basic block
    for (auto &&inst : bb->getInstList())
      InstrSel(list, inst, traces_->GetFunctionName(), bb);
  }

  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(
      frame::ProcEntryExit1(traces_->GetFunctionName(), list)));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}

void CodeGen::InstrSel(assem::InstrList *instr_list, llvm::Instruction &inst,
                       std::string_view function_name, llvm::BasicBlock *bb) {
  // TODO: your lab5 code here
  auto opcode = inst.getOpcode();
  if (opcode == llvm::Instruction::Load) {
    LoadInstrSel(instr_list, llvm::cast<llvm::LoadInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::Add || opcode == llvm::Instruction::Sub || opcode == llvm::Instruction::Mul) {
    BinaryOperatorInstrSel(instr_list, llvm::cast<llvm::BinaryOperator>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::SDiv) {
    SDivInstrSel(instr_list, llvm::cast<llvm::SDivOperator>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::IntToPtr) {
    IntToPtrInstrSel(instr_list, llvm::cast<llvm::IntToPtrInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::PtrToInt) {
    PtrToIntInstrSel(instr_list, llvm::cast<llvm::PtrToIntInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::GetElementPtr) {
    GetElementPtrInstrSel(instr_list, llvm::cast<llvm::GetElementPtrInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::Store) {
    StoreInstrSel(instr_list, llvm::cast<llvm::StoreInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::ZExt) {
    ZExtInstrSel(instr_list, llvm::cast<llvm::ZExtInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::Call) {
    CallInstrSel(instr_list, llvm::cast<llvm::CallInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::Ret) {
    RetInstrSel(instr_list, llvm::cast<llvm::ReturnInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::Br) {
    BrInstrSel(instr_list, llvm::cast<llvm::BranchInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::ICmp) {
    ICmpInstrSel(instr_list, llvm::cast<llvm::ICmpInst>(inst), function_name, bb);
  }
  else if (opcode == llvm::Instruction::PHI) {
    PhiInstrSel(instr_list, llvm::cast<llvm::PHINode>(inst), function_name, bb);
  }
  else {
    throw std::runtime_error(std::string("Unknown instruction: ") +
                           inst.getOpcodeName());
  }
}

void CodeGen::LoadInstrSel(assem::InstrList *instr_list, llvm::LoadInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  if (auto *gv = llvm::dyn_cast<llvm::GlobalVariable>(inst.getPointerOperand())) {
    instr_list->Append(new assem::MoveInstr("movq " + gv->getName().str() + "(%rip),`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList()));
  }
  else {
    instr_list->Append(new assem::OperInstr("movq (`s0),`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList(temp_map_->at(inst.getPointerOperand())), nullptr));
  }
}

void CodeGen::BinaryOperatorInstrSel(assem::InstrList *instr_list, llvm::BinaryOperator &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  std::string op_str;
  std::string src_str1 = "`s0";
  std::string src_str2 = "`s0";
  std::string dst_str = "`d0";
  auto *src_temp1 = new temp::TempList();
  auto *src_temp2 = new temp::TempList();
  auto *dst_temp = new temp::TempList();

  if (inst.getOpcode() == llvm::Instruction::Add) {
    op_str = "addq ";
  }
  else if (inst.getOpcode() == llvm::Instruction::Sub) {
    op_str = "subq ";
  }
  else if (inst.getOpcode() == llvm::Instruction::Mul) {
    op_str = "imulq ";
  }

  if (IsRsp(&inst, function_name)) {
    return;
  }
  else {
    dst_temp->Append(temp_map_->at(&inst));
  }

  if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
    src_str1 = "$" + std::to_string(ci->getSExtValue());
  }
  else if (IsRsp(inst.getOperand(0), function_name)) {
    src_temp1->Append(reg_manager->GetRegister(frame::X64RegManager::Reg::RSP));
  }
  else {
    src_temp1->Append(temp_map_->at(inst.getOperand(0)));
  }

  if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
    src_str2 = "$" + std::to_string(ci->getSExtValue());
  }
  else if (IsRsp(inst.getOperand(1), function_name)) {
    src_temp2->Append(reg_manager->GetRegister(frame::X64RegManager::Reg::RSP));
  }
  else {
    src_temp2->Append(temp_map_->at(inst.getOperand(1)));
  }

  instr_list->Append(new assem::MoveInstr("movq " + src_str1 + "," + dst_str, dst_temp, src_temp1));
  instr_list->Append(new assem::OperInstr(op_str + src_str2 + "," + dst_str, dst_temp, src_temp2, nullptr));
}

void CodeGen::SDivInstrSel(assem::InstrList *instr_list, llvm::SDivOperator &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
    instr_list->Append(new assem::MoveInstr("movq $" + std::to_string(ci->getSExtValue()) + ",`d0", new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)), new temp::TempList()));
  }
  else {
    instr_list->Append(new assem::MoveInstr("movq `s0,`d0", new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)), new temp::TempList(temp_map_->at(inst.getOperand(0)))));
  }
  instr_list->Append(new assem::OperInstr("cqto", new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)), new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)), nullptr));
  if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
    auto tmp = temp::TempFactory::NewTemp();
    instr_list->Append(new assem::MoveInstr("movq $" + std::to_string(ci->getSExtValue()) + ",`d0", new temp::TempList(tmp), new temp::TempList()));
    instr_list->Append(new assem::OperInstr("idivq `s0", new temp::TempList({reg_manager->GetRegister(frame::X64RegManager::Reg::RAX), reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}), new temp::TempList({tmp, reg_manager->GetRegister(frame::X64RegManager::Reg::RAX), reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}), nullptr));
  }
  else {
    instr_list->Append(new assem::OperInstr("idivq `s0", new temp::TempList({reg_manager->GetRegister(frame::X64RegManager::Reg::RAX), reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}), new temp::TempList({temp_map_->at(inst.getOperand(1)), reg_manager->GetRegister(frame::X64RegManager::Reg::RAX), reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}), nullptr));
  }
  instr_list->Append(new assem::MoveInstr("movq `s0,`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX))));
}

void CodeGen::IntToPtrInstrSel(assem::InstrList *instr_list, llvm::IntToPtrInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  instr_list->Append(new assem::MoveInstr("movq `s0,`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList(temp_map_->at(inst.getOperand(0)))));
}

void CodeGen::PtrToIntInstrSel(assem::InstrList *instr_list, llvm::PtrToIntInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  instr_list->Append(new assem::MoveInstr("movq `s0,`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList(temp_map_->at(inst.getPointerOperand()))));
}

void CodeGen::GetElementPtrInstrSel(assem::InstrList *instr_list, llvm::GetElementPtrInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  instr_list->Append(new assem::MoveInstr("movq `s0,`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList(temp_map_->at(inst.getPointerOperand()))));
  for (int i = 1; i < inst.getNumOperands(); i++) {
    auto *temp = temp::TempFactory::NewTemp();

    if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(i))) {
      instr_list->Append(new assem::MoveInstr("movq $" + std::to_string(ci->getSExtValue()) + ",`d0", new temp::TempList(temp), new temp::TempList()));
    }
    else {
      instr_list->Append(new assem::MoveInstr("movq `s0,`d0", new temp::TempList(temp), new temp::TempList(temp_map_->at(inst.getOperand(i)))));
    }

    instr_list->Append(new assem::OperInstr("imulq $8,`d0", new temp::TempList(temp), new temp::TempList(temp), nullptr));
    instr_list->Append(new assem::OperInstr("addq `s0,`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList({temp, temp_map_->at(&inst)}), nullptr));
  }
}

void CodeGen::StoreInstrSel(assem::InstrList *instr_list, llvm::StoreInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getValueOperand())) {
    instr_list->Append(new assem::MoveInstr("movq $" + std::to_string(ci->getSExtValue()) + ",(`s0)", new temp::TempList(), new temp::TempList(temp_map_->at(inst.getPointerOperand()))));
  }
  else {
    instr_list->Append(new assem::OperInstr("movq `s0,(`s1)", new temp::TempList(), new temp::TempList({temp_map_->at(inst.getValueOperand()), temp_map_->at(inst.getPointerOperand())}), nullptr));
  }
}

void CodeGen::ZExtInstrSel(assem::InstrList *instr_list, llvm::ZExtInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  instr_list->Append(new assem::MoveInstr("movq `s0,`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList(temp_map_->at(inst.getOperand(0)))));
}

void CodeGen::CallInstrSel(assem::InstrList *instr_list, llvm::CallInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  std::string called_func_name = inst.getCalledFunction()->getName().str();
  auto arg_regs = reg_manager->ArgRegs()->GetList();

  int i = IsRsp(inst.getOperand(0), function_name) ? 1 : 0;

  for (auto tmp_iter = arg_regs.begin(); tmp_iter != arg_regs.end() && i < inst.getNumOperands() - 1; tmp_iter++, i++) {
    std::string src_str = "`s0";
    auto *src_temp = new temp::TempList();
    auto *dst_temp = new temp::TempList(*tmp_iter);

    if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(i))) {
      src_str = "$" + std::to_string(ci->getSExtValue());
    }
    else if (IsRsp(inst.getOperand(i), function_name)) {
      src_temp->Append(reg_manager->GetRegister(frame::X64RegManager::Reg::RSP));
    }
    else {
      src_temp->Append(temp_map_->at(inst.getOperand(i)));
    }

    instr_list->Append(new assem::MoveInstr("movq " + src_str + ",`d0", dst_temp, src_temp));
  }

  if (i < inst.getNumOperands() - 1) {
    auto *sp = temp::TempFactory::NewTemp();
    instr_list->Append(new assem::MoveInstr("movq `s0,`d0", new temp::TempList(sp), new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RSP))));
    instr_list->Append(new assem::OperInstr("addq $" + called_func_name + "_framesize_local, `s0", new temp::TempList(), new temp::TempList(sp), nullptr));
    while (i < inst.getNumOperands() - 1) {
      std::string src_str = "`s0";
      std::string dst_str = std::to_string(8 * i) + "(`d0)";
      auto *src_temp = new temp::TempList();
      auto *dst_temp = new temp::TempList(sp);

      if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(i))) {
        src_str = "$" + std::to_string(ci->getSExtValue());
      }
      else if (IsRsp(inst.getOperand(i), function_name)) {
        src_temp->Append(reg_manager->GetRegister(frame::X64RegManager::Reg::RSP));
      }
      else {
        src_temp->Append(temp_map_->at(inst.getOperand(i)));
      }

      instr_list->Append(new assem::MoveInstr("movq " + src_str + "," + dst_str, dst_temp, src_temp));

      i++;
    }
  }
  instr_list->Append(new assem::OperInstr("callq " + called_func_name, reg_manager->CallerSaves(), new temp::TempList(), nullptr));
  instr_list->Append(new assem::MoveInstr("movq `s0,`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX))));
}

void CodeGen::RetInstrSel(assem::InstrList *instr_list, llvm::ReturnInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  if (inst.getNumOperands() > 0) {
    std::string src_str = "`s0";
    auto *src_temp = new temp::TempList();
    auto *dst_temp = new temp::TempList(reg_manager->GetRegister(frame::X64RegManager::Reg::RAX));

    if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
      src_str = "$" + std::to_string(ci->getSExtValue());
    }
    else if (IsRsp(inst.getOperand(0), function_name)) {
      src_temp->Append(reg_manager->GetRegister(frame::X64RegManager::Reg::RSP));
    }
    else {
      src_temp->Append(temp_map_->at(inst.getOperand(0)));
    }

    instr_list->Append(new assem::MoveInstr("movq " + src_str + ",`d0", dst_temp, src_temp));
  }

  instr_list->Append(new assem::MoveInstr("movq $" + std::to_string(bb_map_->at(inst.getParent())) + ",`d0", new temp::TempList(phi_temp_), new temp::TempList()));
  instr_list->Append(new assem::OperInstr("jmp " + std::string(function_name) + "_end", new temp::TempList(), new temp::TempList(), new assem::Targets(new std::vector<temp::Label *>({temp::LabelFactory::NamedLabel(std::string(function_name) + "_end")}))));
}

void CodeGen::BrInstrSel(assem::InstrList *instr_list, llvm::BranchInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  if (inst.isConditional()) {
    if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
      instr_list->Append(new assem::OperInstr("cmpq $" + std::to_string(ci->getSExtValue()) + ",$1", new temp::TempList(), new temp::TempList(), nullptr));
    }
    else {
      instr_list->Append(new assem::OperInstr("cmpq $1,`s0", new temp::TempList(), new temp::TempList(temp_map_->at(inst.getOperand(0))), nullptr));
    }

    instr_list->Append(new assem::MoveInstr("movq $" + std::to_string(bb_map_->at(inst.getParent())) + ",`d0", new temp::TempList(phi_temp_), new temp::TempList()));

    instr_list->Append(new assem::OperInstr("jne " + inst.getOperand(1)->getName().str(), new temp::TempList(), new temp::TempList(), new assem::Targets(new std::vector<temp::Label *>({temp::LabelFactory::NamedLabel(inst.getOperand(1)->getName().str())}))));

    instr_list->Append(new assem::MoveInstr("movq $" + std::to_string(bb_map_->at(inst.getParent())) + ",`d0", new temp::TempList(phi_temp_), new temp::TempList()));
    instr_list->Append(new assem::OperInstr("jmp " + inst.getOperand(2)->getName().str(), new temp::TempList(), new temp::TempList(), new assem::Targets(new std::vector<temp::Label *>({temp::LabelFactory::NamedLabel(inst.getOperand(2)->getName().str())}))));
  }
  else {
    instr_list->Append(new assem::MoveInstr("movq $" + std::to_string(bb_map_->at(inst.getParent())) + ",`d0", new temp::TempList(phi_temp_), new temp::TempList()));
    instr_list->Append(new assem::OperInstr("jmp " + inst.getOperand(0)->getName().str(), new temp::TempList(), new temp::TempList(), new assem::Targets(new std::vector<temp::Label *>({temp::LabelFactory::NamedLabel(inst.getOperand(0)->getName().str())}))));
  }
}

void CodeGen::ICmpInstrSel(assem::InstrList *instr_list, llvm::ICmpInst &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  std::string op_str;
  if (auto *ci1 = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0))) {
    if (auto *ci2 = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
      instr_list->Append(new assem::OperInstr("cmpq $" + std::to_string(ci2->getSExtValue()) + ",$" + std::to_string(ci1->getSExtValue()), new temp::TempList(), new temp::TempList(), nullptr));
    }
    else {
      instr_list->Append(new assem::OperInstr("cmpq `s0,$" + std::to_string(ci1->getSExtValue()), new temp::TempList(), new temp::TempList(temp_map_->at(inst.getOperand(1))), nullptr));
    }
  }
  else if (auto *ci2 = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))) {
    instr_list->Append(new assem::OperInstr("cmpq $" + std::to_string(ci2->getSExtValue()) + ",`s0", new temp::TempList(), new temp::TempList(temp_map_->at(inst.getOperand(0))), nullptr));
  }
  else {
    instr_list->Append(new assem::OperInstr( "cmpq `s0,`s1", new temp::TempList(), new temp::TempList({temp_map_->at(inst.getOperand(1)), temp_map_->at(inst.getOperand(0))}),  nullptr));
  }


  instr_list->Append(new assem::OperInstr("movq $0,`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList(), nullptr));

  auto predicate = inst.getPredicate();

  if (predicate == llvm::CmpInst::Predicate::ICMP_EQ) {
    op_str = "sete ";
  }
  else if (predicate == llvm::CmpInst::Predicate::ICMP_NE) {
    op_str = "setne ";
  }
  else if (predicate == llvm::CmpInst::Predicate::ICMP_SLT) {
    op_str = "setl ";
  }
  else if (predicate == llvm::CmpInst::Predicate::ICMP_SLE) {
    op_str = "setle ";
  }
  else if (predicate == llvm::CmpInst::Predicate::ICMP_SGT) {
    op_str = "setg ";
  }
  else if (predicate == llvm::CmpInst::Predicate::ICMP_SGE) {
    op_str = "setge ";
  }

  instr_list->Append(new assem::OperInstr(op_str + "`d0", new temp::TempList(temp_map_->at(&inst)), new temp::TempList(), nullptr));
}

void CodeGen::PhiInstrSel(assem::InstrList *instr_list, llvm::PHINode &inst, std::string_view function_name, llvm::BasicBlock *bb) {
  std::vector<std::string> phi_label;
  for (int i = 0; i <= inst.getNumOperands(); i++) {
    phi_label.push_back(inst.getParent()->getName().str() + "_" + std::to_string(rand()));
  }

  std::string end_label = std::string(inst.getParent()->getName().str()) + "_end";

  for (int i = 0; i < inst.getNumOperands(); i++) {
    instr_list->Append(new assem::OperInstr("cmpq $" + std::to_string(bb_map_->at(inst.getIncomingBlock(i))) + ",`s0", new temp::TempList(), new temp::TempList(phi_temp_), nullptr));
    instr_list->Append(new assem::OperInstr("je " + phi_label[i], new temp::TempList(), new temp::TempList(), new assem::Targets(new std::vector<temp::Label *>({temp::LabelFactory::NamedLabel(phi_label[i])}))));
  }

  for (int i = 0; i < inst.getNumOperands(); i++) {
    std::string src_str = "`s0";
    auto *src_temp = new temp::TempList();

    if (auto *ci = llvm::dyn_cast<llvm::ConstantInt>(inst.getIncomingValue(i))) {
      src_str = "$" + std::to_string(ci->getZExtValue());
    }
    else if (auto *cpn = llvm::dyn_cast<llvm::ConstantPointerNull>(inst.getIncomingValue(i))) {
      src_str = "$0";
    }
    else if (IsRsp(inst.getIncomingValue(i), function_name)) {
      src_temp->Append(reg_manager->GetRegister(frame::X64RegManager::Reg::RSP));
    }
    else {
      src_temp->Append(temp_map_->at(inst.getIncomingValue(i)));
    }

    instr_list->Append(new assem::LabelInstr(phi_label[i]));
    instr_list->Append(new assem::MoveInstr("movq " + src_str + ",`d0", new temp::TempList(temp_map_->at(&inst)), src_temp));
    instr_list->Append(new assem::OperInstr("jmp " + end_label, new temp::TempList(), new temp::TempList(), new assem::Targets(new std::vector<temp::Label *>({temp::LabelFactory::NamedLabel(end_label)}))));
  }
  instr_list->Append(new assem::LabelInstr(end_label));
}

} // namespace cg