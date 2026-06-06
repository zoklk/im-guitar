# CMake generated Testfile for 
# Source directory: C:/VSCode/im-guitar
# Build directory: C:/VSCode/im-guitar/build_web
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_tests "C:/VSCode/im-guitar/emsdk/node/22.16.0_64bit/bin/node.exe" "C:/VSCode/im-guitar/build_web/unit_tests.html")
set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/VSCode/im-guitar/CMakeLists.txt;206;add_test;C:/VSCode/im-guitar/CMakeLists.txt;0;")
subdirs("_deps/nlohmann_json-build")
subdirs("_deps/googletest-build")
