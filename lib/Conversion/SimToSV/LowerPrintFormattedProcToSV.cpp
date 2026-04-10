//===- LowerPrintFormattedProcToSV.cpp - Lower proc.print to sv.fwrite ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass lowers sim.proc.print to sv.fwrite.
//
// Precondition: sim.proc.print must already be inside an SV procedural root
// (e.g. sv.initial/sv.always/sv.alwayscomb/sv.alwaysff). This pass does not
// lower sim.proc.print inside non-SV procedural containers such as
// hw.triggered.
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/Passes.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/SV/SVOps.h"
#include "circt/Dialect/Sim/SimOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/SmallPtrSet.h"

#define DEBUG_TYPE "sim-lower-print-formatted-proc-to-sv"

namespace circt {
#define GEN_PASS_DEF_LOWERPRINTFORMATTEDPROCTOSV
#include "circt/Conversion/Passes.h.inc"
} // namespace circt

using namespace circt;
using namespace sim;

namespace {

static void appendLiteralToFWriteFormat(SmallString<128> &formatString,
                                        StringRef literal) {
  for (char ch : literal) {
    if (ch == '%')
      formatString += "%%";
    else
      formatString.push_back(ch);
  }
}

static LogicalResult appendIntegerSpecifier(SmallString<128> &formatString,
                                            bool isLeftAligned,
                                            uint8_t paddingChar,
                                            std::optional<int32_t> width,
                                            char spec) {
  formatString.push_back('%');
  if (isLeftAligned)
    formatString.push_back('-');

  // SystemVerilog formatting only has built-in support for '0' and ' '. Keep
  // this lowering strict to avoid silently changing formatting semantics.
  if (paddingChar == '0') {
    formatString.push_back('0');
  } else if (paddingChar != ' ') {
    return failure();
  }

  if (width.has_value())
    formatString += std::to_string(width.value());

  formatString.push_back(spec);
  return success();
}

static void appendFloatSpecifier(SmallString<128> &formatString,
                                 bool isLeftAligned,
                                 std::optional<int32_t> fieldWidth,
                                 int32_t fracDigits, char spec) {
  formatString.push_back('%');
  if (isLeftAligned)
    formatString.push_back('-');
  if (fieldWidth.has_value())
    formatString += std::to_string(fieldWidth.value());
  formatString.push_back('.');
  formatString += std::to_string(fracDigits);
  formatString.push_back(spec);
}

static LogicalResult
getFlattenedFormatFragments(Value input, SmallVectorImpl<Value> &fragments,
                            std::string &failureReason) {
  if (auto concat = input.getDefiningOp<FormatStringConcatOp>()) {
    if (failed(concat.getFlattenedInputs(fragments))) {
      failureReason = "cyclic sim.fmt.concat is unsupported";
      return failure();
    }
    return success();
  }

  fragments.push_back(input);
  return success();
}

static LogicalResult
appendFormatFragmentToFWrite(Value fragment, SmallString<128> &formatString,
                             SmallVectorImpl<Value> &args,
                             std::string &failureReason) {
  Operation *fragmentOp = fragment.getDefiningOp();
  if (!fragmentOp) {
    failureReason =
        "block argument format strings are unsupported as sim.proc.print input";
    return failure();
  }

  return TypeSwitch<Operation *, LogicalResult>(fragmentOp)
      .Case<FormatLiteralOp>([&](auto literal) -> LogicalResult {
        appendLiteralToFWriteFormat(formatString, literal.getLiteral());
        return success();
      })
      .Case<FormatHierPathOp>([&](auto hierPath) -> LogicalResult {
        formatString += hierPath.getUseEscapes() ? "%M" : "%m";
        return success();
      })
      .Case<FormatCharOp>([&](auto fmt) -> LogicalResult {
        formatString += "%c";
        args.push_back(fmt.getValue());
        return success();
      })
      .Case<FormatDecOp>([&](auto fmt) -> LogicalResult {
        if (failed(appendIntegerSpecifier(formatString, fmt.getIsLeftAligned(),
                                          fmt.getPaddingChar(),
                                          fmt.getSpecifierWidth(), 'd'))) {
          failureReason = "sim.fmt.dec only supports paddingChar 32 (' ') or "
                          "48 ('0') for SystemVerilog lowering";
          return failure();
        }
        args.push_back(fmt.getValue());
        return success();
      })
      .Case<FormatHexOp>([&](auto fmt) -> LogicalResult {
        if (failed(appendIntegerSpecifier(
                formatString, fmt.getIsLeftAligned(), fmt.getPaddingChar(),
                fmt.getSpecifierWidth(),
                fmt.getIsHexUppercase() ? 'X' : 'x'))) {
          failureReason = "sim.fmt.hex only supports paddingChar 32 (' ') or "
                          "48 ('0') for SystemVerilog lowering";
          return failure();
        }
        args.push_back(fmt.getValue());
        return success();
      })
      .Case<FormatOctOp>([&](auto fmt) -> LogicalResult {
        if (failed(appendIntegerSpecifier(formatString, fmt.getIsLeftAligned(),
                                          fmt.getPaddingChar(),
                                          fmt.getSpecifierWidth(), 'o'))) {
          failureReason = "sim.fmt.oct only supports paddingChar 32 (' ') or "
                          "48 ('0') for SystemVerilog lowering";
          return failure();
        }
        args.push_back(fmt.getValue());
        return success();
      })
      .Case<FormatBinOp>([&](auto fmt) -> LogicalResult {
        if (failed(appendIntegerSpecifier(formatString, fmt.getIsLeftAligned(),
                                          fmt.getPaddingChar(),
                                          fmt.getSpecifierWidth(), 'b'))) {
          failureReason = "sim.fmt.bin only supports paddingChar 32 (' ') or "
                          "48 ('0') for SystemVerilog lowering";
          return failure();
        }
        args.push_back(fmt.getValue());
        return success();
      })
      .Case<FormatScientificOp>([&](auto fmt) -> LogicalResult {
        appendFloatSpecifier(formatString, fmt.getIsLeftAligned(),
                             fmt.getFieldWidth(), fmt.getFracDigits(), 'e');
        args.push_back(fmt.getValue());
        return success();
      })
      .Case<FormatFloatOp>([&](auto fmt) -> LogicalResult {
        appendFloatSpecifier(formatString, fmt.getIsLeftAligned(),
                             fmt.getFieldWidth(), fmt.getFracDigits(), 'f');
        args.push_back(fmt.getValue());
        return success();
      })
      .Case<FormatGeneralOp>([&](auto fmt) -> LogicalResult {
        appendFloatSpecifier(formatString, fmt.getIsLeftAligned(),
                             fmt.getFieldWidth(), fmt.getFracDigits(), 'g');
        args.push_back(fmt.getValue());
        return success();
      })
      .Default([&](auto unsupportedOp) {
        failureReason = (Twine("unsupported format fragment '") +
                         unsupportedOp->getName().getStringRef() + "'")
                            .str();
        return failure();
      });
}

static LogicalResult foldFormatStringToFWrite(Value input,
                                              SmallString<128> &formatString,
                                              SmallVectorImpl<Value> &args,
                                              std::string &failureReason) {
  SmallVector<Value, 8> fragments;
  if (failed(getFlattenedFormatFragments(input, fragments, failureReason)))
    return failure();
  for (auto fragment : fragments)
    if (failed(appendFormatFragmentToFWrite(fragment, formatString, args,
                                            failureReason)))
      return failure();
  return success();
}

static Operation *findProceduralRoot(Operation *op) {
  for (Operation *ancestor = op->getParentOp(); ancestor;
       ancestor = ancestor->getParentOp()) {
    if (isa<sv::InitialOp, sv::AlwaysOp>(ancestor))
      return ancestor;
  }
  return nullptr;
}

} // namespace

LogicalResult circt::sim::lowerPrintFormattedProcToSV(hw::HWModuleOp module) {
  bool sawError = false;
  SmallVector<PrintFormattedProcOp> printOps;
  module.walk([&](PrintFormattedProcOp op) {
    if (findProceduralRoot(op)) {
      printOps.push_back(op);
      return;
    }
    op.emitError("must be contained in a supported SV procedural root "
                 "(sv.initial/sv.always/sv.alwayscomb/sv.alwaysff) before "
                 "running --sim-lower-print-formatted-proc-to-sv");
    sawError = true;
  });

  if (sawError)
    return failure();

  llvm::SmallPtrSet<Operation *, 8> dceRoots;

  for (auto printOp : printOps) {
    SmallString<128> formatString;
    SmallVector<Value> args;
    std::string failureReason;
    if (failed(foldFormatStringToFWrite(printOp.getInput(), formatString, args,
                                        failureReason))) {
      auto diag =
          printOp.emitError("cannot lower 'sim.proc.print' to sv.fwrite: ");
      diag << failureReason;
      sawError = true;
      continue;
    }

    OpBuilder builder(printOp);
    // Align with FIRRTLToHW: default to writing to stderr.
    // Specifying an output stream is not currently supported.
    auto fd = hw::ConstantOp::create(builder, printOp.getLoc(),
                                     APInt(32, 0x80000002));
    sv::FWriteOp::create(builder, printOp.getLoc(), fd, formatString, args);
    if (Operation *procRoot = findProceduralRoot(printOp))
      dceRoots.insert(procRoot);
    printOp.erase();
  }

  mlir::IRRewriter rewriter(module);
  for (Operation *dceRoot : dceRoots)
    (void)mlir::runRegionDCE(rewriter, dceRoot->getRegions());
  return success();
}

namespace {

struct LowerPrintFormattedProcToSVPass
    : public impl::LowerPrintFormattedProcToSVBase<
          LowerPrintFormattedProcToSVPass> {
  void runOnOperation() override {
    if (failed(lowerPrintFormattedProcToSV(getOperation())))
      signalPassFailure();
  }
};

} // namespace
