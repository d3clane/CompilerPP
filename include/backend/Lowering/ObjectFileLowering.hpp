#pragma once

#include <string>

namespace llvm {
class Module;
}  // namespace llvm

namespace Back {

void LowerToObjectFile(
    llvm::Module& module,
    const std::string& output_path);

}  // namespace Back
