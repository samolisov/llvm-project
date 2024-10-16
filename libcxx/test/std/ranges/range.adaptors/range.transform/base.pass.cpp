//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: libcpp-no-concepts
// UNSUPPORTED: libcpp-has-no-incomplete-ranges

// constexpr V base() const& requires copy_constructible<V>
// constexpr V base() &&

#include <ranges>

#include "test_macros.h"
#include "types.h"

constexpr bool test() {
  {
    std::ranges::transform_view<ContiguousView, Increment> transformView;
    ContiguousView base = std::move(transformView).base();
    ASSERT_SAME_TYPE(ContiguousView, decltype(std::move(transformView).base()));
    assert(std::ranges::begin(base) == globalBuff);
  }

  {
    std::ranges::transform_view<CopyableView, Increment> transformView;
    CopyableView base1 = transformView.base();
    ASSERT_SAME_TYPE(CopyableView, decltype(transformView.base()));
    assert(std::ranges::begin(base1) == globalBuff);

    CopyableView base2 = std::move(transformView).base();
    ASSERT_SAME_TYPE(CopyableView, decltype(std::move(transformView).base()));
    assert(std::ranges::begin(base2) == globalBuff);
  }

  {
    const std::ranges::transform_view<CopyableView, Increment> transformView;
    const CopyableView base1 = transformView.base();
    ASSERT_SAME_TYPE(CopyableView, decltype(transformView.base()));
    assert(std::ranges::begin(base1) == globalBuff);

    const CopyableView base2 = std::move(transformView).base();
    ASSERT_SAME_TYPE(CopyableView, decltype(std::move(transformView).base()));
    assert(std::ranges::begin(base2) == globalBuff);
  }

  return true;
}

int main(int, char**) {
  test();
  static_assert(test());

  return 0;
}
