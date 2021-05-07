import pytest
import mnm
from mnm.testing import run_infer_type, randn
import tvm

def test_canonicalize_ops_bias_add_ir():
    x, _ = randn((3, 2, 2))
    bias = mnm.array([1, 2], dtype="float32")
    # pylint: disable=protected-access
    class ModelWithBiasAdd(mnm.Model):
        # pylint: disable=attribute-defined-outside-init
        def build(self, bias):
            self.bias = bias

        @mnm.model.trace
        def forward(self, x):  # pylint: disable=no-self-use
            return mnm.bias_add(x, self.bias)

    def expected():
        x_var = tvm.relay.var('x', tvm.relay.TensorType(x.shape))
        bias_var = tvm.relay.var('bias', tvm.relay.TensorType(bias.shape, dtype="float32"))
        expand_dims_op = mnm._ffi.op.GetOp('mnm.op.expand_dims')
        axis_var = mnm.ir.const(1)
        new_axis_var = mnm.ir.const(1)
        null = mnm.ir.const(None)
        expand_dim = tvm.relay.Call(expand_dims_op, [bias_var, axis_var, new_axis_var])
        add_op = mnm._ffi.op.GetOp('mnm.op.add')
        var_tmp = tvm.relay.var('exp_bias_tmp')
        add = tvm.relay.Call(add_op, [x_var, var_tmp, null, null])
        body = tvm.relay.var('a1')
        body = tvm.relay.Let(body, add, body)
        body = tvm.relay.Let(var_tmp, expand_dim, body)
        return tvm.relay.Function([x_var, bias_var], body)

    model_before = ModelWithBiasAdd(bias)
    # infer type
    mod = mnm._ffi.pass_.InferType()(model_before._internal(x).mod)
    # canonicalize ops
    mod = mnm._ffi.pass_.CanonicalizeOps()(mod)
    func_canonicalized = mnm._ffi.pass_.InferType()(mod)["main"]
    # expected
    func_expected = run_infer_type(expected())
    assert tvm.ir.structural_equal(func_canonicalized, func_expected)

if __name__ == "__main__":
    pytest.main([__file__])
