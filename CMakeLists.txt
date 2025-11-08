cmake_minimum_required(VERSION 3.16)

project(MeeBlueReader VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Bluetooth Core Gui Quick)


qt_standard_project_setup(REQUIRES 6.5)

# Define source files
set(SOURCES
    main.cpp
    meebluereader.h
    meebluereader.cpp
)

# Add iOS-specific files
if (IOS)
    list(APPEND SOURCES
        ibeaconscanner.h
        ibeaconscanner_ios.mm
    )
    # Enable Objective-C++ for .mm files only
    set_source_files_properties(ibeaconscanner_ios.mm PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
endif()

qt_add_executable(appMeeBlueReader
    ${SOURCES}
)

qt_add_qml_module(appMeeBlueReader
    URI MeeBlueReader
    VERSION 1.0
    QML_FILES
        Main.qml
        RESOURCES android/AndroidManifest.xml android/build.gradle android/res/values/libs.xml android/res/xml/qtprovider_paths.xml android/gradle/wrapper/gradle-wrapper.jar android/gradle/wrapper/gradle-wrapper.properties android/gradle.properties android/gradlew android/gradlew.bat
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appMeeBlueReader PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appMeeBlueReader
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appMeeBlueReader PUBLIC
    Qt::Bluetooth
    Qt::Core
    Qt::Gui
    Qt::Quick
)

include(GNUInstallDirs)

if (APPLE)

    set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.tarmoj.meeblue")
    set(MACOSX_BUNDLE_BUNDLE_NAME "Meeblue Reader")

    set_target_properties(appMeeBlueReader PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
    )




    # Link iOS-specific frameworks
    if (IOS)
        find_library(CORELOCATION_FRAMEWORK CoreLocation)
        find_library(FOUNDATION_FRAMEWORK Foundation)
        target_link_libraries(appMeeBlueReader PUBLIC
            ${CORELOCATION_FRAMEWORK}
            ${FOUNDATION_FRAMEWORK}
        )



    endif()

endif()


install(TARGETS appMeeBlueReader
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
