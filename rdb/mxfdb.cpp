// Brief  : MxFDB Mimics a very simple S3 like
// 			file database for backend and user
// 			upload control
// Author : César Godinho
// Date   : 30/05/25

#include "../mxrdb.h"
#include <chrono>
#include <filesystem>
#include <random>
#include <sstream>
#include <iomanip>
#include <unordered_map>

static std::unordered_map<std::string, std::string> _fdb_chunk_filepath_cache;
static std::shared_mutex 							_fdb_chunk_filepath_cache_lock;

namespace mulex
{
	// NOTE: (Cesar) Based on uuid v4
	//				 Not a fast implementation
	//				 Change if speed becomes relevant
	//				 One could eventually use 128bit ints
	static FdbHandle FdbGenerateUniqueHandle()
	{
		std::random_device rd;
		std::mt19937 generator(rd());
		std::uniform_int_distribution<std::uint32_t> distribution(0, 0xFFFFFFFF);
		static constexpr std::int32_t UUID_SIZE = 16;
		std::uint8_t bytes[UUID_SIZE];
		
		for(std::int32_t i = 0; i < UUID_SIZE; i+= 4)
		{
			std::uint32_t value = distribution(generator);
			bytes[i    ] = (value >> 24) & 0xFF;
			bytes[i + 1] = (value >> 16) & 0xFF;
			bytes[i + 2] = (value >>  8) & 0xFF;
			bytes[i + 3] = (value	   ) & 0xFF;
		}

		bytes[6] = (bytes[6] & 0x0F) | 0x40;
		bytes[8] = (bytes[8] & 0x3F) | 0x80;

		std::stringstream ss;
		ss << std::hex << std::setfill('0');
		for(std::int32_t i = 0; i < UUID_SIZE; i++)
		{
			ss << std::setw(2) << static_cast<std::int32_t>(bytes[i]);
			if(i == 3 || i == 5 || i == 7 || i == 9) ss << "-";
		}

		return ss.str();
	}

	static void FdbCacheTransferInit(const FdbHandle& handle)
	{
		std::string path = FdbGetFilePath(handle).c_str();

		if(path.empty())
		{
			LogError("[fdb] Failed to start cache transfer init. Maybe handle is invalid.");
			return;
		}
		
		std::unique_lock lock(_fdb_chunk_filepath_cache_lock);
		bool valid_insert;
		std::tie(std::ignore, valid_insert) = _fdb_chunk_filepath_cache.emplace(handle.c_str(), path);

		if(!valid_insert)
		{
			LogError("[fdb] A cache transfer for handle <%s> is in progress.", handle.c_str());
		}
	}

	static void FdbCacheTransferFinalize(const FdbHandle& handle)
	{
		std::unique_lock lock(_fdb_chunk_filepath_cache_lock);
		_fdb_chunk_filepath_cache.erase(handle.c_str());
	}

	static std::string FdbCacheTransferGetPath(const FdbHandle& handle)
	{
		std::shared_lock lock(_fdb_chunk_filepath_cache_lock);
		auto file_path = _fdb_chunk_filepath_cache.find(handle.c_str());
		if(file_path == _fdb_chunk_filepath_cache.end())
		{
			LogError("[fdb] No cache transfer in progress for handle <%s>.", handle.c_str());
			return "";
		}

		return file_path->second;
	}

	static void FdbInitTables()
	{
		const std::string query = 
		"CREATE TABLE IF NOT EXISTS storage_index ("
			"id TEXT PRIMARY KEY,"
			"bucket TEXT NOT NULL,"
			"key TEXT NOT NULL,"
			"size BIGINT,"
			"mime_type TEXT NOT NULL,"
			"created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
			"path TEXT NOT NULL,"
			"owner_id INTEGER NULL,"
			"FOREIGN KEY(owner_id) REFERENCES users(id)"
		");";
		PdbExecuteQuery(query);
	}

	// Retrieves bucket based on month/year
	static std::string FdbGetCurrentBucket()
	{
		std::stringstream ss;
		auto now = std::chrono::system_clock::now();
		std::chrono::year_month_day days = std::chrono::floor<std::chrono::days>(now);
		std::int32_t year  =  static_cast<std::int32_t>(days.year());
		std::uint32_t month =  static_cast<std::uint32_t>(days.month());
		ss << year << "_" << std::setw(2) << std::setfill('0') << month;
		return ss.str();
	}

	static std::string FdbGetBucketFullPath(const std::string& bucket)
	{
		const std::string path = std::string(SysGetExperimentHome()) + "/.storage/buckets/" + bucket;
		std::filesystem::create_directories(path);
		return path;
	}

	static std::string FdbGetExtFromMime(const std::string& mimetype)
	{
		static const std::unordered_map<std::string, std::string> _mime_types = {
			{"image/jpeg", "jpg"},
			{"image/png", "png"},
			{"image/gif", "gif"},
			{"image/webp", "webp"},
			{"image/svg+xml", "svg"},
			{"image/bmp", "bmp"},
			{"image/tiff", "tiff"},

			{"video/mp4", "mp4"},
			{"video/webm", "webm"},
			{"video/ogg", "ogv"},

			{"audio/mpeg", "mp3"},
			{"audio/ogg", "ogg"},
			{"audio/wav", "wav"},
			{"audio/webm", "weba"},

			{"application/pdf", "pdf"},
			{"application/zip", "zip"},
			{"application/gzip", "gz"},
			{"application/x-tar", "tar"},
			{"application/json", "json"},
			{"application/javascript", "js"},
			{"application/xml", "xml"},
			{"application/octet-stream", "bin"},

			{"text/plain", "txt"},
			{"text/html", "html"},
			{"text/css", "css"},
			{"text/csv", "csv"},
		};

		auto ext = _mime_types.find(mimetype);
		if(ext != _mime_types.end())
		{
			return ext->second;
		}

		LogWarning("[fdb] Failed to infer file extension for mime type <%s>.", mimetype.c_str());
		LogWarning("[fdb] Using <.bin> as extension.");
		return "bin";
	}

	static void FdbLogFileEntry(
		const FdbHandle& handle,
		const std::string& bucket,
		const std::string& path,
		const std::string& key,
		std::uint64_t size,
		const std::string& mimetype,
		const std::optional<std::int32_t> owner
	)
	{
		std::vector<PdbValueType> types = {
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::UINT64,
			PdbValueType::STRING,
			PdbValueType::STRING
		};

		if(owner.has_value())
		{
			types.push_back(PdbValueType::INT32);
		}
		else
		{
			types.push_back(PdbValueType::NIL);
		}

		std::vector<std::uint8_t> data = SysPackArguments(
			PdbString(handle),
			PdbString(bucket),
			PdbString(key),
			size,
			PdbString(mimetype),
			PdbString(path)
		);

		if(owner.has_value())
		{
			SysPackArguments(data, owner.value());
		}

		if(PdbWriteTable("INSERT INTO storage_index (id, bucket, key, size, mime_type, path, owner_id) VALUES (?, ?, ?, ?, ?, ?, ?);", types, data))
		{
			LogDebug("[fdb] Created new storage index resource (handle <%s>).", handle.c_str());
		}
		else
		{
			LogDebug("[fdb] Failed to create storage index resource (handle <%s>).", handle.c_str());
		}
	}

	FdbPath FdbGetFilePath(const FdbHandle& handle)
	{
		static PdbAccessLocal accessor;
		static auto reader = accessor.getReader<PdbString>("storage_index", {"path"});
		auto paths = reader(std::string("WHERE id = '") + handle.c_str() + "'");

		if(paths.empty())
		{
			return "";
		}

		return std::get<0>(paths[0]);
	}

	bool FdbCheckHandle(const FdbHandle& handle)
	{
		static PdbAccessLocal accessor;
		static auto reader = accessor.getReader<PdbString>("storage_index", {"id"});
		auto paths = reader(std::string("WHERE id = '") + handle.c_str() + "'");
		return !paths.empty();
	}

	static void FdbComputeNewFileSizeLog(const FdbHandle& handle, const std::string& path)
	{
		std::error_code ec;
		std::uint64_t file_size =  std::filesystem::file_size(path, ec);
		if(ec)
		{
			LogError("[fdb] Failed to fetch file size for handle <%s>.", handle.c_str());
			LogError("[fdb] Given path <%s>.", path.c_str());
			return;
		}

		// NOTE: (Cesar) Cannot use PdbAccessLocal there is no way to update columns
		// TODO: (Cesar) Add this feature to PdbAccess
		if(!PdbExecuteQueryUnrestricted(
			"UPDATE storage_index SET size = " + std::to_string(file_size) + " WHERE id = '" + handle.c_str() + "';"
		))
		{
			LogError("[fdb] Failed to update file size for handle <%s>.", handle.c_str());
		}
	}

	void FdbInit()
	{
		// Create the default database files directory
		FdbInitTables();
	}

	inline static FdbHandle FdbWriteFile(const std::vector<std::uint8_t>& buffer, string32& mimetype)
	{
		FdbHandle handle = FdbGenerateUniqueHandle();
		const std::string bucket = FdbGetCurrentBucket();
		const std::string file_root = FdbGetBucketFullPath(bucket);
		const std::string file_ext = FdbGetExtFromMime(mimetype.c_str());
		const std::string file_key = std::string(handle.c_str()) + "." + file_ext;
		const std::string full_sys_path = file_root + "/" + file_key;
		const std::string username = GetCurrentCallerUser();
		std::optional<std::int32_t> owner = std::nullopt;

		if(!username.empty())
		{
			std::int32_t id = PdbGetUserId(username);
			if(id >= 0)
			{
				owner = id;
			}
		}

		FdbLogFileEntry(handle, bucket, full_sys_path, file_key, buffer.size(), mimetype.c_str(), owner);

		LogDebug("[fdb] Writing uploaded file. Handle <%s>.", handle.c_str());
		SysWriteBinFile(full_sys_path, buffer);

		return handle;
	}

	inline static void FdbAppendFile(const FdbPath& path, const std::vector<std::uint8_t>& buffer)
	{
		LogTrace("[fdb] Writing chunk (%llu KB).", buffer.size() / 1024);
		SysAppendBinFile(path.c_str(), buffer);
	}

	FdbHandle FdbUploadFile(mulex::RPCGenericType data, mulex::string32 mimetype)
	{
		return FdbWriteFile(data, mimetype);
	}

	FdbHandle FdbChunkedUploadStart(mulex::string32 mimetype)
	{
		// NOTE: (Cesar) Touch the file
		FdbHandle handle = FdbWriteFile({}, mimetype);
		FdbCacheTransferInit(handle);
		return handle;
	}

	bool FdbChunkedUploadSend(mulex::PdbString handle, mulex::RPCGenericType chunk)
	{
		const std::string file_path = FdbCacheTransferGetPath(handle);
		if(file_path.empty())
		{
			LogError("[fdb] Failed to upload chunk to handle <%s>.", handle.c_str());
			return false;
		}

		FdbAppendFile(file_path, chunk);
		return true;
	}

	bool FdbChunkedUploadEnd(mulex::PdbString handle)
	{
		const std::string path = FdbCacheTransferGetPath(handle);
		if(path.empty())
		{
			return false;
		}

		FdbComputeNewFileSizeLog(handle, path);
		FdbCacheTransferFinalize(handle);
		return true;
	}

	bool FdbDeleteFile(mulex::FdbHandle handle)
	{
		std::string file_path = FdbGetFilePath(handle).c_str();
		if(file_path.empty())
		{
			LogError("[fdb] Failed to delete file with handle <%s>.", handle.c_str());
			return false;
		}

		if(!PdbExecuteQueryUnrestricted(std::string("DELETE FROM storage_index WHERE id = '") + handle.c_str() + "';"))
		{
			LogError("[fdb] Failed to delete file with handle <%s>.", handle.c_str());
			return false;
		}
		
		SysDeleteFile(file_path);
		return true;
	}

	FdbPath FdbGetHandleRelativePath(mulex::FdbHandle handle)
	{
		FdbPath fullpath = FdbGetFilePath(handle);
		return std::filesystem::relative(fullpath.c_str(), SysGetExperimentHome()).string();
	}

	void FdbClose()
	{
	}
} // namespace mulex
