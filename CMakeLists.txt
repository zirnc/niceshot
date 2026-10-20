cmake_minimum_required(VERSION 3.25)

project(niceshot
  VERSION 0.1.0
  DESCRIPTION "A screenshot shelf for macOS, built one layer at a time"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NICESHOT_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(NICESHOT_WERROR "Treat warnings as errors" OFF)

set(NICESHOT_WARNINGS -Wall -Wextra -Wpedantic -Wshadow)
if(NICESHOT_WERROR)
  list(APPEND NICESHOT_WARNINGS -Werror)
endif()

if(NICESHOT_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# core: domain + ports + use cases. Standard library only.
# If you need a platform framework here, the code belongs in an adapter.
# ---------------------------------------------------------------------------
add_library(niceshot_core STATIC
  src/domain/domain.cpp
  src/app/app.cpp
  src/app/watch.cpp
)
target_include_directories(niceshot_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(niceshot_core PRIVATE ${NICESHOT_WARNINGS})

# ---------------------------------------------------------------------------
# adapters: SQLite, CoreFoundation, CoreServices (FSEvents), ImageIO.
# ---------------------------------------------------------------------------
add_library(niceshot_adapters STATIC
  src/adapters/macos/screenshots.cpp
  src/adapters/macos/polling_source.cpp
)
target_link_libraries(niceshot_adapters PUBLIC niceshot_core)
target_compile_options(niceshot_adapters PRIVATE ${NICESHOT_WARNINGS})

find_library(SQLITE3_LIBRARY sqlite3 REQUIRED)
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
find_library(CORESERVICES_FRAMEWORK CoreServices REQUIRED)
find_library(IMAGEIO_FRAMEWORK ImageIO REQUIRED)

target_link_libraries(niceshot_adapters PRIVATE
  ${SQLITE3_LIBRARY}
  ${COREFOUNDATION_FRAMEWORK}
  ${CORESERVICES_FRAMEWORK}
  ${IMAGEIO_FRAMEWORK}
)

# ---------------------------------------------------------------------------
# CLI: the composition root. Only this target knows concrete types.
# ---------------------------------------------------------------------------
add_executable(niceshot src/cli/main.cpp)
target_link_libraries(niceshot PRIVATE niceshot_adapters)
target_compile_options(niceshot PRIVATE ${NICESHOT_WARNINGS})

# ---------------------------------------------------------------------------
# tests
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME niceshot_help COMMAND niceshot --help)

# ---------------------------------------------------------------------------
# install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS niceshot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
