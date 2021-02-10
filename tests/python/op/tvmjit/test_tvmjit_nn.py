# pylint: disable=too-many-locals
import numpy as np
import pytest
import torch
import torch.nn.functional as F
import mnm
from mnm.testing import randn, get_device_list, randn_torch, with_seed, check, run_vm_model
from mnm.model.trace import trace_mutate_attr


@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("b", [1, 2, 4])
@pytest.mark.parametrize("n", [1, 2, 4])
@pytest.mark.parametrize("m", [1, 2, 4])
@pytest.mark.parametrize("k", [1, 2, 4])
def test_batch_matmul(b, n, m, k, device):
    class BatchMatmul(mnm.Model):
        def build(self):
            pass
        @mnm.model.trace
        def forward(self, m_a, m_b):  # pylint: disable=no-self-use
            return mnm.batch_matmul(m_a, m_b)
    # check forward
    model = BatchMatmul()
    m_a, n_a = randn((b, m, k), device=device)
    m_b, n_b = randn((b, n, k), device=device)
    m_a.requires_grad = True
    m_b.requires_grad = True
    m_c = model(m_a, m_b)
    v_c = run_vm_model(model, device, [m_a, m_b])
    n_c = np.matmul(n_a, np.transpose(n_b, (0, 2, 1)))
    check(m_c, n_c)
    check(v_c, n_c)
    # check backward
    m_dy, n_dy = randn(m_c.shape, device=device)
    m_c.backward(m_dy)
    n_dyt = np.transpose(n_dy, (0, 2, 1))
    check(m_a.grad, np.matmul(n_dy, n_b))
    check(m_b.grad, np.matmul(n_dyt, n_a))


@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("n", [1, 2, 4])
@pytest.mark.parametrize("m", [1, 2, 4])
@pytest.mark.parametrize("k", [1, 2, 4])
def test_dense(n, m, k, device):
    # pylint: disable=no-member
    class Dense(mnm.Model):
        def build(self):
            pass
        @mnm.model.trace
        def forward(self, m_a, m_b):  # pylint: disable=no-self-use
            return mnm.dense(m_a, m_b)
    # check forward
    model = Dense()
    m_a, n_a = randn((m, k), device=device)
    m_b, n_b = randn((n, k), device=device)
    m_a.requires_grad = True
    m_b.requires_grad = True
    m_c = model(m_a, m_b)
    v_c = run_vm_model(model, device, [m_a, m_b])
    n_c = np.matmul(n_a, np.transpose(n_b))
    check(m_c, n_c)
    check(v_c, n_c)
    # check backward
    m_dy, n_dy = randn(m_c.shape, device=device)
    m_c.backward(m_dy)
    n_dyt = np.transpose(n_dy, (1, 0))
    check(m_a.grad, np.matmul(n_dy, n_b))
    check(m_b.grad, np.matmul(n_dyt, n_a))


# pylint: disable=no-member
# pylint: disable=no-self-use
# pylint: disable=protected-access
@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("dtype", ["float32", "float64"])
@pytest.mark.parametrize("shape", [
    [3],
    [3, 2],
    [3, 2, 5],
    [3, 2, 5, 8],
    [3, 2, 5, 8, 4],
    [3, 2, 5, 8, 4, 7],
])
@pytest.mark.parametrize("axis", range(-8, 8))
@pytest.mark.parametrize(
    "funcs",
    [
        [mnm._op.sym.softmax, torch.softmax],
    ])
def test_unary_with_axis(device, dtype, shape, axis, funcs):
    mnm_fwd, torch_fwd = funcs

    class TestModel(mnm.Model):
        def build(self):
            pass
        @mnm.model.trace
        def forward(self, x):
            return mnm_fwd(x, axis=axis)

    model = TestModel()
    # forward
    m_x, t_x = randn_torch(shape, device=device, dtype=dtype, requires_grad=True)
    if not -len(shape) <= axis < len(shape):
        with pytest.raises(ValueError):
            m_y = model(m_x)
        return
    m_y = model(m_x)
    v_y = run_vm_model(model, device, [m_x])
    t_y = torch_fwd(t_x, dim=axis)
    check(m_y, t_y)
    check(v_y, t_y)
    # backward
    m_dy, t_dy = randn_torch(shape, device=device, dtype=dtype)
    t_y.backward(t_dy)
    m_y.backward(m_dy)
    check(m_x.grad, t_x.grad)


# pylint: disable=no-member
# pylint: disable=no-self-use
# pylint: disable=protected-access
@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("dtype", ["float32", "float64"])
@pytest.mark.parametrize("shape", [
    [3, 2],
    [1, 3]
])
def test_log_softmax(device, dtype, shape):
    class TestModel(mnm.Model):
        def build(self):
            pass
        @mnm.model.trace
        def forward(self, x):
            return mnm._op.sym.log_softmax(x)

    model = TestModel()
    # forward
    m_x, t_x = randn_torch(shape, device=device, dtype=dtype, requires_grad=True)
    m_y = model(m_x)
    v_y = run_vm_model(model, device, [m_x])
    t_y = torch.log_softmax(t_x, dim=-1)
    check(m_y, t_y)
    check(v_y, t_y)
    # backward
    m_dy, t_dy = randn_torch(shape, device=device, dtype=dtype)
    t_y.backward(t_dy)
    m_y.backward(m_dy)
    check(m_x.grad, t_x.grad)


@with_seed(0)
@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("shape", [
    (5, 4, 6, 9),
    (6, 5, 7, 10),
    (12, 32, 6, 8),
    (3, 7, 9)
])
@pytest.mark.parametrize("axis", [0, 1, 2, -1])
@pytest.mark.parametrize("eps", [1e-05, 2e-05])
@pytest.mark.parametrize("dtype", ["float32", "float64"])
def test_layer_norm(device, shape, axis, eps, dtype):
    # pylint: disable=import-outside-toplevel
    # pylint: disable=attribute-defined-outside-init
    import mxnet as mx
    class LayerNorm(mnm.Model):
        def build(self, axis, eps):
            self._axis = axis
            self._eps = eps

        @mnm.model.trace
        def forward(self, x):
            return mnm.layer_norm(x, axis=self._axis, eps=self._eps)
    m_model = LayerNorm(axis, eps)
    m_model.to(device=device, dtype=dtype)
    mx_model = mx.gluon.nn.LayerNorm(axis=axis, epsilon=eps, center=False, scale=False)
    mx_model.initialize(ctx=mx.cpu(0))
    m_x, n_x = randn(shape, device=device, dtype=dtype)
    mx_x = mx.nd.array(n_x)
    m_x.requires_grad = True
    mx_x.attach_grad()
    # check forward
    m_y = m_model(m_x)
    v_y = run_vm_model(m_model, device, [m_x])
    m_dy, n_dy = randn(m_y.shape, device=device, dtype=dtype)
    mx_dy = mx.nd.array(n_dy)
    with mx.autograd.record():
        mx_y = mx_model(mx_x)
        mx_y.backward(mx_dy)
    check(m_y, mx_y.asnumpy())
    check(v_y, mx_y.asnumpy())
    # check backward
    m_y.backward(m_dy)
    check(m_x.grad, mx_x.grad.asnumpy(), rtol=1e-4, atol=1e-4)


@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("dtype", ["float32", "float64"])
@pytest.mark.parametrize("shapes", [
    ((4, 256, 32, 32), (64, 256, 1, 1)),
    ((8, 3, 32, 32), (16, 3, 3, 3)),
])
@pytest.mark.parametrize("stride", [1, 2, 3, 4])
@pytest.mark.parametrize("dilation", [1])
@pytest.mark.parametrize("padding", [0, 1, 2])
def test_conv2d(device, dtype, shapes, stride, dilation, padding):
    # pylint: disable=too-many-arguments
    # N.B.: NCHW + OIHW
    # forward
    class Conv2D(mnm.Model):
        def build(self):
            pass
        @mnm.model.trace
        def forward(self, x, w):  # pylint: disable=no-self-use
            return mnm.conv2d(x, w, stride=stride, padding=padding, dilation=dilation, groups=1)

    model = Conv2D()
    # forward
    xshape, wshape = shapes
    m_x, t_x = randn_torch(xshape, std=0.001, device=device, dtype=dtype, requires_grad=True)
    m_w, t_w = randn_torch(wshape, std=0.01, device=device, dtype=dtype, requires_grad=True)
    m_y = model(m_x, m_w)
    v_y = run_vm_model(model, device, [m_x, m_w])
    t_y = F.conv2d(t_x, t_w, stride=stride, dilation=dilation, padding=padding)
    check(m_y, t_y, rtol=1e-4, atol=1e-4)
    check(v_y, t_y, rtol=1e-4, atol=1e-4)
    # backward
    m_dy, t_dy = randn_torch(t_y.shape, device=device, dtype=dtype)
    m_y.backward(m_dy)
    t_y.backward(t_dy)
    check(m_x.grad, t_x.grad, rtol=1e-4, atol=1e-4)
    check(m_w.grad, t_w.grad, rtol=1e-4, atol=1e-4)


@pytest.mark.parametrize("device", ["cpu"])
@pytest.mark.parametrize("dtype", ["float32", "float64"])
@pytest.mark.parametrize("xshape", [(8, 3, 32, 32)])
@pytest.mark.parametrize("wshape", [(16, 3, 3, 3)])
@pytest.mark.parametrize("stride", [1, 2])
@pytest.mark.parametrize("dilation", [1])
@pytest.mark.parametrize("padding", [0, 1, 2])
def test_conv2d_nhwc(device, dtype, xshape, wshape, stride, dilation, padding):
    # pylint: disable=too-many-locals
    # pylint: disable=too-many-arguments
    # N.B.: NHWC + HWIO
    # forward
    class Conv2D(mnm.Model):
        def build(self):
            pass

        @mnm.model.trace
        def forward(self, x, w):  # pylint: disable=no-self-use
            x = mnm.transpose(x, (0, 2, 3, 1))  # NCHW -> NHWC
            w = mnm.transpose(w, (2, 3, 1, 0))  # OIHW -> HWIO
            conv = mnm.conv2d(x, w, stride=stride, padding=padding, dilation=dilation, groups=1,
                              layout="NHWC", kernel_layout="HWIO", out_layout="NHWC")
            # NHWC -> NCHW
            return mnm.transpose(conv, (0, 3, 1, 2))

    model = Conv2D()
    m_x, t_x = randn_torch(xshape, std=0.001, device=device, dtype=dtype)
    m_w, t_w = randn_torch(wshape, std=0.01, device=device, dtype=dtype)
    # forward only for NHWC
    m_y = model(m_x, m_w)
    t_y = F.conv2d(t_x, t_w, stride=stride, dilation=dilation, padding=padding)
    check(m_y, t_y, rtol=1e-4, atol=1e-4)


@pytest.mark.parametrize("device", ["cpu"])
@pytest.mark.parametrize("dtype", ["float32", "float64"])
@pytest.mark.parametrize("data_shape", [(8, 3, 32, 32)])
@pytest.mark.parametrize("kernel", [1, 2, 3, 4])
@pytest.mark.parametrize("stride", [1, 2, 3, 4])
@pytest.mark.parametrize("padding", [0, 1])
@pytest.mark.parametrize(
    "funcs",
    [
        [mnm._op.sym.max_pool2d, torch.nn.functional.max_pool2d],
        [mnm._op.sym.avg_pool2d, torch.nn.functional.avg_pool2d],
    ])
def test_pool2d(device, dtype, data_shape, kernel, stride, padding, funcs):
    # TODO(@XIAO-XIA): complement test case when device=cuda
    # pylint: disable=too-many-arguments
    mnm_fwd, torch_fwd = funcs
    if padding > kernel // 2:
        return

    class TestModel(mnm.Model):
        def build(self):
            pass
        @mnm.model.trace
        def forward(self, x):  # pylint: disable=no-self-use
            return mnm_fwd(x, kernel=kernel, stride=stride, padding=padding)

    model = TestModel()
    # forward
    m_x, t_x = randn_torch(data_shape, dtype=dtype, device=device, requires_grad=True)
    m_y = model(m_x)
    v_y = run_vm_model(model, device, [m_x])
    t_y = torch_fwd(t_x, kernel_size=kernel, stride=stride, padding=padding)
    check(m_y, t_y)
    check(v_y, t_y)
    # backward
    m_dy, t_dy = randn_torch(m_y.shape, dtype=dtype, device=device)
    m_y.backward(m_dy)
    t_y.backward(t_dy)
    check(m_x.grad, t_x.grad)


@pytest.mark.parametrize("device", ["cpu"])
@pytest.mark.parametrize("dtype", ["float32", "float64"])
@pytest.mark.parametrize("data_shape", [(8, 3, 32, 32)])
@pytest.mark.parametrize("kernel", [1, 2, 3, 4])
@pytest.mark.parametrize("stride", [1, 2, 3, 4])
@pytest.mark.parametrize("padding", [0, 1])
@pytest.mark.parametrize(
    "funcs",
    [
        [mnm._op.sym.max_pool2d, torch.nn.functional.max_pool2d],
        [mnm._op.sym.avg_pool2d, torch.nn.functional.avg_pool2d],
    ])
def test_pool2d_nhwc(device, dtype, data_shape, kernel, stride, padding, funcs):
    # TODO(yzhliu): complement test case when device=cuda
    # pylint: disable=too-many-locals, too-many-arguments
    mnm_fwd, torch_fwd = funcs
    if padding > kernel // 2:
        return

    class TestModel(mnm.Model):
        def build(self):
            pass

        @mnm.model.trace
        def forward(self, x):  # pylint: disable=no-self-use
            x = mnm.transpose(x, (0, 2, 3, 1))  # NCHW -> NHWC
            pool = mnm_fwd(x, kernel=kernel, stride=stride, padding=padding, layout="NHWC")
            # NHWC -> NCHW
            return mnm.transpose(pool, (0, 3, 1, 2))

    model = TestModel()
    m_x, t_x = randn_torch(data_shape, dtype=dtype, device=device, requires_grad=False)
    # forward only for NHWC layout
    m_y = model(m_x)
    t_y = torch_fwd(t_x, kernel_size=kernel, stride=stride, padding=padding)
    check(m_y, t_y)


@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("dtype", ["float32", "float64"])
@pytest.mark.parametrize("n", [1, 2, 4])
@pytest.mark.parametrize("m", [1, 2, 4])
@pytest.mark.parametrize("k", [1, 2, 4])
@pytest.mark.parametrize("transpose_a", [True, False])
@pytest.mark.parametrize("transpose_b", [True, False])
def test_matmul(device, dtype, n, k, m, transpose_a, transpose_b):
    # pylint: disable=too-many-arguments
    class TestModel(mnm.Model):
        def build(self):
            pass
        @mnm.model.trace
        def forward(self, m_a, m_b):  # pylint: disable=no-self-use
            mnm_op = [[mnm.matmul, mnm.matmul_nt],
                      [mnm.matmul_tn, mnm.matmul_tt]]
            mnm_op = mnm_op[transpose_a][transpose_b]
            return mnm_op(m_a, m_b)
    # forward
    model = TestModel()
    m_a, t_a = randn_torch((n, k) if not transpose_a else (k, n),
                           device=device, dtype=dtype, requires_grad=True)
    m_b, t_b = randn_torch((k, m) if not transpose_b else (m, k),
                           device=device, dtype=dtype, requires_grad=True)
    m_c = model(m_a, m_b)
    v_c = run_vm_model(model, device, [m_a, m_b])
    t_c = torch.matmul(t_a.T if transpose_a else t_a, t_b.T if transpose_b else t_b) # pylint: disable=no-member
    check(m_c, t_c, rtol=1e-4, atol=1e-4)
    check(v_c, t_c, rtol=1e-4, atol=1e-4)
    # backward
    m_dc, t_dc = randn_torch(m_c.shape, device=device, dtype=dtype)
    m_c.backward(m_dc)
    t_c.backward(t_dc)
    check(m_a.grad, t_a.grad, rtol=1e-4, atol=1e-4)
    check(m_b.grad, t_b.grad, rtol=1e-4, atol=1e-4)


@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("shape", [[8, 8, 8, 8], [8, 8, 8, 8, 8]])
@pytest.mark.parametrize("momentum", [0.1, 0.2, 0.3, 0.4])
@pytest.mark.parametrize("eps", [1e-3, 1e-4, 1e-5, 1e-6])
def test_mnm_batch_norm_infer(shape, momentum, eps, device):
    stats_shape = [shape[1]]
    m_x, t_x = randn_torch(shape, device=device)
    m_m, t_m = randn_torch(stats_shape, device=device)
    m_v, t_v = randn_torch(stats_shape, device=device, positive=True)
    m_w, t_w = randn_torch(stats_shape, device=device)
    m_b, t_b = randn_torch(stats_shape, device=device)

    class TestModel(mnm.Model):
        def build(self):
            pass
        @mnm.model.trace
        def forward(self, m_x, m_m, m_v, m_w, m_b):  # pylint: disable=no-self-use,too-many-arguments
            return mnm.batch_norm_infer(m_x, m_m, m_v, m_w, m_b, momentum, eps)

    model = TestModel()
    m_y = model(m_x, m_m, m_v, m_w, m_b)
    v_y = run_vm_model(model, device, [m_x, m_m, m_v, m_w, m_b])
    t_y = F.batch_norm(t_x, t_m, t_v, t_w, t_b, False, momentum, eps)
    check(m_y, t_y, rtol=1e-4, atol=1e-4)
    check(v_y, t_y, rtol=1e-4, atol=1e-4)


@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("shape", [[8, 8, 8, 8], [8, 8, 8, 8, 8]])
@pytest.mark.parametrize("momentum", [0.1, 0.2, 0.3, 0.4])
@pytest.mark.parametrize("eps", [1e-3, 1e-4, 1e-5, 1e-6])
def test_mnm_batch_norm_train(shape, momentum, eps, device):
    # pylint: disable=attribute-defined-outside-init
    stats_shape = [shape[1]]
    m_x, t_x = randn_torch(shape, device=device, requires_grad=True)
    m_m, t_m = randn_torch(stats_shape, device=device)
    m_v, t_v = randn_torch(stats_shape, device=device, positive=True)
    m_w, t_w = randn_torch(stats_shape, device=device, requires_grad=True)
    m_b, t_b = randn_torch(stats_shape, device=device, requires_grad=True)
    np_m = m_m.asnumpy()
    np_v = m_v.asnumpy()

    class TestModel(mnm.Model):
        def build(self, m_m, m_v):
            self.m_m = m_m
            self.m_v = m_v

        @mnm.model.trace
        def forward(self, m_x, m_w, m_b):  # pylint: disable=no-self-use,too-many-arguments
            result = mnm.batch_norm_train(m_x, self.m_m, self.m_v, m_w, m_b, momentum, eps)
            trace_mutate_attr(self, "m_m", result[1])
            trace_mutate_attr(self, "m_v", result[2])
            return result[0]

    # forward
    model = TestModel(m_m, m_v)
    m_y = model(m_x, m_w, m_b)
    t_y = F.batch_norm(t_x, t_m, t_v, t_w, t_b, True, momentum, eps)
    check(m_y, t_y, rtol=1e-4, atol=1e-4)
    check(m_m, t_m, rtol=1e-4, atol=1e-4)
    check(m_v, t_v, rtol=1e-4, atol=1e-4)
    # forward vm
    model.m_m = mnm.array(np_m, device=device)
    model.m_v = mnm.array(np_v, device=device)
    v_y = run_vm_model(model, device, [m_x, m_w, m_b])[0]
    check(v_y, t_y, rtol=1e-4, atol=1e-4)
    check(model.m_m, t_m, rtol=1e-4, atol=1e-4)
    check(model.m_v, t_v, rtol=1e-4, atol=1e-4)
    # backward
    m_dy, t_dy = randn_torch(shape, device=device)
    m_y.backward(m_dy)
    t_y.backward(t_dy)
    check(m_x.grad, t_x.grad, rtol=1e-4, atol=1e-4)
    check(m_w.grad, t_w.grad, rtol=1e-4, atol=1e-4)
    check(m_b.grad, t_b.grad, rtol=1e-4, atol=1e-4)


@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("dtype", ["float32", "float64"])
@pytest.mark.parametrize("dimension", [
    ((2, 3), (1, 1, 1, 1)),
])
@pytest.mark.parametrize("pad_value", [0, 2])
@pytest.mark.parametrize("pad_mode", ["constant"])
def test_pad(device, dtype, dimension, pad_value, pad_mode):
    shape, pad_width = dimension

    # pylint: disable=too-many-locals
    # pylint: disable=too-many-arguments
    class TestModel(mnm.Model):
        def build(self):
            pass

        @mnm.model.trace
        def forward(self, m_x):  # pylint: disable=no-self-use
            return mnm.pad(m_x, pad_width, pad_value, pad_mode)

    m_x, t_x = randn_torch(shape, device=device, dtype=dtype)
    model = TestModel()
    m_y = model(m_x)
    t_y = torch.nn.functional.pad(t_x, pad_width, pad_mode, pad_value)
    check(m_y, t_y)


if __name__ == "__main__":
    pytest.main([__file__])
