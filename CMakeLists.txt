cmake_minimum_required(VERSION 3.21)
project(Pixelle VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Test)

# Core library: document, history, file I/O (no widget dependencies beyond Gui)
add_library(pixellecore STATIC
    src/drawing/DrawingDocument.cpp
    src/history/UndoStack.cpp
    src/io/ImageFileService.cpp
    src/drawing/Raster.cpp
    src/drawing/ImageOps.cpp
    src/drawing/Editor.cpp
    src/drawing/Tools.cpp
)
target_include_directories(pixellecore PUBLIC src)
target_link_libraries(pixellecore PUBLIC Qt6::Core Qt6::Gui)

# UI library
add_library(pixelleui STATIC
    src/ui/CanvasView.cpp
    src/ui/ColorPalette.cpp
    src/ui/Dialogs.cpp
    src/app/MainWindow.cpp
)
target_include_directories(pixelleui PUBLIC src)
target_link_libraries(pixelleui PUBLIC pixellecore Qt6::Widgets)

# Application
qt_add_executable(pixelle
    src/app/main.cpp
    resources/resources.qrc
)
target_link_libraries(pixelle PRIVATE pixelleui)

# Tests
enable_testing()
add_executable(test_document tests/test_document.cpp)
target_link_libraries(test_document PRIVATE pixelleui Qt6::Test)
add_test(NAME document COMMAND test_document)
set_tests_properties(document PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(test_raster tests/test_raster.cpp)
target_link_libraries(test_raster PRIVATE pixelleui Qt6::Test)
add_test(NAME raster COMMAND test_raster)
set_tests_properties(raster PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(test_editor tests/test_editor.cpp)
target_link_libraries(test_editor PRIVATE pixelleui Qt6::Test)
add_test(NAME editor COMMAND test_editor)
set_tests_properties(editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

install(TARGETS pixelle RUNTIME DESTINATION bin)
install(FILES resources/pixelle.desktop DESTINATION share/applications)

add_executable(test_gui tests/test_gui.cpp)
target_link_libraries(test_gui PRIVATE pixelleui Qt6::Test)
add_test(NAME gui COMMAND test_gui)
set_tests_properties(gui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
