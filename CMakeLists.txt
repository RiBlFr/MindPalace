cmake_minimum_required(VERSION 3.16)

project(MindPalace LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_SOURCE_DIR}/cmake/LocalConfig.cmake" OPTIONAL RESULT_VARIABLE _LOCALCFG)

if(NOT DEFINED CMAKE_PREFIX_PATH)
    if(DEFINED ENV{QT6_DIR})
        set(CMAKE_PREFIX_PATH "$ENV{QT6_DIR}")
    elseif(DEFINED ENV{Qt6_DIR})
        set(CMAKE_PREFIX_PATH "$ENV{Qt6_DIR}")
    elseif(DEFINED ENV{CMAKE_PREFIX_PATH})
        set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS
        Widgets
        Core
        Gui
        Network
        QuickWidgets
        Quick
        Qml
        Test
)

include_directories(src)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        "src/*.cpp"
        "src/*.h"
        "src/view/*.cpp"
        "src/view/*.h"
        "src/view/*.ui"
        "res/resources.qrc"
)

add_executable(MindPalace ${SOURCES})
set_target_properties(MindPalace PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(MindPalace PRIVATE
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::QuickWidgets
        Qt6::Quick
        Qt6::Qml
)

if (WIN32)
    target_link_libraries(MindPalace PRIVATE dwmapi)
endif()

enable_testing()

set(DECK_CONTROLLER_TEST_SOURCES
        tests/TestDeckController.cpp
        src/controller/deckcontroller.cpp
        src/controller/deckcontroller.h
        src/model/Deck.cpp
        src/model/Deck.h
        src/model/Card.cpp
        src/model/Card.h
        src/service/storagemanager.cpp
        src/service/storagemanager.h
)

add_executable(TestDeckController ${DECK_CONTROLLER_TEST_SOURCES})
target_link_libraries(TestDeckController PRIVATE
        Qt6::Core
        Qt6::Test
)
target_include_directories(TestDeckController PRIVATE src)

add_test(NAME DeckControllerTest COMMAND TestDeckController)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data"
        COMMENT "Syncing data/ to build output..."
)

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")

    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()

    find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt windeployqt.exe
            HINTS
            "${QT_INSTALL_PATH}/bin"
            "${CMAKE_PREFIX_PATH}/bin"
    )

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --qmldir "${CMAKE_SOURCE_DIR}/res/styles"
                "$<TARGET_FILE:${PROJECT_NAME}>"
                COMMENT "Deploying Qt runtime, plugins and QML dependencies..."
        )
        add_custom_command(TARGET TestDeckController POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE:TestDeckController>"
                COMMENT "Deploying Qt runtime for TestDeckController..."
        )
    else()
        message(WARNING "windeployqt not found. You may need to deploy Qt DLLs and QML plugins manually.")
    endif()
endif ()
