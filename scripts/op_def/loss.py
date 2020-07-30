# pylint: disable=too-few-public-methods,too-many-arguments,unused-argument,invalid-name
# pylint: disable=missing-class-docstring,missing-function-docstring
"""Loss function definition and their argument data structures."""
from .base import Op, Tensor


class LossArgs:

    __ops__ = [
        Op("nll_loss"),
        Op("nll_loss_dtrue"),
        Op("nll_loss_dpred"),
    ]

    @staticmethod
    def f(y_true: Tensor,
          y_pred: Tensor,
          ) -> Tensor:
        ...
