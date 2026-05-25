#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "parse/parse.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

int main(int argc, char **argv) {
    /// auto ast = arena::parse::parse("a.*.*.b.& * 3.as(int)");
    if (argc == 1) {
        std::cerr << "Usage: " << argv[0] << " <source-file>\n";
        return 1;
    } else if (argc > 2) {
        std::cerr << "Error: Only one file at a time is currently supported.\n";
        return 1;
    }

    // create a filesystem path
    std::filesystem::path file(argv[1]);
    if (!std::filesystem::exists(file)) {
        std::cerr << "Error: File does not exist: " << file << "\n";
        return 1;
    }

    // read the file into a string
    std::ifstream input_file(file);
    if (!input_file) {
        std::cerr << "Error: Could not open file: " << file << "\n";
        return 1;
    }

    // TODO: support larger files
    std::string input((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());

    auto ast = arena::parse::parse(input);
    std::cout << "Parsed AST: " << ast->to_string() << std::endl;
}

int main_llvm() {
    // Create an LLVM context and module
    llvm::LLVMContext context;
    llvm::Module module("example_module", context);

    // Create an IR builder
    llvm::IRBuilder<> builder(context);

    // Create a simple function: int add(int a, int b) { return a + b; }
    llvm::FunctionType *funcType =
        llvm::FunctionType::get(builder.getInt32Ty(),
                                {builder.getInt32Ty(), builder.getInt32Ty()},
                                false);

    llvm::Function *addFunc =
        llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "add", module);

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
