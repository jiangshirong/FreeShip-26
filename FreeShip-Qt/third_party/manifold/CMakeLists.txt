# Serial 3-D kernel only. All upstream source files are pinned in origin.json.
add_library(print_manifold STATIC
    src/boolean3.cpp src/boolean_result.cpp src/constructors.cpp src/csg_tree.cpp
    src/edge_op.cpp src/execution_impl.cpp src/face_op.cpp src/impl.cpp
    src/manifold.cpp src/minkowski.cpp src/polygon.cpp src/properties.cpp
    src/quickhull.cpp src/sdf.cpp src/smoothing.cpp src/sort.cpp
    src/subdivision.cpp src/tree2d.cpp)
target_include_directories(print_manifold PUBLIC include)
target_compile_features(print_manifold PUBLIC cxx_std_17)
target_compile_definitions(print_manifold PUBLIC MANIFOLD_PAR=-1)
