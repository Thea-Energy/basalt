#pragma once

#include "spdlog/common.h"
#include "spdlog/spdlog.h"
void init();

void terminate();

bool sms_is_initialized();

template <typename... Args>
void throw_exception(spdlog::format_string_t<Args...> fmt, Args &&...args) {
  spdlog::error(fmt, std::forward<Args>(args)...);
  std::string fmt_string =
      spdlog::fmt_lib::format(fmt, std::forward<Args>(args)...);
  throw std::runtime_error(fmt_string);
}