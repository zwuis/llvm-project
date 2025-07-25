//===- TosaOptionalDecompositions.cpp -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Pass to apply the Tosa operations decompositions
// exposed as populate functions in
// include/mlir/Dialect/Tosa/Transforms/Passes.h
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Tosa/Transforms/Passes.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

namespace mlir {
namespace tosa {
#define GEN_PASS_DEF_TOSAOPTIONALDECOMPOSITIONSPASS
#include "mlir/Dialect/Tosa/Transforms/Passes.h.inc"
} // namespace tosa
} // namespace mlir

using namespace mlir;

namespace {

struct TosaOptionalDecompositions
    : public tosa::impl::TosaOptionalDecompositionsPassBase<
          TosaOptionalDecompositions> {
  void runOnOperation() override {
    auto *ctx = &getContext();
    RewritePatternSet patterns(ctx);
    auto func = getOperation();

    mlir::tosa::populateTosaDecomposeTransposeConv(ctx, patterns);
    mlir::tosa::populateTosaDecomposeDepthwise(ctx, patterns);

    if (applyPatternsGreedily(func, std::move(patterns)).failed())
      signalPassFailure();
  }
};

} // namespace
