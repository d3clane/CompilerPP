#pragma once

#include <string>

namespace llvm {
class Module;
}  // namespace llvm

namespace Parsing {

void LowerToExecutableFile(
    llvm::Module& module,
    const std::string& output_path);

}  // namespace Parsing
