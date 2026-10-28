add_executable(subdivision_regression subdivision_regression.cpp)
target_link_libraries(subdivision_regression PRIVATE luxcore_static boost::boost embree::embree robin_hood::robin_hood Imath::Imath openimageio::openimageio nlohmann_json::nlohmann_json OpenMP::OpenMP_CXX)
if(MSVC)
  target_link_options(subdivision_regression PRIVATE /DEBUG:FULL)
endif()

add_test(NAME subdivision_regression COMMAND subdivision_regression)
add_test(NAME texture_reference_regression COMMAND subdivision_regression textures)
