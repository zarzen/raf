/*!
 * Copyright (c) 2019 by Contributors
 * \file ir.h
 * \brief A compatibility layer between MNM and TVM/Relay IR.
 */
#pragma once
#include <string>
#include "tvm/runtime/object.h"
#include "tvm/runtime/data_type.h"
#include "tvm/node/container.h"
#include "tvm/node/node.h"
#include "tvm/relay/base.h"
#include "tvm/relay/expr.h"
#include "tvm/relay/expr_functor.h"
#include "tvm/relay/op.h"
#include "tvm/relay/op_attr_types.h"
#include "tvm/relay/type.h"
#include "tvm/ir/op.h"

namespace mnm {
namespace ir {

// Containers
using tvm::Array;
using tvm::ArrayNode;
using tvm::Map;
using tvm::MapNode;
using tvm::String;

// Scalars
using tvm::Integer;
using tvm::IntImm;
using tvm::IntImmNode;

// Attributes
using tvm::Attrs;
using tvm::AttrsNode;

using tvm::BaseTensorType;
using tvm::BaseTensorTypeNode;
using tvm::Type;
using tvm::TypeNode;

// Object protocol
using tvm::NullValue;
using tvm::runtime::DataType;
using tvm::runtime::Downcast;
using tvm::runtime::GetObjectPtr;
using tvm::runtime::GetRef;
using tvm::runtime::make_object;
using tvm::runtime::NDArray;
using tvm::runtime::Object;
using tvm::runtime::ObjectPtr;
using tvm::runtime::ObjectPtrEqual;
using tvm::runtime::ObjectPtrHash;
using tvm::runtime::ObjectRef;
using tvm::runtime::TypeIndex;

// Relay Expression
using tvm::relay::Expr;
using tvm::relay::ExprNode;

using tvm::relay::Id;
using tvm::relay::IdNode;

using tvm::Op;
using tvm::OpNode;

using tvm::relay::Tuple;
using tvm::relay::TupleNode;

using tvm::relay::Var;
using tvm::relay::VarNode;

using tvm::relay::GlobalVar;
using tvm::relay::GlobalVarNode;

using tvm::relay::Function;
using tvm::relay::FunctionNode;

using tvm::relay::Call;
using tvm::relay::CallNode;

using tvm::relay::Let;
using tvm::relay::LetNode;

using tvm::relay::If;
using tvm::relay::IfNode;

using tvm::relay::TupleGetItem;
using tvm::relay::TupleGetItemNode;

using tvm::relay::RefCreate;
using tvm::relay::RefCreateNode;

using tvm::relay::RefRead;
using tvm::relay::RefReadNode;

using tvm::relay::RefWrite;
using tvm::relay::RefWriteNode;

using tvm::relay::TempExpr;
using tvm::relay::TempExprNode;

// Relay Types
using tvm::relay::Any;
using tvm::relay::AnyNode;
using tvm::relay::Kind;

using tvm::relay::Type;
using tvm::relay::TypeNode;

using tvm::relay::TensorType;
using tvm::relay::TensorTypeNode;

using tvm::relay::TypeVar;
using tvm::relay::TypeVarNode;

using tvm::relay::GlobalTypeVar;
using tvm::relay::GlobalTypeVarNode;

using tvm::relay::TypeCall;
using tvm::relay::TypeCallNode;

using tvm::relay::IncompleteType;
using tvm::relay::IncompleteTypeNode;

using tvm::relay::FuncType;
using tvm::relay::FuncTypeNode;

using tvm::relay::TupleType;
using tvm::relay::TupleTypeNode;

using tvm::relay::TypeConstraint;
using tvm::relay::TypeConstraintNode;

using tvm::relay::TypeRelation;
using tvm::relay::TypeRelationNode;

using tvm::relay::TypeReporter;

// Relay Functors
using tvm::relay::ExprFunctor;
using tvm::relay::ExprMutator;
using tvm::relay::ExprVisitor;

}  // namespace ir
}  // namespace mnm

#define MNM_BASE_OBJECT(TypeName, ParentType)                                                  \
  static const uint32_t RuntimeTypeIndex() {                                                   \
    if (TypeName::_type_index != ::tvm::runtime::TypeIndex::kDynamic) {                        \
      return TypeName::_type_index;                                                            \
    }                                                                                          \
    return _GetOrAllocRuntimeTypeIndex();                                                      \
  }                                                                                            \
  static const uint32_t _GetOrAllocRuntimeTypeIndex() {                                        \
    static uint32_t tidx = GetOrAllocRuntimeTypeIndex(                                         \
        TypeName::_type_key, TypeName::_type_index, ParentType::_GetOrAllocRuntimeTypeIndex(), \
        TypeName::_type_child_slots, TypeName::_type_child_slots_can_overflow);                \
    return tidx;                                                                               \
  }

#define MNM_FINAL_OBJECT(TypeName, ParentType)      \
  static const constexpr bool _type_final = true;   \
  static const constexpr int _type_child_slots = 0; \
  MNM_BASE_OBJECT(TypeName, ParentType)

#define MNM_OBJECT_REF(TypeName, ParentType, ObjectName)                                   \
  TypeName() {                                                                             \
  }                                                                                        \
  explicit TypeName(::tvm::runtime::ObjectPtr<::tvm::runtime::Object> n) : ParentType(n) { \
  }                                                                                        \
  ObjectName* operator->() const {                                                         \
    return static_cast<ObjectName*>(data_.get());                                          \
  }                                                                                        \
  using ContainerType = ObjectName;

#define MNM_REGISTER_OBJECT_NO_REFLECT(TypeName)                              \
  static DMLC_ATTRIBUTE_UNUSED uint32_t __make_Object_tidx##_##TypeName##__ = \
      TypeName::_GetOrAllocRuntimeTypeIndex()

#define MNM_REGISTER_OBJECT_REFLECT(TypeName)                                                    \
  MNM_REGISTER_OBJECT_NO_REFLECT(TypeName);                                                      \
  static DMLC_ATTRIBUTE_UNUSED ::tvm::ReflectionVTable::Registry& __make_Node##_##TypeName##__ = \
      ::tvm::ReflectionVTable::Global()                                                          \
          ->Register<TypeName, ::tvm::detail::ReflectionTrait<TypeName>>()                       \
          .set_creator(                                                                          \
              [](const std::string&) -> ::tvm::runtime::ObjectPtr<::tvm::runtime::Object> {      \
                return ::tvm::runtime::make_object<TypeName>();                                  \
              })

#include "./ir_ext.h"
