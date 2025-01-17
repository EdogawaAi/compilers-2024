#include "tiger/liveness/liveness.h"

#include <unordered_set>

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  for (auto node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto iter = flowgraph_->Nodes()->GetList().rbegin(); iter != flowgraph_->Nodes()->GetList().rend(); iter++) {
      auto node = *iter;
      auto *old_in = in_->Look(node);
      auto *old_out = out_->Look(node);
      auto *in = new temp::TempList();
      auto *out = new temp::TempList();

      for (auto s : node->Succ()->GetList()) {
        auto *in_s = in_->Look(s);
        out = out->Union(in_s);
      }

      if (!out->Equal(old_out)) {
        changed = true;
        out_->Set(node, out);
      }

      auto *use = node->NodeInfo()->Use();
      auto *def = node->NodeInfo()->Def();
      in = use->Union(out->Diff(def));

      if (!in->Equal(old_in)) {
        changed = true;
        in_->Set(node, in);
      }
    }
  }
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  auto *rsp = reg_manager->GetRegister(frame::X64RegManager::Reg::RSP);
  auto *regs = reg_manager->Registers();

  std::unordered_set<temp::Temp *> reg_set;

  for (auto *reg : regs->GetList()) {
    reg_set.insert(reg);
    if (!temp_node_map_->Look(reg)) {
      auto reg_node = live_graph_.interf_graph->NewNode(reg);
      temp_node_map_->Enter(reg, reg_node);
    }

    for (auto reg_it1 = reg_set.begin(); reg_it1 != reg_set.end(); reg_it1++) {
      auto *node1 = temp_node_map_->Look(*reg_it1);
      for (auto reg_it2 = std::next(reg_it1); reg_it2 != reg_set.end(); reg_it2++) {
        auto *node2 = temp_node_map_->Look(*reg_it2);
        live_graph_.interf_graph->AddEdge(node1, node2);
        live_graph_.interf_graph->AddEdge(node2, node1);
      }
    }

    for (auto node_it = flowgraph_->Nodes()->GetList().begin(); node_it != flowgraph_->Nodes()->GetList().end(); node_it++) {
      auto *out = out_->Look(*node_it);
      for (auto temp_it = out->GetList().begin(); temp_it != out->GetList().end(); temp_it++) {
        if (!temp_node_map_->Look(*temp_it)) {
          auto temp_node = live_graph_.interf_graph->NewNode(*temp_it);
          temp_node_map_->Enter(*temp_it, temp_node);
        }
      }
    }

    for (auto node : flowgraph_->Nodes()->GetList()) {
      auto *live = out_->Look(node);
      auto *instr = node->NodeInfo();

      if (auto *move_instr = dynamic_cast<assem::MoveInstr *>(instr)) {
        live = live->Diff(instr->Use());

        for (auto *def : instr->Def()->GetList()) {
          if (!temp_node_map_->Look(def)) {
            auto def_node = live_graph_.interf_graph->NewNode(def);
            temp_node_map_->Enter(def, def_node);
          }
          if (def == rsp) {
            continue;
          }

          for (auto *out : live->GetList()) {
            if (out != rsp && def != out) {
              auto def_node = temp_node_map_->Look(def);
              auto out_node = temp_node_map_->Look(out);
              live_graph_.interf_graph->AddEdge(def_node, out_node);
              live_graph_.interf_graph->AddEdge(out_node, def_node);
            }
          }

          for (auto use : instr->Use()->GetList()) {
            auto use_node = temp_node_map_->Look(use);
            auto def_node = temp_node_map_->Look(def);
            if (use != rsp && !live_graph_.moves->Contain(def_node, use_node)) {
              live_graph_.moves->Append(def_node, use_node);
            }
          }
        }
      }
      else {
        for (auto *def : instr->Def()->GetList()) {
          if (!temp_node_map_->Look(def)) {
            auto def_node = live_graph_.interf_graph->NewNode(def);
            temp_node_map_->Enter(def, def_node);
          }

          if (def == rsp) {
            continue;
          }

          for (auto *out : live->GetList()) {
            if (out != rsp && def != out) {
              auto def_node = temp_node_map_->Look(def);
              auto out_node = temp_node_map_->Look(out);
              live_graph_.interf_graph->AddEdge(def_node, out_node);
              live_graph_.interf_graph->AddEdge(out_node, def_node);
            }
          }
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live