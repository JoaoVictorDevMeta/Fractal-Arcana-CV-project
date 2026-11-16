cmake_minimum_required(VERSION 3.16)
project(duelo_magos CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED)

# --- Jogo -------------------------------------------------------------------
file(GLOB_RECURSE SRC src/*.cpp)
if(SRC)
  add_executable(duelo_magos ${SRC})
  target_include_directories(duelo_magos PRIVATE src)
  target_link_libraries(duelo_magos PRIVATE ${OpenCV_LIBS})
endif()

# --- Ferramentas ------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/tools/calibracao_hsv.cpp)
  add_executable(calibracao_hsv tools/calibracao_hsv.cpp)
  target_link_libraries(calibracao_hsv PRIVATE ${OpenCV_LIBS})
endif()

# --- Testes -----------------------------------------------------------------
enable_testing()

# Varinha azul: autoteste sintético (--selftest) + modo ao vivo com webcam
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_wand_blue.cpp)
  add_executable(test_wand_blue tests/test_wand_blue.cpp)
  target_link_libraries(test_wand_blue PRIVATE ${OpenCV_LIBS})
  add_test(NAME wand_blue_selftest COMMAND test_wand_blue --selftest)
endif()