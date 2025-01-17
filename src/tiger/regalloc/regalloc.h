#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() {}
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
private:
  const int K = 15;
  int offset = 0;
  std::string function_name_;
  std::unique_ptr<cg::AssemInstr> assem_instr_;
  std::unique_ptr<Result> result_;
  live::LiveGraphFactory *live_graph_factory_;
  live::INodeListPtr precolored;
  live::INodeListPtr simplifyWorklist;
  live::INodeListPtr freezeWorklist;
  live::INodeListPtr spillWorklist;
  live::INodeListPtr spilledNodes;
  live::INodeListPtr coalescedNodes;
  live::INodeListPtr coloredNodes;
  live::INodeListPtr selectStack;
  live::MoveList *coalescedMoves;
  live::MoveList *constrainedMoves;
  live::MoveList *frozenMoves;
  live::MoveList *worklistMoves;
  live::MoveList *activeMoves;
  std::unordered_map<live::INodePtr, int> *degree;
  std::unordered_map<live::INodePtr, live::MoveList *> *moveList;
  std::unordered_map<live::INodePtr, live::INodePtr> *alias;
  std::unordered_map<live::INodePtr, temp::Temp *> *color;

  void Build();
  void AddEdge(live::INodePtr u, live::INodePtr v);
  void MakeWorklist();
  live::INodeListPtr Adjacent(live::INodePtr n);
  live::MoveList *NodeMoves(live::INodePtr n);
  bool MoveRelated(live::INodePtr n);
  void Simplify();
  void DecrementDegree(live::INodePtr m);
  void EnableMoves(live::INodeListPtr nodes);
  void Coalesce();
  void AddWorkList(live::INodePtr u);
  bool OK(live::INodePtr t, live::INodePtr r);
  bool Conservative(live::INodeListPtr nodes);
  live::INodePtr GetAlias(live::INodePtr n);
  void Combine(live::INodePtr u, live::INodePtr v);
  void Freeze();
  void FreezeMoves(live::INodePtr u);
  void SelectSpill();
  void AssignColors();
  void RewriteProgram();
  void DeleteRepeatMoves();

public:
  RegAllocator(const std::string &function_name, std::unique_ptr<cg::AssemInstr> assem_instr);
  void RegAlloc();
  std::unique_ptr<Result> TransferResult() {
    return std::move(result_);
  }

};

} // namespace ra

#endif