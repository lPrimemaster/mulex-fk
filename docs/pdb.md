## Summary

The **P**ersistent **D**ata**B**ase (**PDB**) is a simple wrapper around a SQLite database present on the mx system.
The user is free to access this database and store long term on it if necessary.

## Using the PDB

As of the current version the PDB is only accessible via backends, using the C++ API. On the `mulex::MxBackend` context
there is a `pdb` field to access a `mulex::PdbAccessRemote` class.

#### `bool PdbAccess::createTable(const std::string& table, const std::initializer_list<std::string>& spec)`
Create a table named `table` under the main database for the experiment. `spec` is a list of all the modifiers given
to the database columns, i.e.:

```cpp
// Within MxBackend context
pdb.createTable(
    "mytable",
    {
        "id INTEGER PRIMARY KEY AUTOINCREMENT",
        "value TEXT NOT NULL"
    }
);
```

#### `template<PdbVariable... Vs> std::function<bool(const std::optional<Vs>&...)> getWriter(const std::string& table, const std::initializer_list<std::string>& names)`
Returns a writter function for the given table, allowing the user to write to the database.
Types need to be specified on the template when calling.
```cpp
auto writer = pdb.getWriter<int, PdbString>("mytable", {"id", "value"});

// Now we can write in the table
// std::nullopt is the equivalent of NULL in SQL here and since id is AUTOINCREMENT
writer(std::nullopt, "mystring");
writer(std::nullopt, "another column");
```

#### `template<PdbVariable... Vs> std::function<std::vector<std::tuple<Vs...>>(const std::string&)> getReader(const std::string& table, const std::initializer_list<std::string>& names)`
Returns a reader function for the given table, allowing the user to read from the database.
Types need to be specified on the template when calling.
```cpp
auto reader = pdb.getReader<int, PdbString>("table0", {"id", "value"});

// Get id 3
auto values = reader("WHERE id = 3"); // The reader function accepts conditions formatted SQL style
auto [id, value] = values[0];

// Get all ids
auto avalues = reader();
auto [id0, value0] = avalues[0];
auto [id1, value1] = avalues[1];
// ...
```
