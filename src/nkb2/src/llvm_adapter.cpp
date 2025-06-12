#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>
#include <unistd.h> // TODO: Remove this, only used for _exit

#include "llvm_adapter_internal.h"
#include "ntk/log.h"

NK_LOG_USE_SCOPE(llvm_adapter);

void *lookupSymbol(LLVMOrcLLJITRef jit_, LLVMOrcJITDylibRef jd_, char const *name) {
    auto *jit = reinterpret_cast<llvm::orc::LLJIT *>(jit_);
    auto *jd = reinterpret_cast<llvm::orc::JITDylib *>(jd_);

    auto sym = jit->lookup(*jd, name);
    if (!sym) {
        auto err = sym.takeError();
        // TODO: Report errors properly
        llvm::visitErrors(err, [](llvm::ErrorInfoBase const &err_msg) {
            NK_LOG_ERR("%s", err_msg.message().c_str());
        });
        _exit(1);
        return NULL;
    }
    return sym.get().toPtr<void *>();
}
