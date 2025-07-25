// RUN: llvm-tblgen -gen-sd-node-info -I %p/../../../include %s | FileCheck %s

include "llvm/Target/Target.td"

def MyTarget : Target;

def my_node_1 : SDNode<
    "MyTargetISD::NODE_1",
    SDTypeProfile<1, 1, [SDTCisVT<0, i1>, SDTCisVT<1, i2>]>,
    [SDNPHasChain]
>;

let TSFlags = 42 in
def my_node_2 : SDNode<
    "MyTargetISD::NODE_2",
    SDTypeProfile<3, 1, [
        // Prefix of my_node_3 constraints.
        SDTCisVT<0, i1>,
        SDTCisPtrTy<1>,
        SDTCisInt<2>,
        SDTCisFP<3>,
    ]>,
    [SDNPMayStore, SDNPMayLoad, SDNPSideEffect,
     SDNPMemOperand, SDNPVariadic]
>;

let IsStrictFP = true, TSFlags = 24 in
def my_node_3 : SDNode<
    "MyTargetISD::NODE_3",
    SDTypeProfile<2, -1, [
        SDTCisVT<0, i1>,
        SDTCisPtrTy<1>,
        SDTCisInt<2>,
        SDTCisFP<3>,
        SDTCisVec<4>,
        SDTCisSameAs<6, 5>,
        SDTCisVTSmallerThanOp<8, 7>,
        SDTCisOpSmallerThanOp<10, 9>,
        SDTCisEltOfVec<12, 11>,
        SDTCisSubVecOfVec<14, 13>,
        SDTCVecEltisVT<15, i32>,
        SDTCisSameNumEltsAs<17, 16>,
        SDTCisSameSizeAs<19, 18>,
    ]>,
    [SDNPCommutative, SDNPAssociative, SDNPHasChain,
     SDNPOutGlue, SDNPInGlue, SDNPOptInGlue]
>;

// CHECK:       namespace llvm::MyTargetISD {
// CHECK-EMPTY:
// CHECK-NEXT:  enum GenNodeType : unsigned {
// CHECK-NEXT:    NODE_1 = ISD::BUILTIN_OP_END,
// CHECK-NEXT:    NODE_2,
// CHECK-NEXT:    NODE_3,
// CHECK-NEXT:  };
// CHECK-EMPTY:
// CHECK-NEXT:  static constexpr unsigned GENERATED_OPCODE_END = NODE_3 + 1;
// CHECK-EMPTY:
// CHECK-NEXT:  } // namespace llvm::MyTargetISD

// CHECK:       static constexpr char MyTargetSDNodeNamesStorage[] =
// CHECK-NEXT:    "\0"
// CHECK-NEXT:    "MyTargetISD::NODE_1\0"
// CHECK-NEXT:    "MyTargetISD::NODE_2\0"
// CHECK-NEXT:    "MyTargetISD::NODE_3\0"
// CHECK-NEXT:    ;

// CHECK:       static const SDTypeConstraint MyTargetSDTypeConstraints[] = {
// CHECK-NEXT:    /* 0 */ {SDTCisVT, 1, 0, MVT::i2},
// CHECK-SAME:            {SDTCisVT, 0, 0, MVT::i1},
// CHECK-NEXT:    /* 2 */ {SDTCisSameSizeAs, 19, 18, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisSameNumEltsAs, 17, 16, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCVecEltisVT, 15, 0, MVT::i32},
// CHECK-SAME:            {SDTCisSubVecOfVec, 14, 13, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisEltOfVec, 12, 11, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisOpSmallerThanOp, 10, 9, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisVTSmallerThanOp, 8, 7, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisSameAs, 6, 5, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisVec, 4, 0, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisFP, 3, 0, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisInt, 2, 0, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisPtrTy, 1, 0, MVT::INVALID_SIMPLE_VALUE_TYPE},
// CHECK-SAME:            {SDTCisVT, 0, 0, MVT::i1},
// CHECK-NEXT:  };
// CHECK-EMPTY:
// CHECK-NEXT:  static const SDNodeDesc MyTargetSDNodeDescs[] = {
// CHECK-NEXT:          {1, 1, 0|1<<SDNPHasChain, 0, 0, 1, 0, 2}, // NODE_1
// CHECK-NEXT:          {3, 1, 0|1<<SDNPVariadic|1<<SDNPMemOperand, 0, 42, 21, 11, 4}, // NODE_2
// CHECK-NEXT:          {2, -1, 0|1<<SDNPHasChain|1<<SDNPOutGlue|1<<SDNPInGlue|1<<SDNPOptInGlue, 0|1<<SDNFIsStrictFP, 24, 41, 2, 13}, // NODE_3
// CHECK-NEXT:  };
// CHECK-EMPTY:
// CHECK-NEXT: static const SDNodeInfo MyTargetGenSDNodeInfo(
// CHECK-NEXT:     /*NumOpcodes=*/3, MyTargetSDNodeDescs,
// CHECK-NEXT:     MyTargetSDNodeNames, MyTargetSDTypeConstraints);
