/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cassert>
#include <iostream>

#include <gtest/gtest.h>

#include <mnm/device.h>
#include <mnm/op.h>

using mnm::DevType;
using mnm::ir::Array;
using mnm::ir::Attrs;
using mnm::ir::Op;
using mnm::op::CallValues;
using mnm::op::GetDialect;
using mnm::op::IsDialectOp;
using mnm::op::OpDialect;
using mnm::op::OpEnv;
using mnm::op::OpEnvMaker;
using mnm::value::Value;

MNM_REGISTER_OP("mnm.op.cpptest.conv2d");

class Conv2d : public OpEnv {
 public:
  int type;
  Conv2d() = default;
  virtual ~Conv2d() = default;
  std::string name() const override {
    return "cpptest.conv2d";
  }
  void Execute(const CallValues& call) override final {
  }
  void Execute(const std::vector<Value>& inputs, Value output) override final {
  }
};

// Implement 0 of "mnm.cpptest.conv2d"
class Conv2dX : public Conv2d {
 public:
  Conv2dX() {
    type = 0;
  }
  virtual ~Conv2dX() = default;
  static OpEnv* make(const CallValues& call) {
    return new Conv2dX();
  }
};
MNM_REGISTER_DIALECT("mklShallowNN").set_enable(DevType::kCPU());
MNM_REGISTER_DIALECT_OP(mklShallowNN, cpptest.conv2d, 10);
MNM_OP_ENV_MAKER("mnm.op.mklShallowNN.cpptest.conv2d", Conv2dX::make);

// Implement 1 of "mnm.cpptest.conv2d"
class Conv2dY : public Conv2d {
 public:
  Conv2dY() {
    type = 1;
  }
  virtual ~Conv2dY() = default;
  static OpEnv* make(const CallValues& call) {
    return new Conv2dY();
  }
};
MNM_REGISTER_DIALECT("sshadow").set_enable(DevType::kCPU());
MNM_REGISTER_DIALECT_OP(sshadow, cpptest.conv2d, 12);
MNM_OP_ENV_MAKER("mnm.op.sshadow.cpptest.conv2d", Conv2dY::make);

TEST(OpDialect, Registry) {
  auto dispatch_list =
      OpDialect::GetDispatchList(Op::Get("mnm.op.cpptest.conv2d"), DevType::kCPU());
  ASSERT_EQ(dispatch_list.size(), 2);
  CallValues call;
  for (const auto e : dispatch_list) {
    auto dialect_op = Op::Get(e.dialect_op);
    ASSERT_TRUE(dialect_op.defined());
    ASSERT_TRUE(IsDialectOp(dialect_op));
    ASSERT_EQ(GetDialect(dialect_op), e.dialect);
    auto maker = OpEnvMaker::Get(e.dialect_op);
    ASSERT_NE(maker, nullptr);
    const auto* env = static_cast<Conv2d*>((*maker)(call));
    ASSERT_NE(env, nullptr);
    if (e.dialect == "mklShallowNN") {
      ASSERT_EQ(dialect_op->name, "mnm.op.mklShallowNN.cpptest.conv2d");
      ASSERT_EQ(env->type, 0);
    } else if (e.dialect == "sshadow") {
      ASSERT_EQ(dialect_op->name, "mnm.op.sshadow.cpptest.conv2d");
      ASSERT_EQ(env->type, 1);
    } else {
      ASSERT_TRUE(false);
    }
    delete env;
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
