from .._ffi._tvm import _get_global_func, _make_node, _NodeBase
from .._ffi.op import MakeOutput
from .base import register_mnm_node


@register_mnm_node("mnm.op.OpInfo")
class OpInfo(_NodeBase):
    pass


def invoke_make_output(op_name, attrs_name, args, **attrs):
    if not isinstance(args, tuple):
        args = (args, )
    if attrs_name:
        attrs = _make_node(attrs_name, **attrs)
    else:
        attrs = None
    tmp = MakeOutput(op_name, args, attrs)
    return tmp.output


def _get_op_dict():
    f_list_name = _get_global_func("relay.op._ListOpNames")
    f_get_op = _get_global_func("relay.op._GetOp")
    op_dict = {
        name: f_get_op(name)
        for name in map(lambda x: x.value, f_list_name())
        if name.startswith("mnm.op.")
    }
    return op_dict


OP_DICT = _get_op_dict()
