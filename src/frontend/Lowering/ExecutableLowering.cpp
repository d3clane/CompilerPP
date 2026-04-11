#include "Lowering/ExecutableLowering.hpp"

#include <cstdlib>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <lld/Common/Driver.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

#include "Lowering/ObjectFileLowering.hpp"

LLD_HAS_DRIVER(macho)

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
      const std::error_code ignored_error = llvm::sys::fs::remove(path_);
      (void)ignored_error;
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

bool IsValidMacOSSdkPath(const std::string& path) {
  return llvm::sys::fs::exists(path + "/usr/lib/libSystem.tbd");
}

std::string FindMacOSSdkPath() {
  if (const char* sdk_root = std::getenv("SDKROOT")) {
    if (IsValidMacOSSdkPath(sdk_root)) {
      return sdk_root;
    }
  }

  const std::vector<std::string> sdk_roots{
      "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk",
      "/Applications/Xcode.app/Contents/Developer/Platforms/"
      "MacOSX.platform/Developer/SDKs/MacOSX.sdk"};
  for (const auto& sdk_root : sdk_roots) {
    if (IsValidMacOSSdkPath(sdk_root)) {
      return sdk_root;
    }
  }

  throw std::runtime_error(
      "Failed to find a macOS SDK with usr/lib/libSystem.tbd");
}

std::string GetMacOSSdkVersion(const std::string& sdk_path) {
  llvm::SmallString<128> canonical_path;
  const std::error_code error_code =
      llvm::sys::fs::real_path(sdk_path, canonical_path);
  const llvm::StringRef sdk_name =
      error_code ? llvm::sys::path::filename(sdk_path)
                 : llvm::sys::path::filename(canonical_path);
  const std::regex version_regex(R"(MacOSX([0-9]+(\.[0-9]+)*)\.sdk)");
  std::smatch match;
  const std::string sdk_name_string = sdk_name.str();
  if (std::regex_match(sdk_name_string, match, version_regex)) {
    return match[1].str();
  }

  return "15.0";
}

std::string GetMinimumMacOSVersion(const llvm::Triple& target_triple) {
  if (target_triple.getArch() == llvm::Triple::aarch64) {
    return "11.0";
  }

  return "10.13";
}

void LinkMachOExecutable(
    const std::string& object_path,
    const std::string& output_path,
    const llvm::Triple& target_triple) {
  if (!target_triple.isOSDarwin()) {
    throw std::runtime_error(
        "Mach-O executable lowering requires a Darwin target triple, got " +
        target_triple.str());
  }

  const std::string sdk_path = FindMacOSSdkPath();
  const std::string min_macos_version =
      GetMinimumMacOSVersion(target_triple);
  const std::string sdk_version = GetMacOSSdkVersion(sdk_path);
  const std::string arch = target_triple.getArchName().str();

  const std::vector<const char*> args{
      "ld64.lld",
      "-arch",
      arch.c_str(),
      "-platform_version",
      "macos",
      min_macos_version.c_str(),
      sdk_version.c_str(),
      "-syslibroot",
      sdk_path.c_str(),
      "-o",
      output_path.c_str(),
      object_path.c_str(),
      "-lSystem"};

  std::string stdout_buffer;
  std::string stderr_buffer;
  llvm::raw_string_ostream stdout_stream(stdout_buffer);
  llvm::raw_string_ostream stderr_stream(stderr_buffer);
  const lld::Result result = lld::lldMain(
      args,
      stdout_stream,
      stderr_stream,
      {{lld::Darwin, &lld::macho::link}});
  stdout_stream.flush();
  stderr_stream.flush();

  if (result.retCode != 0) {
    throw std::runtime_error(
        "LLD Mach-O linker failed with exit code " +
        std::to_string(result.retCode) +
        ":\n" +
        stderr_buffer +
        stdout_buffer);
  }
}

}  // namespace

void LowerToExecutableFile(
    llvm::Module& module,
    const std::string& output_path) {
  TemporaryFile object_file = CreateTemporaryObjectFile();
  LowerToObjectFile(module, object_file.Path());
  LinkMachOExecutable(
      object_file.Path(),
      output_path,
      llvm::Triple(module.getTargetTriple()));
}

}  // namespace Parsing
