//===- examples/RegAllocUser/regalloc-user.cpp - Simplest regalloc user ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is the simplest demonstration ever to show how to use regalloc
//
//===----------------------------------------------------------------------===//
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/CodeGen/MIRParser/MIRParser.h"
#include "llvm/CodeGen/MIRPrinter.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachinePassManager.h"
#include "llvm/CodeGen/MachineVerifier.h"
#include "llvm/CodeGen/RegAllocFast.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Target/CGPassBuilderOption.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace llvm;

static codegen::RegisterCodeGenFlags CGF; // Registers flags from CodeGen/CommandFlags.h

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input mir-code>"), cl::init("-"));

static cl::opt<std::string> OutputFilename("o", cl::desc("Output filename"),
                                           cl::value_desc("filename"));

static cl::opt<std::string>
    TargetTriple("mtriple", cl::desc("Override target triple for module"));

// struct LLCDiagnoscticLevel : public Diag
//  DiagnosticHandler.h is defined in llvm/IR/DiagnosticHandler.h and I'm not
//  sure it is needed for MIR.

[[noreturn]] static void reportError(Twine Msg, StringRef Filename = "") {
  SmallString<256> Prefix;
  if (!Filename.empty()) {
    if (Filename == "-")
      Filename = "<stdin>";
    ("'" + Twine(Filename) + "': ").toStringRef(Prefix);
  }
  WithColor::error(errs(), "regalloc-user") << Prefix << Msg << "\n";
  exit(1);
}

[[maybe_unused]] [[noreturn]] static void reportError(Error Err,
                                                      StringRef Filename) {
  assert(Err);
  handleAllErrors(createFileError(Filename, std::move(Err)),
                  [&](const ErrorInfoBase &EI) { reportError(EI.message()); });
  llvm_unreachable("reportError() should not return");
}

static std::unique_ptr<ToolOutputFile> GetOutputStream(const char *TargetName,
                                                       Triple::OSType OS,
                                                       const char *ProgName) {
  // If we don't yet have an output filename, make one.
  if (OutputFilename.empty()) {
    if (InputFilename == "-") {
      OutputFilename = "-";
    } else {
      // If InputFilename ends with .mir, remove it.
      StringRef IFN = InputFilename;
      if (IFN.ends_with(".mir")) {
        OutputFilename =
            std::string(IFN.drop_back(4)).append(".out").append(".mir");
      } else {
        OutputFilename = std::string(IFN).append(".out.mir");
      }
      switch (codegen::getFileType()) {
      case CodeGenFileType::Null:
        OutputFilename = "-";
        break;
      default:
        break;
      }
    }
  }

  // Open the file.
  std::error_code EC;
  sys::fs::OpenFlags OpenFlags = sys::fs::OF_None;
  auto FDOut = std::make_unique<ToolOutputFile>(OutputFilename, EC, OpenFlags);
  if (EC)
    reportError(EC.message());
  return FDOut;
}

int main(int argc, char **argv) {
  // Initialize targets first, so that --version shows registered targets.
  // Without this, no target for a passed target triple will be detected.
  InitializeNativeTarget();

  // For all supported (built with) targets:
  //InitializeAllTargetInfos();
  //InitializeAllTargets();
  // initAsmInfo() is called from the constructor of <Target>TargetMachine, so
  // and checks for MCRegInfo initialization. So, MCs must be initialized.
  //InitializeAllTargetMCs();

  cl::ParseCommandLineOptions(argc, argv, "regalloc-user\n");

  LLVMContext Context;
  // Set a diagnostic handler that doesn't exit on the first error
  // Context.setDiagnosticHandler(std::make_unique<LLCDiagnosticHandler>());

  Triple TheTriple;
  std::string CPUStr = codegen::getCPUStr(),
              FeaturesStr = codegen::getFeaturesStr();
  TargetOptions Options;
  SMDiagnostic Err;
  std::unique_ptr<Module> M;
  std::unique_ptr<MIRParser> MIR;

  std::optional<Reloc::Model> RM = codegen::getExplicitRelocModel();
  std::optional<CodeModel::Model> CM = codegen::getExplicitCodeModel();

  const Target *TheTarget = nullptr;
  std::unique_ptr<TargetMachine> Target;

  auto SetDataLayout =
      [&](StringRef DataLayoutTargetTriple,
          [[maybe_unused]] StringRef OldDLStr) -> std::optional<std::string> {
    // If we are supposed to override the target triple, do so now.
    std::string IRTargetTriple = DataLayoutTargetTriple.str();
    if (!TargetTriple.empty())
      IRTargetTriple = Triple::normalize(TargetTriple);
    TheTriple = Triple(IRTargetTriple);
    if (TheTriple.getTriple().empty())
      TheTriple.setTriple(sys::getDefaultTargetTriple());

    std::string Error;
    TheTarget =
        TargetRegistry::lookupTarget(codegen::getMArch(), TheTriple, Error);
    if (!TheTarget) {
      WithColor::error(errs(), argv[0]) << Error;
      exit(1);
    }

    // InitializeOptions(TheTriple);
    Target = std::unique_ptr<TargetMachine>(TheTarget->createTargetMachine(
        TheTriple.getTriple(), CPUStr, FeaturesStr, Options, RM, CM,
        CodeGenOptLevel::None));
    assert(Target && "Could not allocate target machine!");

    return Target->createDataLayout().getStringRepresentation();
  };

  assert(StringRef(InputFilename).ends_with(".mir"));

  // Set attributes on functions as loaded from MIR from command line arguments.
  auto setMIRFunctionAttributes = [&CPUStr, &FeaturesStr](Function &F) {
    codegen::setFunctionAttributes(CPUStr, FeaturesStr, F);
  };

  MIR = createMIRParserFromFile(InputFilename, Err, Context,
                                setMIRFunctionAttributes);
  if (MIR)
    M = MIR->parseIRModule(SetDataLayout); // Note: Good idea to split off
                        // getting the actual layout into a callback.
  if (!TargetTriple.empty())
    M->setTargetTriple(Triple::normalize(TargetTriple));
  std::optional<CodeModel::Model> CM_IR = M->getCodeModel();
  if (!CM && CM_IR)
    Target->setCodeModel(*CM_IR);

  assert(M && "Should have exited if we didn't have a module!");

  // Figure out where we are going to send the output.
  std::unique_ptr<ToolOutputFile> Out =
      GetOutputStream(TheTarget->getName(), TheTriple.getOS(), argv[0]);
  if (!Out)
    return 1;

  // Ensure the filename is passed down to CodeViewDebug.
  Target->Options.ObjectFilenameForDebug = Out->outputFilename();

  // Tell target that this tool is not necessarily used with argument ABI
  // compliance (i.e. narrow integer argument extensions).
  Target->Options.VerifyArgABICompliance = 0;

  // Add an appropriate TargetLibraryInfo pass for the module's triple.
  TargetLibraryInfoImpl TLII(Triple(M->getTargetTriple()));

  // Flush the output
  {
    raw_pwrite_stream *OS = &Out->os();

    // Manually do the buffering rather than using buffer_ostream,
    // so we can memcpm the contents in CompileTwice mode
    SmallVector<char, 0> Buffer;
    std::unique_ptr<raw_svector_ostream> BOS;
    // I'm not sure if this is actual for my driver, but let it be.
    if (codegen::getFileType() != CodeGenFileType::AssemblyFile && !Out->os().supportsSeeking()) {
      BOS = std::make_unique<raw_svector_ostream>(Buffer);
      OS = BOS.get();
    }

    // Fetch options from TargetPassConfig
    // from Target/CGPassBuilderOption.h
    CGPassBuilderOption Opt = getCGPassBuilderOption();
    Opt.DisableVerify = false;
    Opt.DebugPM = false;
    Opt.RegAlloc = "fast"; // TODO for change

    MachineModuleInfo MMI(Target.get());
    PassInstrumentationCallbacks PIC;
    StandardInstrumentations SI(Context, /*DebugLogging=*/false,
                                /*VerifyEach=*/false);
    registerCodeGenCallback(PIC, *Target);

    MachineFunctionAnalysisManager MFAM;
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;
    PassBuilder PB(Target.get(), PipelineTuningOptions(), std::nullopt, &PIC);
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.registerMachineFunctionAnalyses(MFAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM, &MFAM);
    // Not needed for running passes, but required to let -print-after
    // /-print-before work
    SI.registerCallbacks(PIC, &MAM);

    FAM.registerPass([&] { return TargetLibraryAnalysis(TLII); });
    MAM.registerPass([&] { return MachineModuleAnalysis(MMI); });

    ModulePassManager MPM;
    FunctionPassManager FPM;

    // For printing the output MIR code.
    MPM.addPass(PrintMIRPreparePass(*OS));
    MachineFunctionPassManager MFPM;
    MFPM.addPass(RegAllocFastPass());
    MFPM.addPass(MachineVerifierPass());
    // For printing the output YAML including new body.
    MFPM.addPass(PrintMIRPass(*OS));
    FPM.addPass(createFunctionToMachineFunctionPassAdaptor(std::move(MFPM)));
    MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));

    // What if we exclude? No parsing - no content (functions) in the output
    // .mir file.
    if (MIR->parseMachineFunctions(*M, MAM)) // if MMI is passed - core dump on nullptr
       return 1;

    // Before executing passes, print the final values of the LLVM options.
    cl::PrintOptionValues();

    MPM.run(*M, MAM);

    if (Context.getDiagHandlerPtr()->HasErrors)
      exit(1);

    if (BOS) {
      Out->os() << Buffer;
    }
  }

  // Declare success.
  Out->keep();

  return 0;
}