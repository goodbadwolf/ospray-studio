// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

struct FileUtils
{
  static bool MakeDirectory(const std::string &path, bool createParent = true);
};