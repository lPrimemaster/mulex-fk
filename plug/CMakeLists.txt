mx_resource_clear()

add_executable(mxplug
	main.cpp
	../mxsystem.cpp
)

# Link main
target_link_libraries(mxplug PRIVATE mxrpc mxrdb mxdrv mxhttp mxbck)

if(WIN32)
	target_link_libraries(mxplug PRIVATE pdh)

	# Statically link against C runtime (assuming we use mingw)
	target_link_options(mxplug PRIVATE -static-libstdc++ -static-libgcc)
endif()

# For the resources
target_include_directories(mxplug PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

# Package libs
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/websocket.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/convert.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/event.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/rpc.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/rdb.ts lib)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/utils.ts lib)

# Package components
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/Button.tsx components)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/Plot.tsx components)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/GaugeVertical.tsx components)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/Selector.tsx components)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/Switch.tsx components)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/ValueControl.tsx components)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/ValuePanel.tsx components)

# Package solid-ui
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/components/ui/tooltip.tsx ui)
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/components/ui/switch.tsx ui)

# Package solid-ui-lib
mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/lib/utils.ts uilib)

# Package npm/yarn build configs
mx_resource_append(${CMAKE_SOURCE_DIR}/plug/package.json build)
mx_resource_append(${CMAKE_SOURCE_DIR}/plug/tsconfig.json build)
mx_resource_append(${CMAKE_SOURCE_DIR}/plug/vite.config.ts build)

# Main plugin file (just a placeholder)
mx_resource_append(${CMAKE_SOURCE_DIR}/plug/plugin.tsx)

mx_resource_gen()
