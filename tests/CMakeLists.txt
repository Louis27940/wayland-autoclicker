find_package(Qt6 REQUIRED COMPONENTS Test DBus Gui Widgets)

# Test TimingPolicy
add_executable(tst_TimingPolicy
    tst_TimingPolicy.cpp
    ../src/TimingPolicy.cpp
    ../src/TimingPolicy.hpp
)
target_include_directories(tst_TimingPolicy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(tst_TimingPolicy PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_TimingPolicy COMMAND tst_TimingPolicy)

# Test ClickController
add_executable(tst_ClickController
    tst_ClickController.cpp
    ../src/ClickController.cpp
    ../src/ClickController.hpp
    ../src/TimingPolicy.cpp
    ../src/TimingPolicy.hpp
    FakeInputInjector.hpp
)
target_include_directories(tst_ClickController PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_ClickController PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tst_ClickController COMMAND tst_ClickController)

# Test DBusSignatures
add_executable(tst_DBusSignatures
    tst_DBusSignatures.cpp
    ../src/PortalManager.cpp
    ../src/PortalManager.hpp
)
target_include_directories(tst_DBusSignatures PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(tst_DBusSignatures PRIVATE Qt6::Core Qt6::Gui Qt6::DBus Qt6::Test)
add_test(NAME tst_DBusSignatures COMMAND tst_DBusSignatures)

# Test DebugInfo
add_executable(tst_DebugInfo
    tst_DebugInfo.cpp
    ../src/DebugDialog.cpp
    ../src/DebugDialog.hpp
    ../src/PortalManager.cpp
    ../src/PortalManager.hpp
    ../src/ClickController.cpp
    ../src/ClickController.hpp
    ../src/TimingPolicy.cpp
    ../src/TimingPolicy.hpp
)
target_include_directories(tst_DebugInfo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(tst_DebugInfo PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::DBus Qt6::Test)
add_test(NAME tst_DebugInfo COMMAND tst_DebugInfo)
