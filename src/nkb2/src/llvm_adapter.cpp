#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>

#include "llvm_adapter_internal.h"
#include "ntk/error.h"

void *lookupSymbol(LLVMOrcLLJITRef clljit, LLVMOrcJITDylibRef cjd, char const *name) {
    auto *lljit = reinterpret_cast<llvm::orc::LLJIT *>(clljit);
    auto *jd = reinterpret_cast<llvm::orc::JITDylib *>(cjd);

    auto sym = lljit->lookup(*jd, name);
    if (!sym) {
        auto err = sym.takeError();
        nk_error_printf("Failed to lookup symbol: %s", llvm::toString(std::move(err)).c_str());
        return NULL;
    }
    return sym.get().toPtr<void *>();
}
