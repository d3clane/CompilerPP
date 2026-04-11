#include "Lowering/ExecutableLowering.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>

#include "Lowering/ObjectFileLowering.hpp"

namespace Parsing {

namespace {

class TemporaryFile {
 public:
  explicit TemporaryFile(std::string path)
      : path_(std::move(path)) {}

  TemporaryFile(const TemporaryFile&) = delete;
  TemporaryFile& operator=(const TemporaryFile&) = delete;

  ~TemporaryFile() {
    if (!path_.empty()) {
      auto err = llvm::sys::fs::remove(path_);
      assert(!err);
    }
  }

  const std::string& Path() const {
    return path_;
  }

 private:
  std::string path_;
};

TemporaryFile CreateTemporaryObjectFile() {
  llvm::SmallString<128> object_path;
  const std::error_code error_code =
      llvm::sys::fs::createTemporaryFile(
          "compilerpp",
          "o",
          object_path,
          llvm::sys::fs::OF_None);
  if (error_code) {
    throw std::runtime_error(
        "Failed to create temporary object file: " + error_code.message());
  }

  return TemporaryFile(object_path.str().str());
}

std::string FindLinkerProgram() {
  if (llvm::ErrorOr<std::string> clang =
          llvm::sys::findProgramByName("clang")) {
    return *clang;
  }

  if (llvm::ErrorOr<std::string> cc = llvm::sys::findProgramByName("cc")) {
    return *cc;
  }

  throw std::runtime_error(
      "Failed to find clang or cc to link executable");
}

void LinkObjectFileToExecutable(
    const std::string& object_path,
    const std::string& output_path) {
  const std::string linker = FindLinkerProgram();
  const std::vector<llvm::StringRef> args{
      linker,
      object_path,
      "-o",
      output_path};

  std::string error_message;
  bool execution_failed = false;
  const int exit_code = llvm::sys::ExecuteAndWait(
      linker,
      args,
      std::nullopt,
      {},
      0,
      0,
      &error_message,
      &execution_failed);
  if (execution_failed) {
    throw std::runtime_error(
        "Failed to execute linker " + linker + ": " + error_message);
  }

  if (exit_code != 0) {
    throw std::runtime_error(
        "Linker " +
        linker +
        " failed with exit code " +
        std::to_string(exit_code));
  }
}

}  // namespace

void LowerToExecutableFile(
    llvm::Module& module,
    const std::string& output_path) {
  TemporaryFile object_file = CreateTemporaryObjectFile();
  LowerToObjectFile(module, object_file.Path());
  LinkObjectFileToExecutable(object_file.Path(), output_path);
}

}  // namespace Parsing
