
if (NOT TARGET lwip) 
  message(FATAL_ERROR "Define a library called lwip that incorporate"
                      "(as an INTERFACE dep): the lwip port, options, and any lwip apps "
                      "you intend to depend on from this library.")
endif()

if (NOT TARGET freertos)
  message(FATAL_ERROR "Define a library called freertos that incorporate"
                      "(as an INTERFACE dep): the freertos port, options, kernel, and heap.")
endif()

add_library(lwipcpp_common EXCLUDE_FROM_ALL lwip_cpp/error.cc lwip_cpp/error.h)
set_target_properties(lwipcpp_common PROPERTIES COMPILER_LANGUAGE CXX)
set_target_properties(lwipcpp_common PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(lwipcpp_common PUBLIC stdc++)
target_compile_features(lwipcpp_common PUBLIC cxx_std_23)

add_library(lwipcpp_mqtt EXCLUDE_FROM_ALL lwip_cpp/mqtt.cc lwip_cpp/mqtt.h)
target_link_libraries(lwipcpp_mqtt PRIVATE freertos lwip lwipcpp_common stdc++)
