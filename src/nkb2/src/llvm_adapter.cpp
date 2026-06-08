#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>

#include "llvm_adapter_internal.h"
#include "ntk/error.h"

static llvm::Expected<llvm::orc::ExecutorAddr> lookupImpl(
    LLVMOrcLLJITRef clljit,
    LLVMOrcJITDylibRef cjd,
    NkString name) {
    auto *lljit = reinterpret_cast<llvm::orc::LLJIT *>(clljit);
    auto *jd = reinterpret_cast<llvm::orc::JITDylib *>(cjd);

    return lljit->lookup(*jd, llvm::StringRef{name.data, name.size});
}

void *tryLookupSymbol(LLVMOrcLLJITRef lljit, LLVMOrcJITDylibRef jd, NkString name) {
    auto sym = lookupImpl(lljit, jd, name);
    return sym ? sym.get().toPtr<void *>() : nullptr;
}

void *lookupSymbol(LLVMOrcLLJITRef lljit, LLVMOrcJITDylibRef jd, NkString name) {
    auto sym = lookupImpl(lljit, jd, name);
    if (!sym) {
        auto err = sym.takeError();
        nk_error_printf("Failed to lookup symbol: %s", llvm::toString(std::move(err)).c_str());
        return nullptr;
    }
    return sym.get().toPtr<void *>();
}
