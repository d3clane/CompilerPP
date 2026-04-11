#pragma once

#include <string>

namespace llvm {
class Module;
}  // namespace llvm

namespace Parsing {

void LowerToObjectFile(
    llvm::Module& module,
    const std::string& output_path);

}  // namespace Parsing
