# for Modern CMake import tbus2_api.a

set(Libtbuspp2_VERSION "0.16.4")


get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

set(Libtbuspp2_LIBRARY_DIRS ${PACKAGE_PREFIX_DIR}/lib)
set(Libtbuspp2_INCLUDE_DIRS ${PACKAGE_PREFIX_DIR}/inc)

add_library(tbuspp2::libtbuspp2 STATIC IMPORTED)
add_library(tbuspp2::libtbuspp2_auxi STATIC IMPORTED)

if (MSVC)
  set_target_properties(tbuspp2::libtbuspp2 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_PREFIX_DIR}/inc"
    IMPORTED_LOCATION "${PACKAGE_PREFIX_DIR}/lib/tbus2_api.lib"
    IMPORTED_LOCATION_DEBUG "${PACKAGE_PREFIX_DIR}/lib/tbus2_apid.lib"
  )
  set_target_properties(tbuspp2::libtbuspp2_auxi PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_PREFIX_DIR}/inc"
    IMPORTED_LOCATION "${PACKAGE_PREFIX_DIR}/lib/tbus2_auxi.lib"
  )
else()
  set_target_properties(tbuspp2::libtbuspp2 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_PREFIX_DIR}/inc"
    IMPORTED_LOCATION "${PACKAGE_PREFIX_DIR}/lib/libtbus2_api.a"
  )
  set_target_properties(tbuspp2::libtbuspp2_auxi PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_PREFIX_DIR}/inc"
    IMPORTED_LOCATION "${PACKAGE_PREFIX_DIR}/lib/libtbus2_auxi.a"
  )
endif()
