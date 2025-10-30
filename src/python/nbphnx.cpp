#include "init.h"

#include <nanobind/nanobind.h>

#include <nanobind/intrusive/counter.h>
#include <nanobind/intrusive/counter.inl> // include only here

#include <nanobind/operators.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/typing.h>

namespace nb = nanobind;
using namespace nb::literals;

#include "autogen/bind_phnx.h"

NB_MODULE(_core, m) {
  init();

  // Install the instrusive reference counters
  nb::intrusive_init(
      [](PyObject *o) noexcept {
        nb::gil_scoped_acquire guard;
        Py_INCREF(o);
      },
      [](PyObject *o) noexcept {
        nb::gil_scoped_acquire guard;
        Py_DECREF(o);
      });

  bind_phnx(m);

  // Register memory cleanup on interpreter exit
  const auto atexit = nb::module_::import_("atexit");
  atexit.attr("register")(nb::cpp_function(&terminate));
}