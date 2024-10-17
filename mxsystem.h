#pragma once
#include <vector>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include "rpc/socket.h"
#include "mxtypes.h"
#include "mxlogger.h"

namespace mulex
{
	template<typename T>
	inline void SysPackArguments(std::vector<std::uint8_t>& buffer, T& t)
	{
		static_assert(
			std::is_trivially_copyable_v<T>,
			"SysPackArguments requires trivially copyable arguments."
		);
		std::uint8_t ibuf[sizeof(T)];
		*reinterpret_cast<T*>(ibuf) = t; // NOTE: Copy constructor
		buffer.insert(buffer.end(), ibuf, ibuf + sizeof(T));
	}

	template<typename ...Args>
	inline std::vector<std::uint8_t> SysPackArguments(Args&... args)
	{
		std::vector<std::uint8_t> buffer;
		(SysPackArguments(buffer, args), ...);
		return buffer;
	}

	template<typename T>
	inline constexpr std::size_t SysVargSize()
	{
		return sizeof(T);
	}

	template<typename T, typename U, typename ...Args>
	inline constexpr std::size_t SysVargSize()
	{
		return sizeof(T) + SysVargSize<U, Args...>();
	}

	struct Experiment
	{
		Socket _exp_socket;
		Socket _rpc_socket;
	};

	static constexpr std::uint16_t EXP_DEFAULT_PORT = 5700;

	std::optional<const Experiment*> SysGetConnectedExperiment();
	bool SysConnectToExperiment(const char* hostname, std::uint16_t port = EXP_DEFAULT_PORT);
	void SysDisconnectFromExperiment();

	static constexpr std::uint64_t RDB_MAX_KEY_SIZE = 512;
	static constexpr std::uint64_t RDB_MAX_STRING_SIZE = 512;

	using RdbKeyName = mxstring<RDB_MAX_KEY_SIZE>;

	struct RdbKey
	{
		RdbKeyName _name;
	};

	enum class RdbValueType : std::uint8_t
	{
		INT8,
		INT16,
		INT32,
		INT64,
		UINT8,
		UINT16,
		UINT32,
		UINT64,
		FLOAT32,
		FLOAT64,
		STRING
	};

	struct RdbValue
	{
		RdbValueType  _type;
		std::uint64_t _size;
		std::uint64_t _count;
		std::uint8_t  _ptr[];

		template<typename T>
		constexpr inline T as() const
		{
			if constexpr(std::is_pointer_v<T>)
			{
				// Arrays
				if(_count == 0)
				{
					LogError("RdbValue::as() called with pointer type but its value is not an array.");
					return nullptr;
				}
				return reinterpret_cast<T>(_ptr);
			}
			else
			{
				// Non arrays
				if(_count > 0)
				{
					LogError("RdbValue::as() called with non pointer type but its value is an array.");
					return T();
				}
				return *reinterpret_cast<T*>(_ptr);
			}
		}
	};

	struct RdbEntry
	{
		// Entry locking
		mutable std::shared_mutex _rw_lock;

		// Entry data
		RdbKey   _key;
		RdbValue _value;
	};

	void RdbInit(std::uint64_t size);
	void RdbClose();

	// TODO: (Cesar) Implement this
	bool RdbImportFromSQL(const std::string& filename);

	RdbEntry* RdbNewEntry(const RdbKeyName& key, const RdbValueType& type, void* data, std::uint64_t count = 0);
	bool RdbDeleteEntry(const RdbKeyName& key);
	RdbEntry* RdbFindEntryByName(const RdbKeyName& key);

	MX_RPC_RAW_METHOD std::vector<std::uint8_t> RdbReadValueDirect(mulex::RdbKeyName keyname);
	MX_RPC_METHOD void RdbWriteValueDirect(mulex::RdbKeyName keyname, std::vector<std::uint8_t> data);

	bool RdbKeyIsNotLeaf(const RdbKeyName& keyname);

	void RdbLockEntryRead(const RdbEntry& entry);
	void RdbLockEntryReadWrite(const RdbEntry& entry);
	void RdbUnlockEntryRead(const RdbEntry& entry);
	void RdbUnlockEntryReadWrite(const RdbEntry& entry);

	class RdbAccess
	{
	public:
		RdbAccess(const std::string& rootkey = "");
		
		template<typename T>
		inline T operator[](const std::string& key)
		{
		}
	};
} // namespace mulex
