//===- SimToSV.h - SV conversion for sim ops ----------------===-*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares passes which lower `sim` to `sv` and `hw`.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_CONVERSION_SIMTOSV_H
#define CIRCT_CONVERSION_SIMTOSV_H

#include "circt/Dialect/HW/HWOps.h"
#include "circt/Support/LLVM.h"
#include <memory>

namespace circt {

#define GEN_PASS_DECL_LOWERPRINTFORMATTEDPROCTOSV
#define GEN_PASS_DECL_LOWERSIMTOSV
#include "circt/Conversion/Passes.h.inc"

namespace sim {
LogicalResult lowerPrintFormattedProcToSV(hw::HWModuleOp module);
} // namespace sim

std::unique_ptr<mlir::Pass> createLowerSimToSVPass();

} // namespace circt

#endif // CIRCT_CONVERSION_SIMTOSV_H
