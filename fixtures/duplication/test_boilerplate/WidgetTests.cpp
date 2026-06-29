// Test fixture: WidgetTests.cpp
// Contains a typical unit test arrange-act-assert pattern that should NOT be reported
// as a clone when found in another test file.

#include <catch2/catch_test_macros.hpp>
#include "widget.h"

TEST_CASE("Widget initialization", "[widget][unit]")
{
  // Arrange
  Widget w;
  w.setSize(10);
  w.setColor(0xFF0000);

  // Act
  w.render();

  // Assert
  REQUIRE(w.isVisible() == true);
  REQUIRE(w.getSize() == 10);
  REQUIRE(w.getColor() == 0xFF0000);
}
