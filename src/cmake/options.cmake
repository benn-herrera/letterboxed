# build project generation options
set(BNG_BUILD_TESTS TRUE CACHE BOOL "add tests suites to project")
set(BNG_USE_FOLDERS TRUE CACHE BOOL "use folders in IDE organization")

set(BNG_OPTIMIZED_BUILD_TYPE BNG_DEBUG CACHE STRING "what it says on the tin")
set_property(CACHE BNG_OPTIMIZED_BUILD_TYPE PROPERTY STRINGS BNG_DEBUG BNG_RELEASE)
