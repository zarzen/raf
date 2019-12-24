/*!
 * Copyright (c) 2019 by Contributors
 * \file src/op/grad/nn.cc
 * \brief Declaration of gradients
 */
#include "mnm/op.h"

namespace mnm {
namespace op {
namespace grad {

using namespace mnm::ir;

Expr Shape(const Expr& expr) {
  static auto op_shape = Op::Get("mnm.op.shape");
  return CallNode::make(op_shape, {expr});
}

Array<Expr> Conv2dGrad(const Var& y, const Expr& orig_call, const Array<Expr>& ograds) {
  // schema for conv2d is:
  //    x, w, stride, padding, dilation, groups
  // schema for conv2d_grad is:
  //    x_or_w, y, dy, shape, stride, padding, dilation, groups
  static auto op_dx = Op::Get("mnm.op.conv2d_dx");
  static auto op_dw = Op::Get("mnm.op.conv2d_dw");
  CHECK_EQ(ograds.size(), 1);
  const Expr& dy = ograds[0];
  const CallNode* call = orig_call.as<CallNode>();
  // TODO(@junrushao1994): this piece of code is particularly suitable for auto-gen
  CHECK_GE(call->args.size(), 6);
  const Expr& x = call->args[0];
  const Expr& w = call->args[1];
  const Expr& stride = call->args[2];
  const Expr& padding = call->args[3];
  const Expr& dilation = call->args[4];
  const Expr& groups = call->args[5];
  // dx: w, y, dy, shape(x), stride, padding, dilation, groups
  // dw: x, y, dy, shape(w), stride, padding, dilation, groups
  return {CallNode::make(op_dx, {w, y, dy, Shape(x), stride, padding, dilation, groups}),
          CallNode::make(op_dw, {x, y, dy, Shape(w), stride, padding, dilation, groups})};
}

MNM_OP_GRAD("mnm.op.conv2d", Conv2dGrad);

Array<Expr> ReluGrad(const Var& y, const Expr& orig_call, const Array<Expr>& ograds) {
  // schema for relu is:
  //    x
  // schema for relu_dx is:
  //    x, y, dy
  static auto op_dx = Op::Get("mnm.op.relu_dx");
  CHECK_EQ(ograds.size(), 1);
  const Expr& dy = ograds[0];
  const CallNode* call = orig_call.as<CallNode>();
  CHECK_GE(call->args.size(), 1);
  const Expr& x = call->args[0];
  return {CallNode::make(op_dx, {x, y, dy})};
}

MNM_OP_GRAD("mnm.op.relu", ReluGrad);

Array<Expr> BatchNormTrainGrad(const Var& y, const Expr& orig_call, const Array<Expr>& ograds,
                               const Array<Expr>& igrads) {
  // schema for batch_norm_train is:
  //    x, running_mean,running_var, w, b, momentum, eps
  // schema for batch_norm_train_dxwb is:
  //    dy, x, w, b, eps
  static auto op_dxwb = Op::Get("mnm.op.batch_norm_train_dxwb");
  const Expr& dy = ograds[0];
  const CallNode* call = orig_call.as<CallNode>();
  const Expr& x = call->args[0];
  const Expr& w = call->args[3];
  const Expr& b = call->args[4];
  const Expr& eps = call->args[6];
  const Expr &ret = CallNode::make(op_dxwb, {dy, x, w, b, eps});
  return {
    TupleGetItemNode::make(ret, 0),
    NullValue<Expr>(),
    NullValue<Expr>(),
    TupleGetItemNode::make(ret, 1),
    TupleGetItemNode::make(ret, 2),
  };
}

MNM_OP_FUSED_GRAD("mnm.op.batch_norm_train", BatchNormTrainGrad);

}  // namespace grad
}  // namespace op
}  // namespace mnm
