#pragma once

#include<string>
#include<stdint.h>
#include "dbformat.h"
#include "cache.h"
#include "table.h"
#include "option.h"

class TableCache {
public:
	TableCache(const std::string& dbname, const Options& options, int entries);

	~TableCache();

	// Return an iterator for the specified file number (the corresponding
	// file length must be exactly "file_size" bytes).  If "tableptr" is
	// non-null, also sets "*tableptr" to point to the Table object
	// underlying the returned iterator, or to nullptr if no Table object
	// underlies the returned iterator.  The returned "*tableptr" object is owned
	// by the cache and should not be deleted, and is valid for as long as the
	// returned iterator is live.
	std::shared_ptr<Iterator> newIterator(const ReadOptions& options,
		uint64_t fileNumber,
		uint64_t fileSize,
		std::shared_ptr<Table> tableptr = nullptr);

	// If a seek to internal key "k" in specified file finds an entry,
	// call (*handle_result)(arg, found_key, found_value).
	Status get(const ReadOptions& options,
		uint64_t fileNumber,
		uint64_t fileSize,
		const std::string_view& k,
		const std::any& arg,
		std::function<void(const std::any&,
			const std::string_view&, const std::string_view&)>&& callback);

	Status findTable(uint64_t fileNumber, uint64_t fileSize,
		std::shared_ptr<LRUHandle>& handle);

	std::shared_ptr<ShardedLRUCache> getCache() { return cache; }

	void evict(uint64_t fileNumber);

	std::shared_ptr<Iterator> getFileIterator(const ReadOptions& options, const std::string_view& fileValue);

private:
	TableCache(const TableCache&) = delete;

	void operator=(const TableCache&) = delete;

	std::string dbname;
	const Options options;
	std::shared_ptr<ShardedLRUCache> cache;
};
