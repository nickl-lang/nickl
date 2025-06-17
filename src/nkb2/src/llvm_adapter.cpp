#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>
#include <unistd.h> // TODO: Remove this, only used for _exit

#include "llvm_adapter_internal.h"
#include "ntk/log.h"

NK_LOG_USE_SCOPE(llvm_adapter);

void *lookupSymbol(LLVMOrcLLJITRef clljit, LLVMOrcJITDylibRef cjd, char const *name) {
    auto *lljit = reinterpret_cast<llvm::orc::LLJIT *>(clljit);
    auto *jd = reinterpret_cast<llvm::orc::JITDylib *>(cjd);

    auto sym = lljit->lookup(*jd, name);
    if (!sym) {
        auto err = sym.takeError();
        // TODO: Report errors properly
        NK_LOG_ERR("%s", llvm::toString(std::move(err)).c_str());
        _exit(1);
        return NULL;
    }
    return sym.get().toPtr<void *>();
}
