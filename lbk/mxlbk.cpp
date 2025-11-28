#include "../mxlbk.h"
#include <string>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#endif

namespace mulex
{
	static void LbkInitTables()
	{
		ZoneScoped;
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

		const std::string posts_fts5 =
		"CREATE VIRTUAL TABLE IF NOT EXISTS posts_fts USING FTS5("
			"author, title, mdbody, created_at, modified_at,"
			"content='posts', content_rowid='id'"
		");";
		PdbExecuteQuery(posts_fts5);

		const std::string posts_trigger_insert =
		"CREATE TRIGGER IF NOT EXISTS posts_ti AFTER INSERT ON posts BEGIN\n"
			"\tINSERT INTO posts_fts(rowid, author, title, mdbody, created_at, modified_at)\n"
			"\tVALUES(new.id, new.author, new.title, new.mdbody, new.created_at, new.modified_at);\n"
		"END;";
		PdbExecuteQuery(posts_trigger_insert);

		const std::string posts_trigger_update =
		"CREATE TRIGGER IF NOT EXISTS posts_tu AFTER UPDATE ON posts BEGIN\n"
			"\tUPDATE posts_fts SET\n"
				"\t\tauthor = new.author,\n"
				"\t\ttitle = new.title,\n"
				"\t\tmdbody = new.mdbody,\n"
				"\t\tcreated_at = new.created_at,\n"
				"\t\tmodified_at = new.modified_at\n"
			"\tWHERE rowid = new.id;\n"
		"END;";
		PdbExecuteQuery(posts_trigger_update);

		const std::string posts_trigger_delete =
		"CREATE TRIGGER IF NOT EXISTS posts_td AFTER DELETE ON posts BEGIN\n"
			"\tDELETE FROM posts_fts WHERE rowid = old.id;\n"
		"END;";
		PdbExecuteQuery(posts_trigger_delete);

		const std::string comments =
		"CREATE TABLE IF NOT EXISTS comments ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"author INTEGER NOT NULL,"
			"post_id INTEGER NOT NULL,"
			"body TEXT NOT NULL,"
			"created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
			"FOREIGN KEY(post_id) REFERENCES posts(id) ON DELETE CASCADE"
		");";
		PdbExecuteQuery(comments);
	}

	void LbkInit()
	{
		ZoneScoped;
		LbkInitTables();
	}

	void LbkClose()
	{
		ZoneScoped;
	}

	bool LbkPostCreate(mulex::PdbString title, mulex::RPCGenericType body, mulex::RPCGenericType meta)
	{
		ZoneScoped;
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

	bool LbkPostDelete(std::int32_t id)
	{
		ZoneScoped;
		static PdbAccessLocal accessor;
		static auto deleter = accessor.getDeleter("posts");
		static auto reader = accessor.getReader<std::int32_t>("posts", {"id"});

		auto postid = reader("WHERE id = " + std::to_string(id));
		if(postid.empty())
		{
			LogError("[lbk] Failed to find and delete post with id <%d>.", id);
			return false;
		}

		if(deleter("WHERE id = " + std::to_string(id)))
		{
			LogDebug("[lbk] Deleted post <%d>.", id);
			return true;
		}
		
		LogError("[lbk] Failed to delete post <%d>.", id);
		return false;
	}

	mulex::RPCGenericType LbkPostRead(std::int32_t id)
	{
		ZoneScoped;
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

	mulex::RPCGenericType LbkGetEntriesPageSearch(mulex::PdbString query, std::uint64_t limit, std::uint64_t page)
	{
		ZoneScoped;
		std::string fquery = 
		"SELECT posts.id, users.username, posts.title, posts.created_at, posts.metadata FROM posts_fts "
		"JOIN posts ON posts_fts.rowid = posts.id "
		"JOIN users ON posts.author = users.id "
		"WHERE posts_fts MATCH '";
		fquery += query.c_str();
		fquery += "' ORDER BY posts.created_at DESC LIMIT ";
		fquery += std::to_string(limit) + " OFFSET (";
		fquery += std::to_string(page) + " * " + std::to_string(limit) + ");";

		static const std::vector<PdbValueType> types = {
			PdbValueType::INT32,
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::BINARY
		};
	
		return PdbReadTable(fquery, types);
	}

	mulex::RPCGenericType LbkGetEntriesPage(std::uint64_t limit, std::uint64_t page)
	{
		ZoneScoped;
		static const std::vector<PdbValueType> types = {
			PdbValueType::INT32,
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::BINARY
		};

		std::string query = 
			"SELECT posts.id, users.username, posts.title, posts.created_at, posts.metadata "
			"FROM posts JOIN users ON posts.author = users.id ORDER BY posts.created_at DESC LIMIT "
			+ std::to_string(limit) + " OFFSET (" + std::to_string(page)
			+ " * " + std::to_string(limit) + ");";

		const std::vector<std::uint8_t> data = PdbReadTable(query, types);
		return data;
	}

	std::int64_t LbkGetNumEntriesWithCondition(mulex::PdbString query)
	{
		ZoneScoped;
		std::string squery = query.c_str();
		if(squery.empty())
		{
			const auto data = PdbReadTable("SELECT COUNT(*) FROM posts;", std::vector<PdbValueType>{ PdbValueType::INT64 });
			return data;
		}
		else
		{
			std::string fquery = 
			"SELECT COUNT(*) FROM posts_fts "
			"JOIN posts ON posts_fts.rowid = posts.id "
			"WHERE posts_fts MATCH '";
			fquery += squery + "';";
			const auto data = PdbReadTable(fquery, std::vector<PdbValueType>{ PdbValueType::INT64 });
			return data;
		}
	}

	mulex::RPCGenericType LbkGetComments(std::int32_t postid, std::uint64_t limit, std::uint64_t page)
	{
		ZoneScoped;
		static const std::vector<PdbValueType> types = {
			PdbValueType::STRING,
			PdbValueType::CSTRING,
			PdbValueType::STRING
		};

		std::string query = 
			"SELECT users.username, comments.body, comments.created_at "
			"FROM comments JOIN users ON comments.author = users.id WHERE comments.post_id = " +
			std::to_string(postid) + " "
			" ORDER BY comments.created_at DESC LIMIT "
			+ std::to_string(limit) + " OFFSET (" + std::to_string(page)
			+ " * " + std::to_string(limit) + ");";

		const std::vector<std::uint8_t> data = PdbReadTable(query, types);
		return data;
	}

	std::int64_t LbkGetNumComments(std::int32_t postid)
	{
		ZoneScoped;
		std::string query = "SELECT COUNT(*) FROM comments WHERE post_id = " + std::to_string(postid) + ";";
		const auto data = PdbReadTable(query, std::vector<PdbValueType>{ PdbValueType::INT64 });
		return data;
	}

	bool LbkCommentCreate(std::int32_t postid, mulex::RPCGenericType body)
	{
		ZoneScoped;
		std::string current_user = GetCurrentCallerUser();
		if(current_user.empty())
		{
			LogError("[pdb] Only a user can create a comment.");
			return false;
		}

		std::int32_t user_id = PdbGetUserId(current_user);
		if(user_id == -1)
		{
			return false;
		}


		const std::vector<std::uint8_t> bodyBytes = body;
		const std::string sbody(bodyBytes.begin(), bodyBytes.end());
		if(sbody.empty())
		{
			LogError("[lbk] Failed to create comment. Body cannot be empty.");
			return false;
		}

		static PdbAccessLocal accessor;
		static auto writer = accessor.getWriter<
			std::int32_t,
			std::int32_t,
			std::int32_t,
			std::string
		>("comments", {
			"id",
			"author",
			"post_id",
			"body"
		});

		bool ok = writer(
			std::nullopt,
			user_id,
			postid,
			sbody
		);

		if(!ok)
		{
			LogError("[lbk] Failed to create comment.");
			return false;
		}

		LogDebug("[lbk] Created new comment.");
		return true;
	}
}
