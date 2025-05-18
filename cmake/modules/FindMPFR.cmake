set(MPFR_PREFIX "" CACHE PATH "The path to the prefix of an MPFR installation")

find_path(MPFR_INCLUDE_DIR
  NAMES mpfr.h
  PATHS ${MPFR_PREFIX}/include /usr/include /usr/local/include
)

find_library(MPFR_LIBRARY
  NAMES mpfr
  PATHS ${MPFR_PREFIX}/lib /usr/lib /usr/local/lib
)

if(MPFR_INCLUDE_DIR AND MPFR_LIBRARY)
  get_filename_component(MPFR_LIBRARY_DIR ${MPFR_LIBRARY} PATH)
  set(MPFR_FOUND TRUE)
else()
  set(MPFR_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPFR DEFAULT_MSG MPFR_INCLUDE_DIR MPFR_LIBRARY)

mark_as_advanced(MPFR_INCLUDE_DIR MPFR_LIBRARY)

if(MPFR_FOUND)
  if(NOT MPFR_FIND_QUIETLY)
    message(STATUS "Found MPFR: ${MPFR_LIBRARY}")
  endif()

  # Create imported target MPFR::MPFR
  add_library(MPFR::MPFR UNKNOWN IMPORTED)

  set_target_properties(MPFR::MPFR PROPERTIES
    IMPORTED_LOCATION "${MPFR_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${MPFR_INCLUDE_DIR}"
  )
else()
  if(MPFR_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find MPFR")
  endif()
endif()
