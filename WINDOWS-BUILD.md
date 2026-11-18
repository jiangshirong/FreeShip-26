## Windows x64 release source (0.1.14)

`FreeShip-Qt/` contains the application; `FreeShip-Tests/qt/` contains its C++ test sources. From the repository root, with Qt 6.8.3, its matching MinGW 13.1 compiler, Ninja, CMake and Python on PATH:

```sh
cmake -S FreeShip-Qt -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt installation>/6.8.3/mingw_64"
cmake --build build --target FreeShipQt FreeShipDocumentService FreeShipNativeLinesplan FreeShipGeometryExport
```

`FreeShip-Qt/runtime-data/` contains the corresponding translated catalogs and inherited runtime assets (including the legacy-source archive). The normal Windows download includes the installer with the required runtime and notices. The internal MSI is not a supported standalone upgrade. This release does not distribute an Ubuntu package.
