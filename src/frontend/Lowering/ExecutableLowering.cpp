#include "Lowering/ExecutableLowering.hpp"

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <lld/Common/Driver.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FileUtilities.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/VersionTuple.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

#include "Lowering/ObjectFileLowering.hpp"
#include "llvm/Support/ErrorHandling.h"

LLD_HAS_DRIVER(macho)

namespace Parsing {
namespace {

std::string ReadFileTrimmed(llvm::StringRef path) {
  auto buffer_or_error = llvm::MemoryBuffer::getFile(path);
  assert(buffer_or_error);
  return buffer_or_error.get()->getBuffer().trim().str();
}

std::string RunXcrun(std::initializer_list<llvm::StringRef> extra_args) {
  auto xcrun = llvm::sys::findProgramByName("xcrun");
  assert(xcrun);
  const std::string& xcrun_path = *xcrun;

  llvm::SmallString<128> stdout_path;
  
  if (const std::error_code _ =
          llvm::sys::fs::createTemporaryFile("xcrun", "out", stdout_path)) {
    assert(false);
    return "";
  }
  llvm::FileRemover stdout_cleanup(stdout_path);

  llvm::SmallString<128> stderr_path;
  if (const std::error_code _ =
          llvm::sys::fs::createTemporaryFile("xcrun", "err", stderr_path)) {
    assert(false);
    return "";
  }
  llvm::FileRemover stderr_cleanup(stderr_path);

  std::vector<std::string> owned_args;
  owned_args.reserve(extra_args.size() + 1);
  owned_args.push_back(xcrun_path);
  for (llvm::StringRef arg : extra_args) {
    owned_args.push_back(arg.str());
  }

  std::vector<llvm::StringRef> args;
  args.reserve(owned_args.size());
  for (const std::string& arg : owned_args) {
    args.push_back(arg);
  }

  std::optional<llvm::StringRef> redirects[] = {
      std::nullopt,
      stdout_path.str(),
      stderr_path.str(),
  };

  std::string err_msg;
  bool execution_failed = false;
  const int return_code = llvm::sys::ExecuteAndWait(
      xcrun_path,
      args,
      std::nullopt,
      redirects,
      /*SecondsToWait=*/0,
      /*MemoryLimit=*/0,
      &err_msg,
      &execution_failed);
  assert(!execution_failed);
  assert(return_code == 0);

  const std::string stdout_text = ReadFileTrimmed(stdout_path);
  const std::string stderr_text = ReadFileTrimmed(stderr_path);
  
  return stdout_text;
}

bool HasUsableSdkRootEnv() {
  return std::getenv("SDKROOT") != nullptr;
}

std::string QueryMacOSSdk(llvm::StringRef query_flag) {
  if (HasUsableSdkRootEnv()) {
    return RunXcrun({query_flag});
  }
  return RunXcrun({"--sdk", "macosx", query_flag});
}

std::string GetMacOSSdkPath() {
  return QueryMacOSSdk("--show-sdk-path");
}

std::string GetMacOSSdkVersion() {
  return QueryMacOSSdk("--show-sdk-version");
}

std::string GetMinimumMacOSVersion(const llvm::Triple& target_triple) {
  llvm::VersionTuple version;
  if (target_triple.getMacOSXVersion(version) && version.getMajor() != 0) {
    return version.getAsString();
  }

  assert(false);
  llvm_unreachable("failure");
}

std::vector<std::string> BuildMachOArgs(
    const std::string& object_path,
    const std::string& output_path,
    const llvm::Triple& target_triple) {
  const std::string sdk_path = GetMacOSSdkPath();
  const std::string sdk_version = GetMacOSSdkVersion();

  return {
      "ld64.lld",
      "-arch",
      target_triple.getArchName().str(),
      "-platform_version",
      "macos",
      GetMinimumMacOSVersion(target_triple),
      sdk_version,
      "-syslibroot",
      sdk_path,
      "-o",
      output_path,
      object_path,
      "-lSystem",
  };
}

bool LinkMachOExecutable(
    const std::string& object_path,
    const std::string& output_path,
    const llvm::Triple& target_triple) {
  if (!target_triple.isOSDarwin()) {
    return false;
  }

  const std::vector<std::string> arg_storage =
      BuildMachOArgs(object_path, output_path, target_triple);

  std::vector<const char*> args;
  args.reserve(arg_storage.size());
  for (const std::string& arg : arg_storage) {
    args.push_back(arg.c_str());
  }

  std::string stdout_buffer;
  std::string stderr_buffer;
  llvm::raw_string_ostream stdout_stream(stdout_buffer);
  llvm::raw_string_ostream stderr_stream(stderr_buffer);

  const lld::Result result = lld::lldMain(
      args,
      stdout_stream,
      stderr_stream,
      {{lld::Darwin, &lld::macho::link}});

  assert(result.retCode == 0);

  stdout_stream.flush();
  stderr_stream.flush();

  return true;
}

}  // namespace

void LowerToExecutableFile(
    llvm::Module& module,
    const std::string& output_path) {
  llvm::SmallString<128> object_path;
  if (const std::error_code ec =
          llvm::sys::fs::createTemporaryFile("compilerpp", "o", object_path)) {
    throw std::runtime_error(
        "Failed to create temporary object file: " + ec.message());
  }

  llvm::FileRemover cleanup(object_path);

  LowerToObjectFile(module, object_path.str().str());
  bool res = LinkMachOExecutable(
      object_path.str().str(),
      output_path,
      llvm::Triple(module.getTargetTriple()));

  assert(res);
}

}  // namespace Parsing