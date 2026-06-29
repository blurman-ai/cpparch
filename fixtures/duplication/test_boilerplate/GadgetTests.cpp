// Test fixture: GadgetTests.cpp
// Contains the same arrange-act-assert pattern as WidgetTests.cpp
// Should NOT be reported as a clone (both are test files).

#include <catch2/catch_test_macros.hpp>
#include "gadget.h"

TEST_CASE("Gadget initialization", "[gadget][unit]")
{
  // Arrange
  Gadget g;
  g.setSize(10);
  g.setColor(0xFF0000);

  // Act
  g.render();

  // Assert
  REQUIRE(g.isVisible() == true);
  REQUIRE(g.getSize() == 10);
  REQUIRE(g.getColor() == 0xFF0000);
}
