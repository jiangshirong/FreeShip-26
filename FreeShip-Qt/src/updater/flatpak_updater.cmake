# Separate executable: the Debian package's helper uses a different protocol and payload.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network DBus)
add_executable(FreeShipFlatpakUpdater src/updater/flatpak_updater.cpp)
set_target_properties(FreeShipFlatpakUpdater PROPERTIES AUTOMOC ON)
target_link_libraries(FreeShipFlatpakUpdater PRIVATE Qt6::Core Qt6::Network Qt6::DBus)
add_dependencies(FreeShipQt FreeShipFlatpakUpdater)
