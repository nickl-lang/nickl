#include "llvm_adapter.h"

#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <unistd.h> // TODO: Remove this, only used for _exit

void *nk_llvm_lookup(LLVMOrcLLJITRef jit_, LLVMOrcJITDylibRef jd_, char const *name) {
    auto *jit = reinterpret_cast<llvm::orc::LLJIT *>(jit_);
    auto *jd = reinterpret_cast<llvm::orc::JITDylib *>(jd_);

    auto sym = jit->lookup(*jd, name);
    if (!sym) {
        auto err = sym.takeError();
        // TODO: Report errors properly
        llvm::logAllUnhandledErrors(std::move(err), llvm::errs(), "ERROR: ");
        _exit(1);
        return NULL;
    }
    return sym.get().toPtr<void *>();
}
