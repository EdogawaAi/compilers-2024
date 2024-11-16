#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <map>

std::map<std::string, llvm::StructType *> struct_types;
std::map<std::string, llvm::GlobalVariable *> global_values;
std::map<std::string, llvm::Function *> functions;

llvm::StructType *addStructType(std::shared_ptr<llvm::Module> ir_module, std::string name, std::vector<llvm::Type *> fields)
{
    llvm::StructType *struct_type = llvm::StructType::create(ir_module->getContext(), name);
    struct_type->setBody(fields);
    struct_types.insert(std::make_pair(name, struct_type));
    return struct_type;
}

llvm::GlobalVariable *addGlobalValue(std::shared_ptr<llvm::Module> ir_module, std::string name, llvm::Type *type, llvm::Constant *initializer, int align)
{
    llvm::GlobalVariable *global = (llvm::GlobalVariable *)ir_module->getOrInsertGlobal(name, type);
    global->setInitializer(initializer);
    global->setDSOLocal(true);
    global->setAlignment(llvm::MaybeAlign(align));
    global_values.insert(std::make_pair(name, global));
    return global;
}

llvm::GlobalVariable *addGlobalString(std::shared_ptr<llvm::Module> ir_module, std::string name, std::string value)
{
    llvm::GlobalVariable *global = (llvm::GlobalVariable *)ir_module->getOrInsertGlobal(name, llvm::ArrayType::get(llvm::Type::getInt8Ty(ir_module->getContext()), value.size() + 1));
    global->setInitializer(llvm::ConstantDataArray::getString(ir_module->getContext(), value, true));
    global->setDSOLocal(true);
    global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    global->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
    global->setConstant(true);
    global->setAlignment(llvm::MaybeAlign(1));
    global_values.insert(std::make_pair(name, global));
    return global;
}

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
    llvm::IRBuilder<> ir_builder(ir_module->getContext());

    // add struct type
    llvm::Type *Edge_s_fields[] = {llvm::Type::getInt32Ty(ir_module->getContext()), llvm::Type::getInt32Ty(ir_module->getContext()), llvm::Type::getInt64Ty(ir_module->getContext())};
    llvm::StructType *Edge_s = addStructType(ir_module, "struct.Edge_s", std::vector<llvm::Type *>(Edge_s_fields, Edge_s_fields + 3));

    // add global value
    llvm::PointerType::get(Edge_s, 0);
    llvm::GlobalVariable *edge1 = addGlobalValue(ir_module, "edge1", Edge_s, llvm::ConstantStruct::get(Edge_s, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 5)), 8);
    llvm::GlobalVariable *edge2 = addGlobalValue(ir_module, "edge2", Edge_s, llvm::ConstantStruct::get(Edge_s, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 10)), 8);
    addGlobalValue(ir_module, "allDist", llvm::ArrayType::get(llvm::ArrayType::get(llvm::Type::getInt32Ty(ir_module->getContext()), 3), 3), llvm::ConstantAggregateZero::get(llvm::ArrayType::get(llvm::ArrayType::get(llvm::Type::getInt32Ty(ir_module->getContext()), 3), 3)), 16);
    addGlobalValue(ir_module, "dist", llvm::ArrayType::get(llvm::PointerType::get(Edge_s, 0), 3), llvm::ConstantArray::get(llvm::ArrayType::get(llvm::PointerType::get(Edge_s, 0), 3), {llvm::ConstantExpr::getBitCast(edge1, llvm::PointerType::get(Edge_s, 0)), llvm::ConstantExpr::getBitCast(edge2, llvm::PointerType::get(Edge_s, 0)), llvm::ConstantPointerNull::get(llvm::PointerType::get(Edge_s, 0))}), 16);
    addGlobalValue(ir_module, "minDistance", llvm::Type::getInt64Ty(ir_module->getContext()), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 5), 8);

    // add global string
    llvm::GlobalVariable *str = addGlobalString(ir_module, ".str", "%lld\00");
    llvm::GlobalVariable *str1 = addGlobalString(ir_module, ".str1", "%lld %lld %d\n\00");

    // add external function
    addFunction(ir_module, "__isoc99_scanf", llvm::FunctionType::get(llvm::Type::getInt32Ty(ir_module->getContext()), llvm::PointerType::get(llvm::Type::getInt8Ty(ir_module->getContext()), 0), true));
    addFunction(ir_module, "printf", llvm::FunctionType::get(llvm::Type::getInt32Ty(ir_module->getContext()), llvm::PointerType::get(llvm::Type::getInt8Ty(ir_module->getContext()), 0), true));
}

void buildCaculateDistance(std::shared_ptr<llvm::Module> ir_module)
{
    llvm::IRBuilder<> builder(ir_module->getContext());

    llvm::Function *caculateDistance = addFunction(ir_module, "caculateDistance", llvm::FunctionType::get(llvm::Type::getVoidTy(ir_module->getContext()), false));
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ir_module->getContext(), "", caculateDistance);
    builder.SetInsertPoint(entry);

    // TODO
    //分配空间
    llvm::Value *k = builder.CreateAlloca(llvm::Type::getInt32Ty(ir_module->getContext()), nullptr, "k");
    llvm::Value *kDist = builder.CreateAlloca(llvm::Type::getInt64Ty(ir_module->getContext()), nullptr, "kDist");

    //初始化循环变量k
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), k);

    //创建循环基本block
    llvm::BasicBlock *for_loop = llvm::BasicBlock::Create(ir_module->getContext(), "for.loop", caculateDistance);
    llvm::BasicBlock *afterLoop = llvm::BasicBlock::Create(ir_module->getContext(), "afterLoop", caculateDistance);

    //进入循环
    builder.CreateBr(for_loop);
    builder.SetInsertPoint(for_loop);

    //Load 循环变量k
    llvm::Value *kValue = builder.CreateLoad(llvm::Type::getInt32Ty(ir_module->getContext()), k, "kValue");

    // 比较k是否小于3
    llvm::Value *cond = builder.CreateICmpSLT(kValue, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 3), "cond");
    builder.CreateCondBr(cond, for_loop, afterLoop);

    //循环体
    builder.SetInsertPoint(for_loop);

    //获取dist[k]的值
    llvm::Value *distPtr = builder.CreateGEP(global_values["dist"]->getValueType(), global_values["dist"], {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 0), kValue}, "distPtr");
    llvm::Value *edgePtr = builder.CreateLoad(llvm::PointerType::get(struct_types["struct.Edge_s"], 0), distPtr, "edgePtr");
    llvm::Value *wPtr = builder.CreateGEP(struct_types["struct.Edge_s"], edgePtr, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 2)}, "wPtr");
    llvm::Value *wValue = builder.CreateLoad(llvm::Type::getInt64Ty(ir_module->getContext()), wPtr, "wValue");

    //存储KDist
    builder.CreateStore(wValue, kDist);

    //比较kDist和minDistance
    llvm::Value *minDistance = builder.CreateLoad(llvm::Type::getInt64Ty(ir_module->getContext()), global_values["minDistance"], "minDistance");
    llvm::Value *kDistValue = builder.CreateLoad(llvm::Type::getInt64Ty(ir_module->getContext()), kDist, "kDistValue");
    llvm::Value *cmp = builder.CreateICmpSLT(kDistValue, minDistance, "cmp");

    //创建条件分支
    llvm::BasicBlock *trueBlock = llvm::BasicBlock::Create(ir_module->getContext(), "trueBlock", caculateDistance);
    llvm::BasicBlock *falseBlock = llvm::BasicBlock::Create(ir_module->getContext(), "falseBlock", caculateDistance);

    builder.CreateCondBr(cmp, trueBlock, falseBlock);

    //真分支：更新minDistance
    builder.SetInsertPoint(trueBlock);
    builder.CreateStore(wValue, global_values["minDistance"]);
    builder.CreateBr(falseBlock);

    //假分支：不做任何操作
    builder.SetInsertPoint(falseBlock);

    //更新循环变量k
    llvm::Value *kValueAdd = builder.CreateAdd(kValue, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 1), "kValueAdd");
    builder.CreateStore(kValueAdd, k);
    builder.CreateBr(for_loop);

    //循环结束
    builder.SetInsertPoint(afterLoop);
    builder.CreateRetVoid();
}

void buildMain(std::shared_ptr<llvm::Module> ir_module)
{
    llvm::IRBuilder<> builder(ir_module->getContext());

    llvm::Function *main = addFunction(ir_module, "main", llvm::FunctionType::get(llvm::Type::getInt32Ty(ir_module->getContext()), false));
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ir_module->getContext(), "", main);
    builder.SetInsertPoint(entry);

    // 为局部变量分配空间
    llvm::Value *edge = builder.CreateAlloca(struct_types["struct.Edge_s"], nullptr, "edge");

    // 读取输入
    llvm::Value *wPtr = builder.CreateGEP(struct_types["struct.Edge_s"], edge, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 2)}, "wPtr");
    llvm::Function *scanf = functions["__isoc99_scanf"];
    llvm::Value *formatStr = builder.CreateGlobalStringPtr("%lld", "formatStr");
    builder.CreateCall(scanf, {formatStr, wPtr});

    // 将读取的边存储到 dist 数组中
    llvm::Value *distPtr = builder.CreateGEP(global_values["dist"]->getValueType(), global_values["dist"], {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 2)}, "distPtr");
    builder.CreateStore(edge, distPtr);

    // 将边的距离存储到 allDist 数组中
    llvm::Value *allDistPtr = builder.CreateGEP(global_values["allDist"]->getValueType(), global_values["allDist"], {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0), llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0)}, "allDistPtr");
    llvm::Value *wValue = builder.CreateLoad(llvm::Type::getInt64Ty(ir_module->getContext()), wPtr, "wValue");
    llvm::Value *wTrunc = builder.CreateTrunc(wValue, llvm::Type::getInt32Ty(ir_module->getContext()), "wTrunc");
    builder.CreateStore(wTrunc, allDistPtr);

    // 调用 caculateDistance 函数
    llvm::Function *caculateDistance = functions["caculateDistance"];
    builder.CreateCall(caculateDistance);

    // 计算并输出结果
    llvm::Value *minDistanceValue = builder.CreateLoad(llvm::Type::getInt64Ty(ir_module->getContext()), global_values["minDistance"], "minDistanceValue");
    llvm::Value *sum = builder.CreateAdd(wValue, llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 5), "sum1");
    sum = builder.CreateAdd(sum, llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_module->getContext()), 10), "sum2");
    llvm::Value *allDistValue = builder.CreateLoad(llvm::Type::getInt64Ty(ir_module->getContext()), allDistPtr, "allDistValue");

    llvm::Function *printf = functions["printf"];
    llvm::Value *formatStr1 = builder.CreateGlobalStringPtr("%lld %lld %d\n", "formatStr1");
    builder.CreateCall(printf, {formatStr1, minDistanceValue, sum, allDistValue});

    // 返回 0
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ir_module->getContext()), 0));
}

void buildFunction(std::shared_ptr<llvm::Module> ir_module)
{
    buildCaculateDistance(ir_module);
    buildMain(ir_module);
}

int main(int, char **)
{
    llvm::LLVMContext context;
    std::shared_ptr<llvm::Module> ir_module = std::make_shared<llvm::Module>("calculateDistance", context);
    ir_module->setTargetTriple("x86_64-pc-linux-gnu");

    buildGlobal(ir_module);
    buildFunction(ir_module);

    ir_module->print(llvm::outs(), nullptr);

    return 0;
}
