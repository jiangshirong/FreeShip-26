# Pinned subset of JKQtPlotter: LGPL-2.1-or-later, see LICENSE/UPSTREAM.json.
find_package(Qt6 REQUIRED COMPONENTS Widgets Xml)
file(GLOB_RECURSE mathtext_sources CONFIGURE_DEPENDS jkqtcommon/*.cpp jkqtcommon/*.h jkqtmathtext/*.cpp jkqtmathtext/*.h)
add_library(jkqt_mathtext STATIC ${mathtext_sources})
set_target_properties(jkqt_mathtext PROPERTIES AUTOMOC ON)
target_include_directories(jkqt_mathtext PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(jkqt_mathtext PUBLIC Qt6::Widgets Qt6::Xml)
