/*!
 * Copyright (c) 2019 by Contributors
 * \file src/impl/ir_ext.cc
 * \brief MNM extension to TVM/Relay IR.
 */
#include <printer/text_printer.h>
#include "mnm/ir_ext.h"
#include "mnm/registry.h"
#include "mnm/pass.h"
#include "mnm/value.h"

namespace mnm {
namespace ir {

using namespace mnm::value;

IRModule GlobalModule() {
  static IRModule inst = IRModule();
  return inst;
}

tvm::runtime::NDArray MakeFakeTensor() {
  static int64_t a[1] = {-114514};
  static int64_t b[1] = {1};
  Device dev = mnm::Device(mnm::DevType::kCPU(), 0);
  DType dtype = mnm::DType(mnm::DTypeCode::kInt(), 64, 1);
  DLTensor tensor;
  tensor.data = a;
  tensor.device = dev;
  tensor.dtype = dtype;
  tensor.shape = b;
  tensor.ndim = 0;
  tensor.strides = nullptr;
  tensor.byte_offset = 0;
  auto array = tvm::runtime::NDArray::Empty({}, dtype, dev);
  array.CopyFrom(&tensor);
  return array;
}

bool ConstantNode::IsTensor() const {
  return value.defined() && value.as<BaseTensorValueObj>();
}

ObjectPtr<ConstantNode> MakeConstantNode(ObjectRef node_ref) {
  ObjectPtr<ConstantNode> n = make_object<ConstantNode>();
  static const auto fake_tensor = MakeFakeTensor();
  n->data = fake_tensor;
  n->value = std::move(node_ref);
  return n;
}

RelayConstant MakeConstant(ObjectRef node_ref) {
  ObjectPtr<ConstantNode> n = MakeConstantNode(node_ref);
  return RelayConstant(n);
}

RelayConstant MakeNull() {
  return MakeConstant(NullValue<Value>());
}

ObjectRef ConstantExtractValue(RelayConstant _node) {
  const ConstantNode* node = static_cast<const ConstantNode*>(_node.get());
  return node->value;
}

Var MakeVar_(Id vid, Type type_annotation, Var may_share = Var()) {
  ObjectPtr<ExtendedVarNode> n = make_object<ExtendedVarNode>();
  n->vid = std::move(vid);
  n->type_annotation = std::move(type_annotation);
  n->may_share = may_share;
  return Var(n);
}

Var MakeVar(const std::string& name_hint, Type type_annotation, Var may_share) {
  return MakeVar_(Id(name_hint), type_annotation, may_share);
}

/*!
 * \brief Extract the may_share field of an extended variable
 * \param var the variable
 * \return the may_share field of this variable
 */
Var GetMayShare(Var var) {
  const auto* vn = var.as<ExtendedVarNode>();
  return vn->may_share;
}

MNM_REGISTER_GLOBAL("mnm.ir.AsText").set_body_typed([](ObjectRef value) {
  auto annotate =
      tvm::runtime::TypedPackedFunc<String(ObjectRef)>([](const ObjectRef& expr) -> String {
        std::ostringstream os;
        const auto* constant = expr.as<ConstantNode>();
        if (constant) {
          // \b to erase "-114514"
          if (constant->value.defined()) {
            os << "\b\b\b\b\b\b\b" << constant->value;
          } else {
            os << "\b\b\b\b\b\b\bnullptr";
          }
        }
        if ((expr.as<ConstantNode>() || expr.as<CallNode>()) &&
            Downcast<Expr>(expr)->checked_type_.defined()) {
          tvm::relay::RelayTextPrinter printer(false, nullptr, nullptr);
          os << " /* ty=" << printer.Print(Downcast<Expr>(expr)->checked_type()).str() << " */";
        }
        const auto* extended_var = expr.as<ExtendedVarNode>();
        if (extended_var && extended_var->may_share.defined()) {
          os << "(share: %" << extended_var->may_share->name_hint() << ")";
        }
        return String(os.str());
      });
  return tvm::AsText(value, false, annotate);
});

String AsText(const ObjectRef& node) {
  return registry::GetPackedFunc("mnm.ir.AsText")(node);
}

MNM_REGISTER_GLOBAL("mnm.ir._make.Constant").set_body_typed(MakeConstant);
MNM_REGISTER_GLOBAL("mnm.ir._make.Var").set_body_typed(MakeVar);
MNM_REGISTER_GLOBAL("mnm.ir.constant.ExtractValue").set_body_typed(ConstantExtractValue);
MNM_REGISTER_GLOBAL("mnm.ir.variable.GetMayShare").set_body_typed(GetMayShare);
MNM_REGISTER_GLOBAL("mnm.ir.module.Global").set_body_typed(GlobalModule);

}  // namespace ir
}  // namespace mnm
