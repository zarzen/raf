import sys

from ._ffi import _tvm


def register_mnm_node(type_key=None):
    result = None
    if isinstance(type_key, type):
        result = _tvm._register_node(type_key.__name__)(type_key)
    elif isinstance(type_key, str):
        result = _tvm._register_node(type_key)
    else:
        raise ValueError("Unsupported type of type_key: ",
                         type(type_key).__name__)
    return result


def set_module(module):
    def decorator(func):
        if module is not None:
            func.__module__ = module
        return func
    return decorator


def import_to_module(module_name):
    def decorator(func):
        module = sys.modules[module_name]
        setattr(module, func.__name__, func)
    return decorator
