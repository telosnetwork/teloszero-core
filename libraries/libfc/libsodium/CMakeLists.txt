# libsodium for the ed25519_verify host function (Ed25519, RFC 8032).
#
# Consensus code: signature verification must give the same answer on every node, so libsodium is vendored as a
# submodule pinned to one release (libsodium/ = tag 1.0.22-RELEASE, commit 77e1ce5d6dee871c49ef211222ba18ef0c486bda)
# and linked statically. The OS package is never used.
#
# libsodium's own build is autotools. The release tag ships a generated `configure` and the build runs out of tree,
# so the source tree stays untouched. AM_MAINTAINER_MODE is off by default, so make never tries to rerun automake
# over file times from the git checkout.

include(ExternalProject)

set(LIBSODIUM_SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/libsodium)
set(LIBSODIUM_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/install)
set(LIBSODIUM_LIBRARY     ${LIBSODIUM_INSTALL_DIR}/lib/libsodium.a)
set(LIBSODIUM_INCLUDE_DIR ${LIBSODIUM_INSTALL_DIR}/include)

if(NOT EXISTS ${LIBSODIUM_SOURCE_DIR}/configure)
   message(FATAL_ERROR "libsodium submodule missing: run `git submodule update --init libraries/libfc/libsodium/libsodium`")
endif()

ExternalProject_Add(libsodium_project
   SOURCE_DIR        ${LIBSODIUM_SOURCE_DIR}
   BINARY_DIR        ${CMAKE_CURRENT_BINARY_DIR}/build
   INSTALL_DIR       ${LIBSODIUM_INSTALL_DIR}
   CONFIGURE_COMMAND ${LIBSODIUM_SOURCE_DIR}/configure
                     --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib
                     --disable-shared --enable-static --with-pic --disable-debug
                     CC=${CMAKE_C_COMPILER}
   BUILD_COMMAND     make -j4
   INSTALL_COMMAND   make install
   UPDATE_COMMAND    ""
   PATCH_COMMAND     ""
   TEST_COMMAND      ""
   BUILD_BYPRODUCTS  ${LIBSODIUM_LIBRARY}
)

# the include dir must exist when the imported target is defined, before libsodium is installed into it
file(MAKE_DIRECTORY ${LIBSODIUM_INCLUDE_DIR})

add_library(sodium STATIC IMPORTED GLOBAL)
set_target_properties(sodium PROPERTIES
   IMPORTED_LOCATION             ${LIBSODIUM_LIBRARY}
   INTERFACE_INCLUDE_DIRECTORIES ${LIBSODIUM_INCLUDE_DIR}
   INTERFACE_COMPILE_DEFINITIONS SODIUM_STATIC=1
)
add_dependencies(sodium libsodium_project)
