#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <map>

std::map<std::string, llvm::Function *> functions;

llvm::Function *addFunction(std::shared_ptr<llvm::Module> ir_module, std::string name, llvm::FunctionType *type)
{
    llvm::Function *function = llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, ir_module.get());
    for (auto &arg : function->args())
        arg.addAttr(llvm::Attribute::NoUndef);
    functions.insert(std::make_pair(name, function));
    return function;
}

void buildGlobal(std::shared_ptr<llvm::Module> ir_module)
{
}

void buildMain(std::shared_ptr<llvm::Module> ir_module)
{
    llvm::IRBuilder<> builder(ir_module->getContext());

    llvm::Function *main = addFunction(ir_module, "main", llvm::FunctionType::get(llvm::Type::getInt32Ty(ir_module->getContext()), false));
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ir_module->getContext(), "", main);
    builder.SetInsertPoint(entry);

    // 为局部变量 a 和 b 分配空间
    llvm::Value *a = builder.CreateAlloca(llvm::Type::getInt32Ty(ir_module->getContext()), nullptr, "a");
    llvm::Value *b = builder.CreateAlloca(llvm::Type::getInt32Ty(ir_module->getContext()), nullptr, "b");

    // 将初始值存储到 a 和 b 中
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 1), a);
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 2), b);

    // 加载 a 和 b 的值
    llvm::Value *load_a = builder.CreateLoad(llvm::Type::getInt32Ty(ir_module->getContext()), a, "load_a");
    llvm::Value *load_b = builder.CreateLoad(llvm::Type::getInt32Ty(ir_module->getContext()), b, "load_b");

    // 比较 a 和 b
    llvm::Value *cmp = builder.CreateICmpSLT(load_a, load_b, "cmp");

    // 创建用于条件分支的基本块
    llvm::BasicBlock *if_true = llvm::BasicBlock::Create(ir_module->getContext(), "if_true", main);
    llvm::BasicBlock *if_false = llvm::BasicBlock::Create(ir_module->getContext(), "if_false", main);
    llvm::BasicBlock *mergeBlock = llvm::BasicBlock::Create(ir_module->getContext(), "mergeBlock", main);

    // 创建条件分支
    builder.CreateCondBr(cmp, if_true, if_false);

    // 真分支：将 b 设置为 3
    builder.SetInsertPoint(if_true);
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 3), b);
    builder.CreateBr(mergeBlock);

    // 假分支：直接跳转到合并块
    builder.SetInsertPoint(if_false);
    builder.CreateBr(mergeBlock);

    // 合并块：将 a 和 b 相加并返回结果
    builder.SetInsertPoint(mergeBlock);
    llvm::Value *finalA = builder.CreateLoad(llvm::Type::getInt32Ty(ir_module->getContext()), a, "finalA");
    llvm::Value *finalB = builder.CreateLoad(llvm::Type::getInt32Ty(ir_module->getContext()), b, "finalB");
    llvm::Value *add = builder.CreateAdd(finalA, finalB, "add");
    builder.CreateRet(add);
}

void buildFunction(std::shared_ptr<llvm::Module> ir_module)
{
    buildMain(ir_module);
}

int main()
{
    llvm::LLVMContext context;
    std::shared_ptr<llvm::Module> ir_module = std::make_shared<llvm::Module>("easy", context);
    ir_module->setTargetTriple("x86_64-pc-linux-gnu");

    buildGlobal(ir_module);
    buildFunction(ir_module);

    ir_module->print(llvm::outs(), nullptr);

    return 0;
}