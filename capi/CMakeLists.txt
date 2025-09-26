add_library(MxCapi
	mxcapi.cpp
	$<TARGET_OBJECTS:mxsys>
)

target_link_libraries(MxCapi PUBLIC mxbck)

# Enforce c++20
target_compile_features(MxCapi PUBLIC cxx_std_20)

add_library(mxcapi ALIAS MxCapi)
