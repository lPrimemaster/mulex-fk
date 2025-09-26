add_library(MxCapi SHARED
	mxcapi.cpp
	$<TARGET_OBJECTS:mxsys>
)

target_link_libraries(MxCapi PUBLIC mxbck)

# Hide symbols by default
# We don't want to export the whole tables of all
# of the mx symbols
set_target_properties(MxCapi PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Enforce c++20
target_compile_features(MxCapi PUBLIC cxx_std_20)

add_library(mxcapi ALIAS MxCapi)
