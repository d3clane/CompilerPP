#pragma once

#include <string>

namespace llvm {
class Module;
}  // namespace llvm

namespace Front {

void LowerToExecutableFile(
    llvm::Module& module,
    const std::string& output_path);

}  // namespace Front
