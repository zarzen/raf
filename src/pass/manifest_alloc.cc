/*!
 * Copyright (c) 2020 by Contributors
 * \file memory_alloc.cc
 * \brief Manifest memory allocation in the IR.
 */
#include <algorithm>
#include <vector>

#include "mnm/op.h"
#include "mnm/ir.h"
#include "mnm/ir_ext.h"
#include "mnm/value.h"
#include "mnm/pass.h"
#include "./let_list.h"
#include "tvm/relay/attrs/memory.h"

namespace tvm {
namespace relay {

extern bool IsDynamic(const Type& ty);
extern Expr ToTupleType(const Type& ty, const std::vector<Expr>& exprs);

}  // namespace relay
}  // namespace tvm

namespace mnm {
namespace pass {
namespace manifest_alloc {

using namespace mnm::ir;
using namespace mnm::value;

class InplaceVisitor : public MixedModeVisitor {
 public:
  void VisitExpr_(const LetNode* node) override {
    auto pre_visit = [this](const LetNode* node) {
      auto var = node->var.as<ExtendedVarNode>();
      if (var->may_share.defined()) {
        auto call = node->value.as<CallNode>();
        auto tup_get = node->value.as<TupleGetItemNode>();
        if (call) {
          // If the value of the var is a call node and this var has may_share defined,
          // it can only be a TensorType. We add the mapping from this var to its may_share var.
          var_share_map.emplace(node->var, std::vector<Var>{var->may_share});
        } else if (tup_get) {
          // If variables in a tuple share with others, we record the mapping from the tuple var to
          // a list of variables that the tuple items share with. The list could contain empty vars,
          // indicating that the corresponding item doesn't share memory with others.
          auto tup = Downcast<Var>(tup_get->tuple);
          if (var_share_map.count(tup) == 0) {
            size_t num_fields = Downcast<TupleType>(tup->checked_type())->fields.size();
            std::vector<Var> shares(num_fields);
            var_share_map.emplace(tup, shares);
          }
          var_share_map[tup][tup_get->index] = var->may_share;
        }
      }
    };
    auto post_visit = [this](const LetNode* node) {
      VisitExpr(node->value);
      VisitExpr(node->body);
    };
    ExpandANormalForm(node, pre_visit, post_visit);
  }

  /*! \brief Mapping from a var to a list of vars that it shares memory with.
   *  When the var is a tuple, #vars in the list must be the same as #items in the tuple.
   */
  std::unordered_map<Var, std::vector<Var>, ObjectPtrHash, ObjectPtrEqual> var_share_map;
};

class ManifestAllocMutator : public ExprMutator {
 public:
  ManifestAllocMutator() : scopes_{LetList()} {
  }

  Expr VisitExpr_(const TupleNode* node) {
    auto& scope = scopes_.back();
    Array<Expr> new_fields;
    for (auto field : node->fields) {
      auto new_field = VisitExpr(field);
      if (auto constant_field = field.as<ConstantNode>()) {
        auto const_var = scope.Push(field);
        new_field = const_var;
      }
      new_fields.push_back(new_field);
    }
    return Tuple(new_fields);
  }

  Expr VisitExpr_(const ConstantNode* node) {
    return scopes_.back().Push(GetRef<Expr>(node));
  }

  Expr VisitExpr_(const LetNode* node) {
    scopes_.emplace_back();
    auto& scope = scopes_.back();
    Expr body;
    do {
      let_binding_.emplace(node->value, node->var);
      scope.Push(node->var, VisitExpr(node->value));
      body = node->body;
      node = body.as<LetNode>();
    } while (node);
    auto new_body = VisitExpr(body);
    auto ret = scopes_.back().Get(new_body);
    scopes_.pop_back();
    return ret;
  }

  Expr VisitExpr_(const CallNode* node) {
    const auto* op = node->op.as<OpNode>();
    const auto* func = node->op.as<FunctionNode>();
    if (op || func && func->HasNonzeroAttr(attr::kPrimitive)) {
      auto& scope = scopes_.back();
      Var bind_var = let_binding_[GetRef<Call>(node)];
      Array<Expr> new_args;
      for (auto& arg : node->args) {
        new_args.push_back(VisitExpr(arg));
      }
      auto ret_type = node->checked_type();
      auto out_types = tvm::relay::FlattenTupleType(ret_type);
      if (tvm::relay::IsDynamic(ret_type)) {
        LOG(FATAL) << "Dynamic type not supported.";
        return Expr();
      } else {
        std::vector<Expr> outs;
        auto it = inplace_.var_share_map.find(bind_var);
        if (it != inplace_.var_share_map.end()) {
          // some outputs have inplace update
          auto share = it->second;
          CHECK_EQ(share.size(), out_types.size());
          for (size_t i = 0; i < out_types.size(); ++i) {
            // check if the output shares the memory with input
            if (share[i].defined()) {
              outs.push_back(share[i]);
            } else {
              outs.push_back(MakeStaticAllocation(&scope, out_types[i].as<TensorTypeNode>()));
            }
          }
        } else {
          for (size_t i = 0; i < out_types.size(); i++) {
            outs.push_back(MakeStaticAllocation(&scope, out_types[i].as<TensorTypeNode>()));
          }
        }
        auto invoke = Call(Op::Get("mnm.op.vm.invoke_op"),
                           Array<Expr>{scope.Push(node->op), scope.Push(Tuple(new_args)),
                                       scope.Push(Tuple(Array<Expr>(outs)))});
        scope.Push(invoke);
        return tvm::relay::ToTupleType(ret_type, outs);
      }
    } else {
      return ExprMutator::VisitExpr_(node);
    }
  }

  Expr VisitExpr_(const FunctionNode* node) {
    if (node->HasNonzeroAttr(attr::kPrimitive)) {
      return GetRef<Expr>(node);
    } else {
      return ExprMutator::VisitExpr_(node);
    }
  }

  Expr operator()(const Expr& expr) {
    inplace_.VisitExpr(expr);
    return Mutate(expr);
  }

 private:
  Expr ComputeAlignment(DataType dtype) {
    int64_t align = dtype.bits() / 8 * dtype.lanes();
    if (align < 64) {
      align = 64;
    }
    return MakeConstant(ScalarValue::make(align));
  }

  Expr ComputeStorage(const TensorTypeNode* type) {
    int64_t size = 1;
    for (auto dim : type->shape) {
      auto dim_imm = dim.as<IntImmNode>();
      CHECK(dim_imm);
      size *= dim_imm->value;
    }
    size *= (type->dtype.bits() * type->dtype.lanes() + 7) / 8;
    return MakeConstant(ScalarValue::make(size));
  }

  Expr MakeAllocStorage(const Array<Expr>& args, int device_type, int device_id,
                        const tvm::runtime::DataType& dtype) {
    static const Op& op = Op::Get("mnm.op.vm.alloc_storage");
    Array<Expr> new_args = args;
    new_args.push_back(MakeConstant(ScalarValue::make(device_type)));
    new_args.push_back(MakeConstant(ScalarValue::make(device_id)));
    new_args.push_back(MakeConstant(StringValue::make(DLDataType2String(dtype))));
    return Call(op, new_args);
  }

  Expr MakeAllocTensor(const Array<Expr>& args, const Expr& assert_shape,
                       const tvm::runtime::DataType& dtype) {
    static const Op& op = Op::Get("mnm.op.vm.alloc_tensor");
    Array<Expr> new_args = args;
    new_args.push_back(MakeConstant(StringValue::make(DLDataType2String(dtype))));
    new_args.push_back(assert_shape);
    return Call(op, new_args);
  }

  Expr MakeStaticAllocation(LetList* scope, const TensorTypeNode* type) {
    Expr shape = MakeConstant(type->shape);
    Expr size = ComputeStorage(type);
    Expr alignment = ComputeAlignment(type->dtype);
    auto alloc_storage_attrs = make_object<tvm::relay::AllocStorageAttrs>();
    auto dtype = type->dtype;
    auto target = tvm::Target::Current();
    auto device_type = target.defined() ? target->kind->device_type : kDLCPU;
    int device_id = 0;
    auto storage = scope->Push(MakeAllocStorage(Array<Expr>{size, alignment},
                                                static_cast<int>(device_type), device_id, dtype));
    auto tensor = scope->Push(MakeAllocTensor(Array<Expr>{storage, shape}, shape, dtype));
    return tensor;
  }

  /*! \brief The scope stack of the let list. */
  std::vector<LetList> scopes_;
  /*! \brief The mapping from expr to let bound var. */
  std::unordered_map<Expr, Var, ObjectPtrHash, ObjectPtrEqual> let_binding_;
  /*! \breif Inplace visitor to check the may_share information. */
  InplaceVisitor inplace_;
};

}  // namespace manifest_alloc

Pass ManifestAlloc() {
  runtime::TypedPackedFunc<Function(Function, IRModule, PassContext)> pass_func =
      [=](Function f, IRModule m, PassContext pc) {
        return Downcast<ir::Function>(manifest_alloc::ManifestAllocMutator()(f));
      };
  return CreateMNMFunctionPass(pass_func, 0, "ManifestAlloc", {});
}

MNM_REGISTER_GLOBAL("mnm.pass_.ManifestAlloc").set_body_typed(ManifestAlloc);

}  // namespace pass
}  // namespace mnm
