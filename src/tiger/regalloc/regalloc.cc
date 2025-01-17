#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"
#include <iostream>

extern frame::RegManager *reg_manager;
extern std::map<std::string, std::pair<int, int>> frame_info_map;

namespace ra {
/* TODO: Put your lab6 code here */
 RegAllocator::RegAllocator(const std::string &function_name,
                           std::unique_ptr<cg::AssemInstr> assem_instr) : function_name_(function_name), assem_instr_(std::move(assem_instr)), result_(std::make_unique<Result>()) {}

void RegAllocator::RegAlloc() {
   precolored = new live::INodeList();
   simplifyWorklist = new live::INodeList();
   freezeWorklist = new live::INodeList();
   spillWorklist = new live::INodeList();
   spilledNodes = new live::INodeList();
   coalescedNodes = new live::INodeList();
   coloredNodes = new live::INodeList();
   selectStack = new live::INodeList();
   coalescedMoves = new live::MoveList();
   constrainedMoves = new live::MoveList();
   frozenMoves = new live::MoveList();
   worklistMoves = new live::MoveList();
   activeMoves = new live::MoveList();
   degree = new std::unordered_map<live::INodePtr, int>();
   moveList = new std::unordered_map<live::INodePtr, live::MoveList *>();
   alias = new std::unordered_map<live::INodePtr, live::INodePtr>();
   color = new std::unordered_map<live::INodePtr, temp::Temp *>();

   auto *flow_graph_factory = new fg::FlowGraphFactory(assem_instr_->GetInstrList());
   flow_graph_factory->AssemFlowGraph();
   live_graph_factory_ = new live::LiveGraphFactory(flow_graph_factory->GetFlowGraph());
   live_graph_factory_->Liveness();
   worklistMoves = live_graph_factory_->GetLiveGraph().moves;

   Build();
   MakeWorklist();

   do {
     if (!simplifyWorklist->GetList().empty()) {
       Simplify();
     }
     else if (!worklistMoves->GetList().empty()) {
       Coalesce();
     }
     else if (!freezeWorklist->GetList().empty()) {
       Freeze();
     }
     else if (!spillWorklist->GetList().empty()) {
       SelectSpill();
     }
   } while (!(simplifyWorklist->GetList().empty() && worklistMoves->GetList().empty() && freezeWorklist->GetList().empty() && spillWorklist->GetList().empty()));

   AssignColors();

   if (!spilledNodes->GetList().empty()) {
     RewriteProgram();
     RegAlloc();
   }
   else {
     DeleteRepeatMoves();

     result_->coloring_ = temp::Map::Empty();
     for (auto node : live_graph_factory_->GetLiveGraph().interf_graph->Nodes()->GetList()) {
       result_->coloring_->Enter(node->NodeInfo(), reg_manager->temp_map_->Look(color->at(node)));
     }
     result_->il_ = assem_instr_->GetInstrList();
   }
 }

void RegAllocator::Build() {
   for (auto node : live_graph_factory_->GetLiveGraph().interf_graph->Nodes()->GetList()) {
     auto *move_list = new live::MoveList();
     for (auto &[src, dst] : worklistMoves->GetList()) {
       if (src == node || dst == node) {
         move_list->Append(src, dst);
       }
     }
     degree->insert({node, node->Degree()});
     moveList->insert({node, move_list});
   }

   for (auto *reg : reg_manager->Registers()->GetList()) {
     auto node = live_graph_factory_->GetTempNodeMap()->Look(reg);
     precolored->Append(node);
     color->insert({node, reg});
   }
 }

void RegAllocator::AddEdge(live::INodePtr u, live::INodePtr v) {
   if (!u->Succ()->Contain(v) && u != v) {
     live_graph_factory_->GetLiveGraph().interf_graph->AddEdge(u, v);
     live_graph_factory_->GetLiveGraph().interf_graph->AddEdge(v, u);
     degree->at(u)++;
     degree->at(v)++;
   }
 }


void RegAllocator::MakeWorklist() {
   for (auto node : live_graph_factory_->GetLiveGraph().interf_graph->Nodes()->GetList()) {
     if (degree->at(node) >= K) {
       spillWorklist->Append(node);
     }
     else if (MoveRelated(node)) {
       freezeWorklist->Append(node);
     }
     else {
       simplifyWorklist->Append(node);
     }
   }
 }


live::INodeListPtr RegAllocator::Adjacent(live::INodePtr n) {
   return n->Adj()->Diff(selectStack->Union(coalescedNodes));
 }

live::MoveList *RegAllocator::NodeMoves(live::INodePtr n) {
   return moveList->at(n)->Intersect(activeMoves->Union(worklistMoves));
 }

bool RegAllocator::MoveRelated(live::INodePtr n) {
   return !NodeMoves(n)->GetList().empty();
 }

void RegAllocator::Simplify() {
   auto node = simplifyWorklist->GetList().front();
   simplifyWorklist->DeleteNode(node);
   selectStack->Prepend(node);
   for (auto next : Adjacent(node)->GetList()) {
     DecrementDegree(next);
   }
 }

void RegAllocator::DecrementDegree(live::INodePtr m) {
   int d = degree->at(m);
   degree->at(m) = d - 1;
   if (d == K) {
     auto adjacent = Adjacent(m);
     adjacent->Append(m);
     EnableMoves(adjacent);
     spillWorklist->DeleteNode(m);
     if (MoveRelated(m)) {
       freezeWorklist->Append(m);
     }
     else {
       simplifyWorklist->Append(m);
     }
   }
 }


void RegAllocator::EnableMoves(live::INodeListPtr nodes) {
   for (auto cur : nodes->GetList()) {
     for (auto &[src, dst] : NodeMoves(cur)->GetList()) {
       if (activeMoves->Contain(src, dst)) {
         activeMoves->Delete(src, dst);
         worklistMoves->Append(src, dst);
       }
     }
   }
 }

void RegAllocator::Coalesce() {
   auto &[mx, my] = worklistMoves->GetList().front();
   auto x = GetAlias(mx);
   auto y = GetAlias(my);
   live::INodePtr u, v;
   if (precolored->Contain(y)) {
     u = y;
     v = x;
   }
   else {
     u = x;
     v = y;
   }

   worklistMoves->Delete(mx, my);
   if (u == v) {
     coalescedMoves->Append(mx, my);
     AddWorkList(u);
   }
   else if (precolored->Contain(v) || u->Succ()->Contain(v)) {
     constrainedMoves->Append(mx, my);
     AddWorkList(u);
     AddWorkList(v);
   }
   else {
     bool condition = false;
     if (precolored->Contain(u)) {
       condition = true;
       for (auto t : Adjacent(v)->GetList()) {
         if (!OK(t, u)) {
           condition = false;
           break;
         }
       }
     }

     if (condition || (!precolored->Contain(u) && Conservative(Adjacent(u)->Union(Adjacent(v))))) {
       coalescedMoves->Append(mx, my);
       Combine(u, v);
       AddWorkList(u);
     }
     else {
       activeMoves->Append(mx, my);
     }
   }
 }

void RegAllocator::AddWorkList(live::INodePtr u) {
   if (!precolored->Contain(u) && !MoveRelated(u) && degree->at(u) < K) {
     freezeWorklist->DeleteNode(u);
     simplifyWorklist->Append(u);
   }
 }

bool RegAllocator::OK(live::INodePtr t, live::INodePtr r) {
   return degree->at(t) < K || precolored->Contain(t) || t->Succ()->Contain(r);
 }


bool RegAllocator::Conservative(live::INodeListPtr nodes) {
   int k = 0;
   for (auto node : nodes->GetList()) {
     if (degree->at(node) >= K) {
        k++;
     }
   }
   return k < K;
 }

live::INodePtr RegAllocator::GetAlias(live::INodePtr n) {
   if (coalescedNodes->Contain(n)) {
     return GetAlias(alias->at(n));
   }
   else {
     return n;
   }
 }

void RegAllocator::Combine(live::INodePtr u, live::INodePtr v) {
   if (freezeWorklist->Contain(v)) {
     freezeWorklist->DeleteNode(v);
   }
   else {
     spillWorklist->DeleteNode(v);
   }

   coalescedNodes->Append(v);
   alias->insert({v, u});
   moveList->at(u) = moveList->at(u)->Union(moveList->at(v));
   auto v_list = new live::INodeList();
   v_list->Append(v);
   EnableMoves(v_list);
   for (auto t : Adjacent(v)->GetList()) {
     AddEdge(t, u);
     DecrementDegree(t);
   }
   if (degree->at(u) >= K && freezeWorklist->Contain(u)) {
     freezeWorklist->DeleteNode(u);
     spillWorklist->Append(u);
   }
 }

void RegAllocator::Freeze() {
   auto u = freezeWorklist->GetList().front();
   freezeWorklist->DeleteNode(u);
   simplifyWorklist->Append(u);
   FreezeMoves(u);
 }

void RegAllocator::FreezeMoves(live::INodePtr u) {
   for (auto &[x, y] : NodeMoves(u)->GetList()) {
     live::INodePtr v;
     if (GetAlias(y) == GetAlias(u)) {
       v = GetAlias(x);
     }
     else {
       v = GetAlias(y);
     }
     activeMoves->Delete(x, y);
     frozenMoves->Append(x, y);
     if (NodeMoves(v)->GetList().empty() && degree->at(v) < K) {
       freezeWorklist->DeleteNode(v);
       simplifyWorklist->Append(v);
     }
   }
 }

void RegAllocator::SelectSpill() {
   auto m = spillWorklist->GetList().front();
   spillWorklist->DeleteNode(m);
   simplifyWorklist->Append(m);
   FreezeMoves(m);
 }

void RegAllocator::AssignColors() {
   while (!selectStack->GetList().empty()) {
     auto n = selectStack->GetList().front();
     selectStack->DeleteNode(n);
     auto *okColors = reg_manager->Registers();
     for (auto w : n->Adj()->GetList()) {
       if (coloredNodes->Union(precolored)->Contain(GetAlias(w))) {
         okColors->Delete(color->at(GetAlias(w)));
       }
     }
     if (okColors->GetList().empty()) {
       spilledNodes->Append(n);
     }
     else {
       coloredNodes->Append(n);
       auto *c = okColors->GetList().front();
       color->operator[](n) = c;
     }
   }
   for (auto n : coalescedNodes->GetList()) {
     color->insert({n, color->at(GetAlias(n))});
   }
 }

void RegAllocator::RewriteProgram() {
   auto *rsp = reg_manager->GetRegister(frame::X64RegManager::Reg::RSP);
   auto &[x, y] = frame_info_map[function_name_];
   for (auto node : spilledNodes->GetList()) {
     auto *temp = temp::TempFactory::NewTemp();
     for (auto iter = assem_instr_->GetInstrList()->GetList().begin(); iter != assem_instr_->GetInstrList()->GetList().end(); iter++) {
       auto *src = (*iter)->Use();
       auto *dst = (*iter)->Def();

       if (src != nullptr && src->Contain(node->NodeInfo())) {
         src->Replace(node->NodeInfo(), temp);
         if (auto *move_instr = dynamic_cast<assem::MoveInstr *>(*iter)) {
           move_instr->src_ = src;
         }
         else if (auto *oper_instr = dynamic_cast<assem::OperInstr *>(*iter)) {
           oper_instr->src_ = src;
         }
         assem_instr_->GetInstrList()->Insert(iter, new assem::MoveInstr("movq " + std::string(function_name_) + "_framesize_local" + std::to_string(x) + "(`s0),`d0", new temp::TempList(temp), new temp::TempList(rsp)));
       }

       if (dst != nullptr && dst->Contain(node->NodeInfo())) {
         dst->Replace(node->NodeInfo(), temp);
         if (auto *move_instr = dynamic_cast<assem::MoveInstr *>(*iter)) {
           move_instr->dst_ = dst;
         }
         else if (auto *oper_instr = dynamic_cast<assem::OperInstr *>(*iter)) {
           oper_instr->dst_ = dst;
         }
         x -= 8;
         y += 8;
         assem_instr_->GetInstrList()->Insert(std::next(iter), new assem::MoveInstr("movq `s0," + std::string(function_name_) + "_framesize_local" + std::to_string(x) + "(`d0)", new temp::TempList(rsp), new temp::TempList(temp)));
       }
     }
   }
 }

void RegAllocator::DeleteRepeatMoves() {
   std::list<assem::Instr *> instrs;
   auto *rsp = reg_manager->GetRegister(frame::X64RegManager::Reg::RSP);

   for (auto iter = assem_instr_->GetInstrList()->GetList().begin(); iter != assem_instr_->GetInstrList()->GetList().end(); iter++) {
     if (auto *move_instr = dynamic_cast<assem::MoveInstr *>(*iter)) {
       auto *src = move_instr->src_;
       auto *dst = move_instr->dst_;
       if (src->GetList().size() == 1 && dst->GetList().size() == 1) {
         auto *src_temp = src->GetList().front();
         auto *dst_temp = dst->GetList().front();
         if (src_temp == rsp || dst_temp == rsp) {
           continue;
         }
         auto src_node = live_graph_factory_->GetTempNodeMap()->Look(src_temp);
         auto dst_node = live_graph_factory_->GetTempNodeMap()->Look(dst_temp);
         if (color->at(src_node) == color->at(dst_node)) {
           instrs.push_back(move_instr);
         }
       }
     }
   }
   for (auto *instr : instrs) {
     assem_instr_->GetInstrList()->Remove(instr);
   }
 }


} // namespace ra