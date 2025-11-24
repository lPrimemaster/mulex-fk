// Brief  : MxFDB Mimics a very simple S3 like
// 			file database for backend and user
// 			upload control
// Author : César Godinho
// Date   : 30/05/25

#include "../mxrdb.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <random>
#include <sstream>
#include <iomanip>
#include <unordered_map>

static std::unordered_map<std::string, std::unique_ptr<mulex::SysBufferStack>> _fdb_chunk_stream;
static std::unordered_map<std::string, std::uint64_t> 		  				   _fdb_chunk_size_cache;
static std::unordered_map<std::string, std::string> 		  				   _fdb_chunk_filepath_cache;
static std::shared_mutex 							  		  				   _fdb_chunk_filepath_cache_lock;

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

	static bool FdbCacheTransferInit(const FdbHandle& handle, std::uint64_t chunksz)
	{
		std::string path = FdbGetFilePath(handle).c_str();

		if(path.empty())
		{
			LogError("[fdb] Failed to start cache transfer init. Maybe handle is invalid.");
			return false;
		}
		
		std::unique_lock lock(_fdb_chunk_filepath_cache_lock);
		bool valid_insert;
		std::tie(std::ignore, valid_insert) = _fdb_chunk_filepath_cache.emplace(handle.c_str(), path);
		_fdb_chunk_size_cache.emplace(handle.c_str(), chunksz);
		_fdb_chunk_stream.emplace(handle.c_str(), std::make_unique<SysBufferStack>());

		if(!valid_insert)
		{
			LogError("[fdb] A cache transfer for handle <%s> is in progress.", handle.c_str());
		}

		return valid_insert;
	}

	static void FdbCacheTransferFinalize(const FdbHandle& handle)
	{
		std::unique_lock lock(_fdb_chunk_filepath_cache_lock);
		_fdb_chunk_filepath_cache.erase(handle.c_str());
		_fdb_chunk_size_cache.erase(handle.c_str());
		_fdb_chunk_stream.erase(handle.c_str());
	}

	static std::uint64_t FdbCacheTransferGetChunkSize(const FdbHandle& handle)
	{
		std::shared_lock lock(_fdb_chunk_filepath_cache_lock);
		auto chunk_size = _fdb_chunk_size_cache.find(handle.c_str());
		if(chunk_size == _fdb_chunk_size_cache.end())
		{
			LogError("[fdb] No cache transfer in progress for handle <%s>.", handle.c_str());
			return 0;
		}

		return chunk_size->second;
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

	static SysBufferStack* FdbCacheTransferGetStream(const FdbHandle& handle)
	{
		std::shared_lock lock(_fdb_chunk_filepath_cache_lock);
		auto stream = _fdb_chunk_stream.find(handle.c_str());
		if(stream == _fdb_chunk_stream.end())
		{
			LogError("[fdb] No cache transfer in progress for handle <%s>.", handle.c_str());
			return nullptr;
		}

		return stream->second.get();
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

	std::string FdbGetMimeFromExt(const std::string& ext)
	{
		static const std::unordered_map<std::string, std::string> _extensions_to_mime = {
			{"jpg",  "image/jpeg"},
			{"png",  "image/png"},
			{"gif",  "image/gif"},
			{"webp", "image/webp"},
			{"svg",  "image/svg+xml"},
			{"bmp",  "image/bmp"},
			{"tiff", "image/tiff"},

			{"mp4",  "video/mp4"},
			{"webm", "video/webm"},
			{"ogv",  "video/ogg"},

			{"mp3",  "audio/mpeg"},
			{"ogg",  "audio/ogg"},
			{"wav",  "audio/wav"},
			{"weba", "audio/webm"},

			{"pdf",  "application/pdf"},
			{"zip",  "application/zip"},
			{"gz",   "application/gzip"},
			{"tar",  "application/x-tar"},
			{"json", "application/json"},
			{"js",   "application/javascript"},
			{"xml",  "application/xml"},
			{"bin",  "application/octet-stream"},

			{"txt",  "text/plain"},
			{"html", "text/html"},
			{"css",  "text/css"},
			{"csv",  "text/csv"}
		};

		auto mime = _extensions_to_mime.find(ext);
		if(mime != _extensions_to_mime.end())
		{
			return mime->second;
		}

		LogWarning("[fdb] Failed to infer file mime type for extension <%s>.", ext.c_str());
		LogWarning("[fdb] Using <application/octet-stream> as mime type.");
		return "application/octet-stream";
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
		LogWarning("[fdb] Using built-in value or <.bin> as extension.");
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

	inline static FdbHandle FdbWriteFile(const std::vector<std::uint8_t>& buffer, const string32& mimetype, const string32& extension)
	{
		FdbHandle handle = FdbGenerateUniqueHandle();
		const std::string bucket = FdbGetCurrentBucket();
		const std::string file_root = FdbGetBucketFullPath(bucket);
		std::string file_ext = FdbGetExtFromMime(mimetype.c_str());
		if(file_ext == "bin" && (strlen(extension.c_str()) > 0)) {
			file_ext = extension.c_str();
		}
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

	FdbHandle FdbChunkedUploadStart(mulex::string32 mimetype, mulex::string32 extension)
	{
		// NOTE: (Cesar) Touch the file
		FdbHandle handle = FdbWriteFile({}, mimetype, extension);
		FdbCacheTransferInit(handle, 0);
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

	bool FdbChunkedDownloadStart(mulex::PdbString handle, std::uint64_t chunksize)
	{
		if(!FdbCacheTransferInit(handle, chunksize))
		{
			return false;
		}

		// Start preloading chunks in the cache
		std::thread([handle, chunksize]() {
			SysBufferStack* stream = FdbCacheTransferGetStream(handle);
			std::string path = FdbCacheTransferGetPath(handle);
			
			std::ifstream file(path, std::ios::binary);
			std::vector<std::uint8_t> buffer(chunksize);

			if(!file)
			{
				LogError("[fdb] Failed to open file with handle <%s> for streaming.", handle.c_str());
				stream->requestUnblock();
				return;
			}

			while(file.read(reinterpret_cast<char*>(buffer.data()), chunksize) || file.gcount() > 0)
			{
				std::uint64_t size = file.gcount();
				std::vector<std::uint8_t> ibuf;
				std::uint8_t last = (size < chunksize) || file.eof() ? 1 : 0;

				ibuf.reserve(size + 1 + sizeof(uint64_t));
				buffer.resize(size);

				ibuf.push_back(last); // Last chunk flag
				for(int i = 0; i < 8; i++) ibuf.push_back(static_cast<std::uint8_t>(size >> (i * 8))); // Bytearray size
				ibuf.insert(ibuf.end(), buffer.begin(), buffer.end()); // Bytearray

				stream->push(ibuf);
			}
		}).detach();

		return true;
	}

	mulex::RPCGenericType FdbChunkedDownloadReceive(mulex::PdbString handle)
	{
		std::string path = FdbCacheTransferGetPath(handle);
		SysBufferStack* stream = FdbCacheTransferGetStream(handle);

		if(path.empty() || stream == nullptr)
		{
			LogError("[fdb] Failed to send chunk to client. Maybe did not call FdbChunkedDownloadStart or Chunked download ended.");
			return {};
		}
		// TODO: (Cesar) This won't work very well probably due to the nature of notify
		return stream->pop();
	}

	bool FdbChunkedDownloadEnd(mulex::PdbString handle)
	{
		SysBufferStack* stream = FdbCacheTransferGetStream(handle);
		if(stream != nullptr && stream->size() != 0)
		{
			LogError("[fdb] Donwload End was called, but chunks are still pending.");
			return false;
		}

		FdbCacheTransferFinalize(handle);
		return true;
	}

	bool FdbDeleteFile(mulex::PdbString handle)
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
		LogDebug("[fdb] Deleted file with handle <%s>.", handle.c_str());
		return true;
	}

	FdbPath FdbGetHandleRelativePath(mulex::PdbString handle)
	{
		FdbPath fullpath = FdbGetFilePath(handle);
		return std::filesystem::relative(fullpath.c_str(), SysGetExperimentHome()).string();
	}

	mulex::PdbString FdbGetFileTimestamp(mulex::PdbString handle)
	{
		static PdbAccessLocal accessor;
		static auto reader = accessor.getReader<PdbString>("storage_index", {"created_at"});
		auto timestamps = reader(std::string("WHERE id = '") + handle.c_str() + "'");

		if(timestamps.empty())
		{
			return "";
		}

		return std::get<0>(timestamps[0]);
	}

	void FdbClose()
	{
	}
} // namespace mulex
