#include "tiger/liveness/flowgraph.h"
#include <iostream>
namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  FNodePtr prev = nullptr;
  std::list<FNodePtr> jump_nodes;
  for (auto *instr : instr_list_->GetList()) {
    auto node = flowgraph_->NewNode(instr);
    if (prev) {
      flowgraph_->AddEdge(prev, node);
    }
    prev = node;

    if (auto *label_instr = dynamic_cast<assem::LabelInstr *>(instr)) {
      label_map_->insert({label_instr->label_->Name(), node});
    }
    else if (auto *oper_instr = dynamic_cast<assem::OperInstr *>(instr)) {
      if (oper_instr->jumps_) {
        jump_nodes.push_back(node);
        if (oper_instr->assem_.find("jmp") != std::string::npos) {
          prev = nullptr;
        }
      }
    }
  }
  for (auto node : jump_nodes) {
    auto *oper_instr = dynamic_cast<assem::OperInstr *>(node->NodeInfo());
    for (auto *label : *(oper_instr->jumps_->labels_)) {
      FNodePtr jump = label_map_->at(label->Name());
      flowgraph_->AddEdge(node, jump);
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Def() const { return dst_; }

temp::TempList *OperInstr::Def() const { return dst_; }

temp::TempList *LabelInstr::Use() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Use() const {
  return (src_) ? src_ : new temp::TempList();
}

temp::TempList *OperInstr::Use() const {
  return (src_) ? src_ : new temp::TempList();
}
} // namespace assem
