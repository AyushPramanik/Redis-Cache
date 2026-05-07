add_executable(cache_tests
  test_resp_parser.cpp
  test_store.cpp
  test_lru.cpp
  test_commands.cpp
)

target_link_libraries(cache_tests
  PRIVATE cachelib Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(cache_tests)
