#include "../mxsystem.h"
#include <mxres.h>
#include <filesystem>
#include <unordered_map>

using namespace mulex;

enum class Mode
{
	NEW,
	BUILD,
	UNKNOWN
};

inline static std::pair<std::string, std::string> PlugGetResourceInfo(const std::string& resource)
{
	auto index = resource.find_last_of(':');
	return std::make_pair(resource.substr(0, index - 1), resource.substr(index + 1));
}

static void PlugWriteFiles(const std::unordered_map<std::string, std::string>& dirs)
{
	for(const auto& dir : dirs)
	{
		if(dir.second.empty()) continue; // Skip if dir is cwd basically
		std::filesystem::create_directories(dir.second);
	}

	for(const auto& [name, data] : ResGetAll())
	{
		const auto [nspace, filename] = PlugGetResourceInfo(name);
		std::string path = dirs.at(nspace) + filename;
		LogMessage("[mxplug] Writing <%s>...", path.c_str());
		SysWriteBinFile(path, data);
	}
}

static bool PlugUnpackFiles(const std::filesystem::path& path)
{
	LogMessage("[mxplug] Unpacking files...");
	
	auto prev_path = std::filesystem::current_path();
	std::filesystem::current_path(path);

	PlugWriteFiles({
		{ "uilib", "lib/" },
		{ "components", "src/api/" },
		{ "ui", "src/components/ui/" },
		{ "lib", "src/lib/" },
		{ "build", "" },
		{ "binary", "src/" }
	});

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

static bool PlugBuildPlugin(const std::string& exp, const std::filesystem::path& path)
{
	std::string exp_dir = std::string(SysGetCacheDir()) + "/" + exp;

	if(!std::filesystem::is_directory(exp_dir))
	{
		LogError("[mxplug] The specified experiment could no be found under mxcache.");
		LogError("[mxplug] Looked under <%s>.", exp_dir.c_str());
		return false;
	}

	if(!std::filesystem::is_directory(exp_dir + "/plugins"))
	{
		LogError("[mxplug] Found experiment directory but could not find /plugins directory inside.");
		LogError("[mxplug] Looked under <%s>.", exp_dir.c_str());
		return false;
	}

	if(!std::filesystem::is_regular_file(path / "package.json"))
	{
		LogError("[mxplug] No plugin found to build under <%s>.", path.c_str());
		return false;
	}

	if(!std::filesystem::is_directory(path / "node_modules"))
	{
		LogMessage("[mxplug] node_modules not found. Configuring dependencies...");
		if(std::system((std::string("yarn --cwd ") + path.string()).c_str()) != 0)
		{
			LogError("[mxplug] Yarn configure command returned with non zero.");
			LogError("[mxplug] Check yarn output.");
			return false;
		}
	}

	if(std::system((std::string("yarn --cwd ") + path.string() + " build").c_str()) != 0)
	{
		LogError("[mxplug] Yarn build command returned with non zero.");
		LogError("[mxplug] Check yarn output.");
		return false;
	}

	std::filesystem::copy(path / "dist", exp_dir + "/plugins", std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive);

	LogMessage("[mxplug] Plugin built for experiment.");
	LogMessage("[mxplug] Copied file under <%s>.", (exp_dir + "/plugins").c_str());
	return true;
}

int main(int argc, char* argv[])
{
	Mode mode = Mode::UNKNOWN;
	auto path = std::filesystem::current_path();
	std::string experiment;

	SysAddArgument("new", 0, false, [&](const std::string&) {
		if(mode != Mode::UNKNOWN)
		{
			LogError("[mxplug] Cannot specify --new and --build at the same time.");
			::exit(0);
		}
		mode = Mode::NEW;
	}, "Create a new plugin project under the current directory (if empty).");

	SysAddArgument("build", 0, true, [&](const std::string& exp) {
		if(mode != Mode::UNKNOWN)
		{
			LogError("[mxplug] Cannot specify --new and --build at the same time.");
			::exit(0);
		}

		experiment = exp;
		mode = Mode::BUILD;
	}, "Build the plugin under for the given experiment.");

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
		{
			if(!PlugBuildPlugin(experiment, path))
			{
				LogError("[mxplug] Failed to build plugin.");
				return 0;
			}
			break;
		}
		case Mode::UNKNOWN:
			return 0;
	}

	return 0;
}
