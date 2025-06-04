#include "../mxlbk.h"
#include <string>

namespace mulex
{
	static void LbkInitTables()
	{
		const std::string posts = 
		"CREATE TABLE IF NOT EXISTS posts ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"author INTEGER NOT NULL,"
			"title TEXT NOT NULL,"
			"mdbody TEXT NOT NULL,"
			"created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
			"modified_at DATETIME,"
			"is_modified BOOLEAN DEFAULT FALSE,"
			"metadata BLOB,"
			"is_pinned BOOLEAN DEFAULT FALSE,"
			"FOREIGN KEY(author) REFERENCES users(id)"
		");";
		PdbExecuteQuery(posts);

		const std::string comments = 
		"CREATE TABLE IF NOT EXISTS comments ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"author INTEGER NOT NULL,"
			"post_id INTEGER NOT NULL,"
			"body TEXT NOT NULL,"
			"created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
			"FOREIGN KEY(post_id) REFERENCES posts(id)"
		");";
		PdbExecuteQuery(comments);
	}

	void LbkInit()
	{
		LbkInitTables();
	}

	void LbkClose()
	{
	}

	bool LbkPostCreate(mulex::PdbString title, mulex::RPCGenericType body, mulex::RPCGenericType meta)
	{
		std::string current_user = GetCurrentCallerUser();
		if(current_user.empty())
		{
			LogError("[pdb] Only a user can create a post.");
			return false;
		}

		std::int32_t user_id = PdbGetUserId(current_user);
		if(user_id == -1)
		{
			return false;
		}

		const std::string stitle = title.c_str();

		const std::vector<std::uint8_t> bodyBytes = body;
		const std::string sbody(bodyBytes.begin(), bodyBytes.end());

		const std::vector<std::uint8_t> metaBytes = meta;
		const std::string smeta(metaBytes.begin(), metaBytes.end());

		if(stitle.empty() || sbody.empty())
		{
			LogError("[lbk] Failed to create post. Title and/or body cannot be empty.");
			return false;
		}

		static PdbAccessLocal accessor;
		static auto writer = accessor.getWriter<
			std::int32_t,
			std::int32_t,
			PdbString,
			std::string,
			std::vector<std::uint8_t>
		>("posts", {
			"id",
			"author",
			"title",
			"mdbody",
			"metadata",
		});

		bool ok = writer(
			std::nullopt,
			user_id,
			title,
			sbody,
			metaBytes
		);

		if(!ok)
		{
			LogError("[lbk] Failed to create post.");
			return false;
		}

		LogDebug("[lbk] Created new post.");
		return true;
	}

	mulex::RPCGenericType LbkPostRead(std::int32_t id)
	{
		static PdbAccessLocal accessor;
		static auto reader = accessor.getReader<std::string>("posts", {"mdbody"});
		auto mdbody = reader("WHERE id = " + std::to_string(id));
		if(mdbody.empty())
		{
			LogError("[lbk] Failed to read post with id <%d>.", id);
			return {};
		}

		std::string content = std::get<0>(mdbody[0]);
		std::vector<std::uint8_t> raw(content.begin(), content.end());
		raw.push_back(0);
		return raw;
	}

	mulex::RPCGenericType LbkSearch(mulex::PdbString query)
	{
		return {};
	}

	mulex::RPCGenericType LbkGetEntriesPage(std::uint64_t limit, std::uint64_t page)
	{
		static const std::vector<PdbValueType> types = {
			PdbValueType::INT32,
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::BINARY
		};

		std::string query = 
			"SELECT posts.id, users.username, posts.title, posts.created_at, posts.metadata "
			"FROM posts JOIN users ON posts.author = users.id LIMIT "
			+ std::to_string(limit) + " OFFSET (" + std::to_string(page)
			+ " * " + std::to_string(limit) + ");";

		const std::vector<std::uint8_t> data = PdbReadTable(query, types);
		return data;
	}
}
