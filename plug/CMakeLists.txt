mx_resource_clear()

add_executable(mxplug
	main.cpp
)

# Link main
target_link_libraries(mxplug PRIVATE mxrpc mxrdb mxdrv mxhttp mxbck mxlbk)

if(WIN32)
	target_link_libraries(mxplug PRIVATE pdh)

	# Statically link against C runtime (assuming we use mingw)
	# target_link_options(mxplug PRIVATE -static-libstdc++ -static-libgcc)
endif()

# For the resources
target_include_directories(mxplug PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
add_custom_command(TARGET mxplug POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:mxplug> $<TARGET_FILE_DIR:mxplug>
	COMMAND_EXPAND_LISTS
)

# Package libs
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/websocket.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/convert.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/event.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/rpc.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/rdb.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/utils.ts lib)

# Package components
file(GLOB MX_COMPONENT_FILES "${CMAKE_SOURCE_DIR}/frontend/src/api/*.tsx")
foreach(file ${MX_COMPONENT_FILES})
	mx_resource_append(${file} components)
endforeach()

# Package solid-ui
file(GLOB SOLID_UI_FILES "${CMAKE_SOURCE_DIR}/frontend/src/components/ui/*.tsx")
foreach(file ${SOLID_UI_FILES})
	mx_resource_append(${file} ui)
endforeach()

# Package solid-ui-lib
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/lib/utils.ts uilib)

# Package npm/yarn build configs
mx_resource_append(${CMAKE_SOURCE_DIR}/plug/package.json build)
mx_resource_append(${CMAKE_SOURCE_DIR}/plug/tsconfig.json build)
mx_resource_append(${CMAKE_SOURCE_DIR}/plug/vite.config.ts build)
mx_resource_append(${CMAKE_SOURCE_DIR}/plug/globals.d.ts build)

# Main plugin file (just a placeholder)
mx_resource_append(${CMAKE_SOURCE_DIR}/plug/plugin.tsx binary)

# Package assets
file(GLOB MX_ASSET_FILES "${CMAKE_SOURCE_DIR}/plug/assets/*")
foreach(file ${MX_ASSET_FILES})
	mx_resource_append(${file} assets)
endforeach()

mx_resource_gen(mxplug)
