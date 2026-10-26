cmake_minimum_required(VERSION 3.22)
project(Fission360 VERSION 1.13.44 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# OSD::SetSignal turns access violations inside OCCT into C++ exceptions; unwinding them needs /EHa
if(MSVC)
    string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Qt6 (installed via aqtinstall — see Build in the README)
# ---------------------------------------------------------------------------
# Qt lives next to the fission360 folder (shared with the parent workspace)
set(QT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../6.8.2/msvc2022_64")
if(NOT EXISTS "${QT_DIR}")
    set(QT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/6.8.2/msvc2022_64")
endif()
if(NOT EXISTS "${QT_DIR}")
    message(FATAL_ERROR "Qt6 not found at ${CMAKE_CURRENT_SOURCE_DIR}/../6.8.2/msvc2022_64. Install via aqtinstall:\n"
            "  pip install aqtinstall && aqt install-qt windows desktop 6.8.2 win64_msvc2022_64")
endif()
list(APPEND CMAKE_PREFIX_PATH "${QT_DIR}")

find_package(Qt6 REQUIRED COMPONENTS Core Quick QuickControls2 Svg Widgets OpenGL OpenGLWidgets Network Test WebEngineQuick)
find_package(Qt6QuickCompiler QUIET)
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# OpenCASCADE (from OrcaSlicer deps)
# ---------------------------------------------------------------------------
# Point FISSION_ORCA_ROOT at an OrcaSlicer source checkout whose dependencies have been built (it provides
# OCCT). Either pass -DFISSION_ORCA_ROOT=<path> or set the FISSION_ORCA_ROOT environment variable;
# otherwise ../MyOrcaSlicer and %USERPROFILE%/Documents/GitHub/MyOrcaSlicer are tried.
set(FISSION_ORCA_ROOT "" CACHE PATH "OrcaSlicer checkout with built deps (provides OpenCASCADE)")
if(NOT FISSION_ORCA_ROOT AND DEFINED ENV{FISSION_ORCA_ROOT})
    set(FISSION_ORCA_ROOT "$ENV{FISSION_ORCA_ROOT}")
endif()
if(NOT FISSION_ORCA_ROOT)
    foreach(cand "${CMAKE_CURRENT_SOURCE_DIR}/../MyOrcaSlicer" "$ENV{USERPROFILE}/Documents/GitHub/MyOrcaSlicer")
        if(EXISTS "${cand}/deps/build/dep_OCCT-prefix")
            set(FISSION_ORCA_ROOT "${cand}")
            break()
        endif()
    endforeach()
endif()
set(OCCT_DIR "${FISSION_ORCA_ROOT}/deps/build/dep_OCCT-prefix/src/dep_OCCT-build")
set(OCCT_SRC "${FISSION_ORCA_ROOT}/deps/build/dep_OCCT-prefix/src/dep_OCCT")
set(OCCT_FLAT_INCLUDE "${FISSION_ORCA_ROOT}/OrcaSlicer_dep/usr/local/include/occt")

if(NOT FISSION_ORCA_ROOT OR NOT EXISTS "${OCCT_DIR}" OR NOT EXISTS "${OCCT_SRC}")
    message(FATAL_ERROR "OCCT not found (looked under '${FISSION_ORCA_ROOT}'). Build OrcaSlicer's deps, then run "
            "cmake with -DFISSION_ORCA_ROOT=<OrcaSlicer checkout>.")
endif()

set(OCCT_INCLUDE_DIR "${OCCT_FLAT_INCLUDE}" "${OCCT_SRC}/src")
file(GLOB OCCT_SUBDIRS "${OCCT_SRC}/src/*")
set(OCCT_EXTRA_INCLUDE_DIRS "")
foreach(d ${OCCT_SUBDIRS})
    if(IS_DIRECTORY "${d}")
        list(APPEND OCCT_EXTRA_INCLUDE_DIRS "${d}")
    endif()
endforeach()

set(OCCT_LIBS_DIR "${OCCT_DIR}/lib/occt")
file(GLOB OCCT_LIBS "${OCCT_LIBS_DIR}/*.lib")

# ---------------------------------------------------------------------------
# Missing OCCT modules built as OBJECT libraries from source
# ---------------------------------------------------------------------------
set(OCCT_MODULE_SRC "${OCCT_SRC}/src")
# Toolkits not present as .lib in this OCCT build — compile their sources into the app
set(OCCT_MISSING_MODULES
    BRepFill TopOpeBRepTool TopOpeBRepDS TopOpeBRepBuild BRepAlgoAPI BRepCheck BRepLib
    BRepAlgo BRepFeat LocOpe)
set(OCCT_OBJECT_LIBS "")
foreach(mod ${OCCT_MISSING_MODULES})
    file(GLOB_RECURSE MOD_SRCS "${OCCT_MODULE_SRC}/${mod}/*.cxx")
    if(MOD_SRCS)
        add_library(${mod} OBJECT ${MOD_SRCS})
        target_include_directories(${mod} PRIVATE ${OCCT_INCLUDE_DIR} ${OCCT_EXTRA_INCLUDE_DIRS})
        list(APPEND OCCT_OBJECT_LIBS ${mod})
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
file(GLOB_RECURSE FISSION_SRCS "src/*.cpp" "src/*.h")
list(REMOVE_ITEM FISSION_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/SelfTest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/UiTestDriver.cpp"
)
file(GLOB FISSION_TESTS "src/app/SelfTest.cpp" "src/app/UiTestDriver.cpp")

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
# App icon resource in its own target: rc.exe fails with the app's OCCT include list ("filename too long")
add_library(Fission360Res OBJECT shell/Fission360.rc)
set_target_properties(Fission360Res PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

add_executable(Fission360
    ${FISSION_SRCS}
    ${FISSION_TESTS}
    $<TARGET_OBJECTS:Fission360Res>
)

# Explorer thumbnail handler for .f360 (plain Win32 + WIC, no Qt); registered by Fission360.exe --register
add_library(Fission360Thumbs SHARED shell/F360Thumbnail.cpp shell/F360Thumbnail.def)
set_target_properties(Fission360Thumbs PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_definitions(Fission360Thumbs PRIVATE UNICODE _UNICODE)
add_dependencies(Fission360 Fission360Thumbs)

target_include_directories(Fission360 PRIVATE src ${OCCT_INCLUDE_DIR} ${OCCT_EXTRA_INCLUDE_DIRS})
target_link_directories(Fission360 PRIVATE "${OCCT_LIBS_DIR}")
target_link_libraries(Fission360 PRIVATE
    ${OCCT_LIBS}
    ${OCCT_OBJECT_LIBS}
    Qt6::Core Qt6::Quick Qt6::QuickControls2 Qt6::Svg Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Network Qt6::Test
    # Not Qt6::WebEngineQuick: the McMaster dialog's "import QtWebEngine" loads Chromium on first use, and
    # windeployqt still ships it because it scans the QML imports
    opengl32 glu32
)

target_compile_definitions(Fission360 PRIVATE FISSION_VERSION="${PROJECT_VERSION}")

# Python scripts and add-ins (PythonHost): built against CPython's stable ABI (python3.lib, 3.11+), with
# python3.dll delay-loaded from the user's own Python at run time, so Fission still starts without Python.
# Without Python headers on the build machine the Scripts dialog says Python support was not built in.
find_package(Python3 3.11 COMPONENTS Development.Module QUIET)
if(Python3_Development.Module_FOUND AND WIN32)
    target_include_directories(Fission360 PRIVATE ${Python3_INCLUDE_DIRS})
    target_link_directories(Fission360 PRIVATE ${Python3_LIBRARY_DIRS})
    target_compile_definitions(Fission360 PRIVATE FISSION_HAS_PYTHON Py_LIMITED_API=0x030B0000)
    target_link_libraries(Fission360 PRIVATE delayimp)
    target_link_options(Fission360 PRIVATE "/DELAYLOAD:python3.dll")
    message(STATUS "Python scripting: stable ABI from ${Python3_INCLUDE_DIRS}")
else()
    message(STATUS "Python scripting: off (no Python 3.11+ development files found)")
endif()
set(QT_BIN_DIR "${QT_DIR}/bin")

# App / brand icons (window icon, Send to Orca, …)
qt_add_resources(Fission360 "fission_icons"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/fission360.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/orca.svg"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/orca.png"
)

# QML resources (after the target exists — required by qt_add_resources).
# Paths are relative to assets/qml via BASE so aliases stay FissionApplication/... under /qml.
set(FISSION_QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/qml")
# The root THIRD_PARTY_NOTICES.md is the single source; a copy is embedded for the About dialog
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "${FISSION_QML_DIR}/THIRD_PARTY_NOTICES.md" COPYONLY)
qt_add_resources(Fission360 "fission_qml"
    PREFIX "/qml"
    BASE "${FISSION_QML_DIR}"
    FILES
        "${FISSION_QML_DIR}/main.qml"
        "${FISSION_QML_DIR}/LICENSE"
        "${FISSION_QML_DIR}/THIRD_PARTY_NOTICES.md"
        "${FISSION_QML_DIR}/FissionApplication/qmldir"
        "${FISSION_QML_DIR}/FissionApplication/AboutDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/UpdateAvailableDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/ExportViewportPngDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/Theme.qml"
        "${FISSION_QML_DIR}/FissionApplication/FloatingPanel.qml"
        "${FISSION_QML_DIR}/FissionApplication/MainWindow.qml"
        "${FISSION_QML_DIR}/FissionApplication/Toolbar.qml"
        "${FISSION_QML_DIR}/FissionApplication/ToolbarButton.qml"
        "${FISSION_QML_DIR}/FissionApplication/Glyph.qml"
        "${FISSION_QML_DIR}/FissionApplication/BrowserPanel.qml"
        "${FISSION_QML_DIR}/FissionApplication/CommandBar.qml"
        "${FISSION_QML_DIR}/FissionApplication/ViewCube.qml"
        "${FISSION_QML_DIR}/FissionApplication/TimelinePanel.qml"
        "${FISSION_QML_DIR}/FissionApplication/Viewport.qml"
        "${FISSION_QML_DIR}/FissionApplication/StatusBar.qml"
        "${FISSION_QML_DIR}/FissionApplication/MarkMenu.qml"
        "${FISSION_QML_DIR}/FissionApplication/PropertyPalette.qml"
        "${FISSION_QML_DIR}/FissionApplication/InspectPanel.qml"
        "${FISSION_QML_DIR}/FissionApplication/ParametersDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/DocumentSettingsDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/AppearanceSettingsDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/CommandSearch.qml"
        "${FISSION_QML_DIR}/FissionApplication/ShortcutsDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/ScriptsDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/UiTest.qml"
        "${FISSION_QML_DIR}/FissionApplication/UiPerf.qml"
        "${FISSION_QML_DIR}/FissionApplication/BatchExportDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/McMasterInsertDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/ProjectsPanel.qml"
        "${FISSION_QML_DIR}/FissionApplication/SheetMetalRulesDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/VersionHistoryDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/RecoverAutosaveDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/ManufactureDialog.qml"
        "${FISSION_QML_DIR}/FissionApplication/MoveTriad.qml"
        "${FISSION_QML_DIR}/FissionApplication/SketchMovePanel.qml"
        "${FISSION_QML_DIR}/FissionRender/qmldir"
        "${FISSION_QML_DIR}/FissionRender/AppearanceDialog.qml"
        "${FISSION_QML_DIR}/FissionRender/RenderDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/qmldir"
        "${FISSION_QML_DIR}/FissionFeatures/FeaturePanel.qml"
        "${FISSION_QML_DIR}/FissionFeatures/FeatureDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/NumberField.qml"
        "${FISSION_QML_DIR}/FissionFeatures/ChoiceField.qml"
        "${FISSION_QML_DIR}/FissionFeatures/ExtrudeDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/RevolveDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/FilletDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/ChamferDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/ShellDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/LoftDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/SweepDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/MirrorDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/PatternDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/ConstructionDialog.qml"
        "${FISSION_QML_DIR}/FissionFeatures/SpecDialog.qml"
        "${FISSION_QML_DIR}/FissionSketch/qmldir"
        "${FISSION_QML_DIR}/FissionSketch/SketchPanel.qml"
        "${FISSION_QML_DIR}/FissionDrawing/qmldir"
        "${FISSION_QML_DIR}/FissionDrawing/DrawingWorkspace.qml"
        "${FISSION_QML_DIR}/FissionDrawing/DrawingViewItem.qml"
        "${FISSION_QML_DIR}/FissionDrawing/CreateDrawingDialog.qml"
)

# ---------------------------------------------------------------------------
# Deployment
# ---------------------------------------------------------------------------
# The dep_OCCT-build/bin/occt tree is incomplete (no TKBRep.dll etc.) while other OCCT DLLs
# still import those libraries. Prefer the full OrcaSlicer_dep OCCT runtime; fall back to build bin.
set(OCCT_RUNTIME_DIR "")
foreach(_cand
        "${FISSION_ORCA_ROOT}/deps/build/OrcaSlicer_dep/usr/local/bin/occt"
        "${FISSION_ORCA_ROOT}/OrcaSlicer_dep/usr/local/bin/occt"
        "${OCCT_DIR}/bin/occt")
    if(EXISTS "${_cand}/TKBRep.dll")
        set(OCCT_RUNTIME_DIR "${_cand}")
        break()
    endif()
endforeach()
if(NOT OCCT_RUNTIME_DIR)
    set(OCCT_RUNTIME_DIR "${OCCT_DIR}/bin/occt")
    message(WARNING "Complete OCCT runtime (with TKBRep.dll) not found; using ${OCCT_RUNTIME_DIR}")
endif()
file(GLOB _occt_copy "${OCCT_RUNTIME_DIR}/*.dll")
# FreeType is required by some OCCT / Qt paths at runtime
set(_freetype_dll "")
foreach(_cand
        "${FISSION_ORCA_ROOT}/deps/build/OrcaSlicer_dep/usr/local/bin/freetype.dll"
        "${FISSION_ORCA_ROOT}/dist/MyOrcaSlicer-portable/freetype.dll"
        "${FISSION_ORCA_ROOT}/build-portable/src/Release/freetype.dll")
    if(EXISTS "${_cand}")
        set(_freetype_dll "${_cand}")
        break()
    endif()
endforeach()
if(_freetype_dll)
    list(APPEND _occt_copy "${_freetype_dll}")
endif()

add_custom_command(TARGET Fission360 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_occt_copy} $<TARGET_FILE_DIR:Fission360>
    COMMAND "${QT_BIN_DIR}/windeployqt.exe" --no-translations --no-compiler-runtime
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/assets/qml" $<TARGET_FILE:Fission360>
    COMMENT "Deploying OCCT and Qt runtime from ${OCCT_RUNTIME_DIR}"
    VERBATIM
)

# ---------------------------------------------------------------------------
# Self-test target (no GUI)
# ---------------------------------------------------------------------------
add_executable(FissionSelfTest EXCLUDE_FROM_ALL
    ${FISSION_SRCS}
    ${FISSION_TESTS}
)
target_compile_definitions(FissionSelfTest PRIVATE SELF_TEST_ONLY=1 FISSION_VERSION="${PROJECT_VERSION}")
target_include_directories(FissionSelfTest PRIVATE src ${OCCT_INCLUDE_DIR} ${OCCT_EXTRA_INCLUDE_DIRS})
target_link_directories(FissionSelfTest PRIVATE "${OCCT_LIBS_DIR}")
target_link_libraries(FissionSelfTest PRIVATE
    ${OCCT_LIBS}
    ${OCCT_OBJECT_LIBS}
    Qt6::Core Qt6::Quick Qt6::QuickControls2 Qt6::Svg Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Network Qt6::Test
    opengl32 glu32
)
