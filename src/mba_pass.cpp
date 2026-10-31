#include "mba_pass.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Passes/PassBuilder.h>

using namespace llvm;

namespace zig_mba {

static uint64_t nextSeed(uint64_t s) {
    return s * 6364136223846793005ULL + 1442695040888963407ULL;
}

using IdentityFn = Value*(*)(IRBuilder<>&, Value*, Value*);

// x + y
static Value* add_0(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateSub(B.CreateShl(B.CreateOr(x, y), 1), B.CreateXor(x, y));
}
static Value* add_1(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateAdd(B.CreateShl(B.CreateAnd(x, y), 1), B.CreateXor(x, y));
}
static Value* add_2(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateAdd(B.CreateOr(x, y), B.CreateAnd(x, y));
}

// x ^ y
static Value* xor_0(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateSub(B.CreateOr(x, y), B.CreateAnd(x, y));
}
static Value* xor_1(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateOr(B.CreateAnd(B.CreateNot(x), y), B.CreateAnd(x, B.CreateNot(y)));
}
static Value* xor_2(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateAnd(B.CreateOr(x, y), B.CreateOr(B.CreateNot(x), B.CreateNot(y)));
}

// x | y
static Value* or_0(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateAdd(B.CreateAnd(x, B.CreateNot(y)), y);
}
static Value* or_1(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateSub(B.CreateAdd(x, y), B.CreateAnd(x, y));
}
static Value* or_2(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateNot(B.CreateAnd(B.CreateNot(x), B.CreateNot(y)));
}

// x & y
static Value* and_0(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateSub(B.CreateAdd(x, y), B.CreateOr(x, y));
}
static Value* and_1(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateNot(B.CreateOr(B.CreateNot(x), B.CreateNot(y)));
}
static Value* and_2(IRBuilder<>& B, Value* x, Value* y) {
    return B.CreateSub(B.CreateOr(x, y), B.CreateXor(x, y));
}

static IdentityFn add_ids[] = { add_0, add_1, add_2 };
static IdentityFn xor_ids[] = { xor_0, xor_1, xor_2 };
static IdentityFn or_ids[]  = { or_0,  or_1,  or_2  };
static IdentityFn and_ids[] = { and_0, and_1, and_2  };

PreservedAnalyses MBAPass::run(Function &F, FunctionAnalysisManager &AM) {
    if (F.isDeclaration()) return PreservedAnalyses::all();

    uint64_t s = seed;
    for (char c : F.getName())
        s = nextSeed(s ^ (uint64_t)c);

    bool changed = false;
    SmallVector<Instruction*, 64> worklist;

    for (auto &BB : F) {
        for (auto &I : BB) {
            auto *BO = dyn_cast<BinaryOperator>(&I);
            if (!BO) continue;
            unsigned op = BO->getOpcode();
            if (op != Instruction::Add && op != Instruction::Xor &&
                op != Instruction::Or  && op != Instruction::And)
                continue;
            if (BO->getType()->isVectorTy()) continue;
            s = nextSeed(s);
            if (prob > 1 && (s % prob) != 0) continue;
            worklist.push_back(BO);
        }
    }

    for (auto *BO : worklist) {
        unsigned op = BO->getOpcode();
        s = nextSeed(s);
        IdentityFn *ids = nullptr;
        unsigned count = 0;
        switch (op) {
            case Instruction::Add: ids = add_ids; count = 3; break;
            case Instruction::Xor: ids = xor_ids; count = 3; break;
            case Instruction::Or:  ids = or_ids;  count = 3; break;
            case Instruction::And: ids = and_ids; count = 3; break;
            default: continue;
        }
        IRBuilder<> B(BO);
        Value *replacement = ids[s % count](B, BO->getOperand(0), BO->getOperand(1));
        BO->replaceAllUsesWith(replacement);
        BO->eraseFromParent();
        changed = true;
    }

    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

void registerMBAPass(PassBuilder &PB, uint64_t seed, unsigned depth, unsigned prob) {
    PB.registerOptimizerLastEPCallback(
        [seed, depth, prob](ModulePassManager &MPM, OptimizationLevel OL) {
            if (OL == OptimizationLevel::O0) return;
            MPM.addPass(createModuleToFunctionPassAdaptor(MBAPass(seed, depth, prob)));
        });
}

} // namespace zig_mba
