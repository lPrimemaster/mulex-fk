#include "../mxsystem.h"
#include "plug.h"
#include <mxres.h>
#include <filesystem>
#include <unordered_map>

#include "../network/socket.h"

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
		{ "binary", "src/entry/" },
		{ "assets", "assets/" }
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

static bool PlugBuildPlugin(const std::string& exp, const std::filesystem::path& path, const std::string& host)
{
	// Copy locally
	if(host.empty())
	{
		const std::string exp_dir = std::string(SysGetCacheDir()) + "/" + exp;

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
	}

	// On remote build before checking to avoid timeouts

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

	// Copy locally
	if(host.empty())
	{
		const std::string exp_dir = std::string(SysGetCacheDir()) + "/" + exp;

		std::filesystem::copy(path / "dist", exp_dir + "/plugins", std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive);

		LogMessage("[mxplug] Plugin built for experiment.");
		LogMessage("[mxplug] Copied file under <%s>.", (exp_dir + "/plugins").c_str());
	}
	else
	{
		// Connect to remote
		Socket server = PlugFSConnectToServer(host);
		if(server._error)
		{
			LogError("[mxplug] Failed to connect to plugfs server at: %s.", host.c_str());
			return false;
		}

		// Check experiment exists
		if(!PlugFSCheckExperimentRemote(server, exp))
		{
			LogError("[mxplug] Failed to fetch experiment <%s> at plugfs server cache.", exp.c_str());
			PlugFSDisconnectFromServer(server);
			return false;
		}

		if(!PlugFSTransfer(server, path.string() + "/dist"))
		{
			LogError("[mxplug] Failed to transfer files to remote host experiment.");
			PlugFSDisconnectFromServer(server);
			return false;
		}

		PlugFSDisconnectFromServer(server);
	}
	return true;
}

int main(int argc, char* argv[])
{
	Mode mode = Mode::UNKNOWN;
	bool hotswap = false;
	std::string host;
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

	SysAddArgument("hotswap", 0, false, [&](const std::string&) {
		hotswap = true;
	}, "Enable hotswap. This builds and copies the plugin everytime a file is saved under cwd.");

	SysAddArgument("remote", 0, true, [&](const std::string& addr) {
		host = addr;
	}, "Setup a remote session. This performs all of the build/copy on the specified host instead.");

	if(!SysParseArguments(argc, argv))
	{
		LogError("[mxplug] Failed to parse arguments.");
		return 0;
	}

	// All arguments OK at this point
	LogTrace("[mxplug] Arguments OK.");

	if(hotswap)
	{
		if(mode != Mode::BUILD)
		{
			LogError("[mxplug] Hotswapping is only supported with the '--build' flag.");
			return 0;
		}
		
		auto watcher = std::make_unique<SysFileWatcher>(
			(path / "src").string(),
			[experiment, path, host](const SysFileWatcher::FileOp op, const std::string& file, const std::int64_t timestamp) {
				auto fpath = std::filesystem::path(file);

				// Skip directories
				if(std::filesystem::is_directory(fpath)) return;

				std::string filename = fpath.filename().string();

				switch(op)
				{
					case SysFileWatcher::FileOp::CREATED:
					{
						return;
					}
					case SysFileWatcher::FileOp::MODIFIED:
					{
						LogMessage("[mxplug] File modified: <%s>. Rebuilding...", filename.c_str());
						break;
					}
					case SysFileWatcher::FileOp::DELETED:
					{
						LogMessage("[mxplug] File deletion detected: <%s>. Rebuilding...", filename.c_str());
						break;
					}
				}

				if(!PlugBuildPlugin(experiment, path, host))
				{
					LogError("[mxplug] Failed to build plugin.");
				}
			},
			250
		);

		const std::atomic<bool>* stop = mulex::SysSetupExitSignal();

		LogMessage("[mxplug] Running hotswap for dir: <%s>.", path.c_str());
		LogMessage("[mxplug] Consider using daemon mode '-D' if you want to continue using this shell.");
		LogMessage("[mxplug] Press ctrl-C to exit.");

		while(!*stop)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		LogMessage("[mxplug] ctrl-C detected. Exiting...");

		return 0;
	}

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
			if(!PlugBuildPlugin(experiment, path, host))
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
