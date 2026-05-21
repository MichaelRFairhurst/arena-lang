#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

int main() {
    // Create an LLVM context and module
    llvm::LLVMContext context;
    llvm::Module module("example_module", context);
    
    // Create an IR builder
    llvm::IRBuilder<> builder(context);
    
    // Create a simple function: int add(int a, int b) { return a + b; }
    llvm::FunctionType *funcType = llvm::FunctionType::get(
        builder.getInt32Ty(),
        {builder.getInt32Ty(), builder.getInt32Ty()},
        false
    );
    
    llvm::Function *addFunc = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        "add",
        module
    );
    
    // Create a basic block
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", addFunc);
    builder.SetInsertPoint(entry);
    
    // Get function arguments
    auto args = addFunc->arg_begin();
    llvm::Value *a = args++;
    llvm::Value *b = args;
    a->setName("a");
    b->setName("b");
    
    // Create the addition instruction
    llvm::Value *sum = builder.CreateAdd(a, b, "sum");
    
    // Create the return instruction
    builder.CreateRet(sum);
    
    // Verify the function
    if (llvm::verifyFunction(*addFunc, &llvm::errs())) {
        llvm::errs() << "Error: Function verification failed!\n";
        return 1;
    }
    
    // Print the generated LLVM IR
    llvm::outs() << "Generated LLVM IR:\n";
    module.print(llvm::outs(), nullptr);
    
    return 0;
}
