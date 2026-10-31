#ifndef ZIG_MBA_PASS_H
#define ZIG_MBA_PASS_H

#include <llvm/IR/PassManager.h>

namespace zig_mba {

struct MBAPass : public llvm::PassInfoMixin<MBAPass> {
    uint64_t seed;
    unsigned depth;
    unsigned prob;

    MBAPass(uint64_t s, unsigned d, unsigned p) : seed(s), depth(d), prob(p) {}
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
};

void registerMBAPass(llvm::PassBuilder &PB, uint64_t seed, unsigned depth, unsigned prob);

} // namespace zig_mba

#endif
