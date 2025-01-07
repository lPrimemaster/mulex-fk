#include "../mxsystem.h"
#include <mxres.h>
#include <filesystem>

using namespace mulex;

enum class Mode
{
	NEW,
	BUILD,
	UNKNOWN
};

inline static void PlugWriteFile(const std::string& resource, const std::string& path)
{
	LogMessage("[mxplug] Writing <%s>...", path.c_str());
	SysWriteBinFile(path, ResGetResource(resource));
}

static bool PlugUnpackFiles(const std::filesystem::path& path)
{
	LogMessage("[mxplug] Unpacking files...");

	// # Package libs
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/websocket.ts lib)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/convert.ts lib)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/event.ts lib)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/rpc.ts lib)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/rdb.ts lib)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/lib/utils.ts lib)
	//
	// # Package components
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/Button.tsx components)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/GaugeVertical.tsx components)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/Selector.tsx components)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/Switch.tsx components)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/ValueControl.tsx components)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/api/ValuePanel.tsx components)
	//
	// # Package solid-ui
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/components/ui/tooltip.tsx ui)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/src/components/ui/switch.tsx ui)
	//
	// # Package solid-ui-lib
	// mx_resource_append(${CMAKE_SOURCE_DIR}/frontend/lib/utils.ts uilib)
	//
	// # Package npm/yarn build configs
	// mx_resource_append(${CMAKE_SOURCE_DIR}/plug/package.json build)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/plug/tsconfig.json build)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/plug/vite.config.ts build)
	//
	// # Main plugin file (just a placeholder)
	// mx_resource_append(${CMAKE_SOURCE_DIR}/plug/plugin.tsx)
	
	auto prev_path = std::filesystem::current_path();
	std::filesystem::current_path(path);

	std::filesystem::create_directory("lib");
	PlugWriteFile("uilib::utils.ts", "lib/utils.ts");

	std::filesystem::create_directories("src/api");
	PlugWriteFile("components::Button.tsx", "src/api/Button.tsx");
	PlugWriteFile("components::GaugeVertical.tsx", "src/api/GaugeVertical.tsx");
	PlugWriteFile("components::Selector.tsx", "src/api/Selector.tsx");
	PlugWriteFile("components::Switch.tsx", "src/api/Switch.tsx");
	PlugWriteFile("components::ValueControl.tsx", "src/api/ValueControl.tsx");
	PlugWriteFile("components::ValuePanel.tsx", "src/api/ValuePanel.tsx");

	std::filesystem::create_directories("src/components/ui");
	PlugWriteFile("ui::tooltip.tsx", "src/components/ui/tooltip.tsx");
	PlugWriteFile("ui::switch.tsx", "src/components/ui/switch.tsx");

	std::filesystem::create_directories("src/lib");
	PlugWriteFile("lib::websocket.ts", "src/lib/websocket.ts");
	PlugWriteFile("lib::convert.ts", "src/lib/convert.ts");
	PlugWriteFile("lib::event.ts", "src/lib/event.ts");
	PlugWriteFile("lib::rpc.ts", "src/lib/rpc.ts");
	PlugWriteFile("lib::rdb.ts", "src/lib/rdb.ts");
	PlugWriteFile("lib::utils.ts", "src/lib/utils.ts");

	PlugWriteFile("build::package.json", "package.json");
	PlugWriteFile("build::tsconfig.json", "tsconfig.json");
	PlugWriteFile("build::vite.config.ts", "vite.config.ts");

	PlugWriteFile("plugin.tsx", "src/plugin.tsx");

	std::filesystem::current_path(prev_path);

	return true;
}

static bool PlugCreateWorkspace(const std::filesystem::path& path)
{
	if(!std::filesystem::exists(path))
	{
		if(!std::filesystem::create_directory(path))
		{
			LogError("[mxplug] Failed to create directory <%s> for new plugin workspace.", path.string().c_str());
			return false;
		}
	}
	else if(!std::filesystem::is_directory(path))
	{
		LogError("[mxplug] The specified path is not a directory.");
		return false;
	}
	else if(!std::filesystem::is_empty(path))
	{
		LogError("[mxplug] The specified directory is not empty.");
		return false;
	}

	// Start unpacking the project files into the specified path
	LogMessage("[mxplug] Creating plugin workspace under <%s>.", path.string().c_str());

	return PlugUnpackFiles(path);
}

int main(int argc, char* argv[])
{
	Mode mode = Mode::UNKNOWN;
	auto path = std::filesystem::current_path();

	SysAddArgument("new", 0, false, [&](const std::string&) {
		if(mode != Mode::UNKNOWN)
		{
			LogError("[mxplug] Cannot specify --new and --build at the same time.");
			::exit(0);
		}
		mode = Mode::NEW;
	}, "Create a new plugin project under the current directory (if empty).");

	SysAddArgument("build", 0, false, [&](const std::string&) {
		if(mode != Mode::UNKNOWN)
		{
			LogError("[mxplug] Cannot specify --new and --build at the same time.");
			::exit(0);
		}
		mode = Mode::BUILD;
	}, "Build the plugin under the working directory (if any).");

	SysAddArgument("cwd", 0, true, [&](const std::string& dir) {
		path = std::filesystem::path(dir);
	}, "Set the current working directory for mxplug.");

	if(!SysParseArguments(argc, argv))
	{
		LogError("[mxplug] Failed to parse arguments.");
		return 0;
	}

	// All arguments OK at this point
	LogTrace("[mxplug] Arguments OK.");

	switch(mode)
	{
		case Mode::NEW:
		{
			if(!PlugCreateWorkspace(path))
			{
				LogError("[mxplug] Failed to create new plugin workspace.");
				return 0;
			}
			break;
		}
		case Mode::BUILD:
			break;
		case Mode::UNKNOWN:
			return 0;
	}

	return 0;
}
