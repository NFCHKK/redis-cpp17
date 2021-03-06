#pragma once

#include<map>
#include<set>
#include<vector>
#include<list>
#include<deque>
#include<assert.h>
#include "logwriter.h"
#include "tablecache.h"
#include "versionedit.h"
#include "option.h"
#include "dbformat.h"
#include "env.h"

class VersionSet;

class Compaction;

class Version {
public:
	Version(VersionSet* vset)
		: vset(vset) {

	}

	// Append to *iters a sequence of iterators that will
	// yield the contents of this Version when merged together.
	// REQUIRES: This version has been saved (see VersionSet::SaveTo)
	void addIterators(const ReadOptions& ops, std::vector<std::shared_ptr<Iterator>>* iters);


	VersionSet* vset;     // VersionSet to which this Version belongs

	// Lookup the value for key.  If found, store it in *val and
	// return OK.  Else return a non-OK status.  Fills *stats.
	// REQUIRES: lock is not held
	struct GetStats {
		std::shared_ptr<FileMetaData> seekFile;
		int seekFileLevel;
	};

	Status get(const ReadOptions& options, const LookupKey& key, std::string* val,
		GetStats* stats);

	// Return the level at which we should place a new memtable compaction
	// result that covers the range [smallest_user_key,largest_user_key].
	int pickLevelForMemTableOutput(const std::string_view& smallestUserKey,
		const std::string_view& largestUserKey);


	void getOverlappingInputs(
		int level,
		const InternalKey* begin,         // nullptr means before all keys
		const InternalKey* end,           // nullptr means after all keys
		std::vector<std::shared_ptr<FileMetaData>>* inputs);

	void saveValue(const std::any& arg, const std::string_view& ikey, const std::string_view& v);
	// Returns true iff some file in the specified level overlaps
	// some part of [*smallest_user_key,*largest_user_key].
	// smallest_user_key==nullptr represents a key smaller than all the DB's keys.
	// largest_user_key==nullptr represents a key largest than all the DB's keys.

	bool overlapInLevel(int level, const std::string_view* smallestUserKey,
		const std::string_view* largestUserKey);

	// Adds "stats" into the version state.  Returns true if a new
	// compaction may need to be triggered, false otherwise.
	// REQUIRES: lock is held
	bool updateStats(const GetStats& stats);

	// Return a human readable string that describes this version's contents.
	std::string debugString() const;

	// No copying allowed
	Version(const Version&);

	void operator=(const Version&);

	friend class Compaction;

	friend class VersionSet;

	class LevelFileNumIterator;

	std::shared_ptr<Iterator> newConcatenatingIterator(const ReadOptions& ops, int level) const;

	// List of files per level
	std::vector<std::shared_ptr<FileMetaData>> files[kNumLevels];

	// Next file to compact based on seek stats.
	std::shared_ptr<FileMetaData> fileToCompact;
	int fileToCompactLevel;

	// Level that should be compacted next and its compaction score.
	// Score< 1 means compaction is not strictly needed.  These fields
	// are initialized by Finalize().
	double compactionScore;
	int compactionLevel;
};

class Builder {
public:
	Builder(VersionSet* vset, const std::shared_ptr<Version>& base);

	~Builder();

	// Apply all of the edits in *edit to the version state.
	void apply(VersionEdit* edit);

	// Save the version state in *v.
	void saveTo(Version* v);

	void maybeAddFile(Version* v, int level, const std::shared_ptr<FileMetaData>& f);

private:
	// Helper to sort by v->files_[file_number].smallest
	struct BySmallestKey {
		const InternalKeyComparator* internalComparator;

		bool operator()(const std::shared_ptr<FileMetaData>& f1,
			const std::shared_ptr<FileMetaData>& f2) const {
			int r = internalComparator->compare(f1->smallest, f2->smallest);
			if (r != 0) {
				return (r< 0);
			}
			else {
				// Break ties by file number
				return (f1->number< f2->number);
			}
		}
	};

	typedef std::set<std::shared_ptr<FileMetaData>, BySmallestKey> FileSet;
	struct LevelState {
		std::set<uint64_t> deletedFiles;
		std::shared_ptr<FileSet> addedFiles;
	};

	VersionSet* vset;
	std::shared_ptr<Version> base;
	LevelState levels[kNumLevels];
};

class VersionSet {
public:
	VersionSet(const std::string& dbname, const Options& options,
		const std::shared_ptr<TableCache>& tablecache, const InternalKeyComparator* cmp);

	~VersionSet();

	uint64_t getLastSequence() const { return lastSequence; }

	void setLastSequence(uint64_t s) {
		assert(s >= lastSequence);
		lastSequence = s;
	}

	// Returns true iff some level needs a compaction.
	bool needsCompaction() const {
		return (version->compactionScore >= 1) || (version->fileToCompact != nullptr);
	}

	// Arrange to reuse "file_number" unless a newer file number has
	// already been allocated.
	// REQUIRES: "file_number" was returned by a call to NewFileNumber().
	void reuseFileNumber(uint64_t fileNumber) {
		if (nextFileNumber == fileNumber + 1) {
			nextFileNumber = fileNumber;
		}
	}

	// Apply *edit to the version version to form a new descriptor that
	// is both saved to persistent state and installed as the new
	// version version.  Will release *mu while actually writing to the file.
	// REQUIRES: *mu is held on entry.
	// REQUIRES: no other thread concurrently calls LogAndApply()
	Status logAndApply(VersionEdit* edit, std::mutex* mutex);

	void finalize(Version* v);

	// Save version contents to *log
	Status writeSnapshot();

	// Add all files listed in any live version to *live.
	// May also mutate some internal state.
	void addLiveFiles(std::set<uint64_t>* live);

	// Return the approximate offset in the database of the data for
	// "key" as of version "v".
	uint64_t approximateOffsetOf(const InternalKey& key);

	uint64_t getLogNumber() { return logNumber; }

	uint64_t getPrevLogNumber() { return prevLogNumber; }

	const InternalKeyComparator icmp;

	uint64_t newFileNumber() { return nextFileNumber++; }

	Status recover(bool* manifest);

	void markFileNumberUsed(uint64_t number);

	uint64_t getManifestFileNumber() { return manifestFileNumber; }

	void appendVersion(const std::shared_ptr<Version>& v);

	bool reuseManifest(const std::string& dscname, const std::string& dscbase);

	// Return the number of Table files at the specified level.
	int numLevelFiles(int level) const;

	// Return the combined file size of all files at the specified level.
	int64_t numLevelBytes(int level) const;

	// Return the maximum overlapping data (in bytes) at next level for any
	// file at a level >= 1.
	int64_t maxNextLevelOverlappingBytes();

	std::shared_ptr<Iterator> makeInputIterator(Compaction* c);

	// Return a human-readable short (single-line) summary of the number
	// of files per level.  Uses *scratch as backing store.
	struct LevelSummaryStorage {
		char buffer[100];
	};

	const char* levelSummary(LevelSummaryStorage* scratch) const;

	// Pick level and inputs for a new compaction.
	// Returns nullptr if there is no compaction to be done.
	// Otherwise returns a pointer to a heap-allocated object that
	// describes the compaction.  Caller should delete the result.
	std::shared_ptr<Compaction> pickCompaction();

	// Return a compaction object for compacting the range [begin,end] in
	// the specified level.  Returns nullptr if there is nothing in that
	// level that overlaps the specified range.  Caller should delete
	// the result.
	std::shared_ptr<Compaction> compactRange(
		int level,
		const InternalKey* begin,
		const InternalKey* end);

	void getRange(const std::vector<std::shared_ptr<FileMetaData>>& inputs,
		InternalKey* smallest,
		InternalKey* largest);

	void getRange2(const std::vector<std::shared_ptr<FileMetaData>>& inputs1,
		const std::vector<std::shared_ptr<FileMetaData>>& inputs2,
		InternalKey* smallest,
		InternalKey* largest);

	void setupOtherInputs(const std::shared_ptr<Compaction>& c);

	std::shared_ptr<TableCache> getTableCache() { return tablecache; }
	// Per-level key at which the next compaction at that level should start.
	// Either an empty string, or a valid InternalKey.

	std::string compactPointer[kNumLevels];
	const std::string dbname;
	const Options options;
	uint64_t nextFileNumber;
	uint64_t manifestFileNumber;
	uint64_t lastSequence;
	uint64_t logNumber;
	uint64_t prevLogNumber;  // 0 or backing store for memtable being compacted

	std::shared_ptr<Version> version;
	std::shared_ptr<LogWriter> descriptorLog;
	std::shared_ptr<WritableFile> descriptorFile;
	std::shared_ptr<TableCache> tablecache;
};

int findFile(const InternalKeyComparator & icmp,
	const std::vector<std::shared_ptr<FileMetaData>> & files,
	const std::string_view & key);

bool someFileOverlapsRange(const InternalKeyComparator & icmp, bool disjointSortedFiles,
	const std::vector<std::shared_ptr<FileMetaData>> & files,
	const std::string_view * smallestUserKey,
	const std::string_view * largestUserKey);

// A Compaction encapsulates information about a compaction.
class Compaction {
public:
	Compaction(const Options* options, int level);

	~Compaction();

	// Return the level that is being compacted.  Inputs from "level"
	// and "level+1" will be merged to produce a set of "level+1" files.
	int getLevel() const { return level; }

	// Return the object that holds the edits to the descriptor done
	// by this compaction.
	VersionEdit* getEdit() { return &edit; }

	// "which" must be either 0 or 1
	int numInputFiles(int which) const { return inputs[which].size(); }

	// Return the ith input file at "level()+which" ("which" must be 0 or 1).
	std::shared_ptr<FileMetaData> input(int which, int i) const { return inputs[which][i]; }

	// Maximum size of files to build during this compaction.
	uint64_t getMaxOutputFileSize() const { return maxOutputfileSize; }

	// Is this a trivial compaction that can be implemented by just
	// moving a single input file to the next level (no merging or splitting)
	bool isTrivialMove() const;

	// Add all inputs to this compaction as delete operations to *edit.
	void addInputDeletions(VersionEdit* edit);

	// Returns true if the information we have available guarantees that
	// the compaction is producing data in "level+1" for which no data exists
	// in levels greater than "level+1".
	bool isBaseLevelForKey(const std::string_view& userKey);

	// Returns true iff we should stop building the version output
	// before processing "internal_key".
	bool shouldStopBefore(const std::string_view& internalKey);

	// Release the input version for the compaction, once the compaction
	// is successful.
	void releaseInputs();

private:
	friend class Version;

	friend class VersionSet;

	int level;
	uint64_t maxOutputfileSize;
	size_t grandparentIndex; // Index in grandparent_starts_
	bool seenKey; // Some output key has been seen
	int64_t overlappedBytes; // Bytes of overlap between version output
	// and grandparent files
	// State for implementing IsBaseLevelForKey

	// level_ptrs_ holds indices into input_version_->levels_: our state
	// is that we are positioned at one of the file ranges for each
	// higher level than the ones involved in this compaction (i.e. for
	// all L >= level_ + 2).
	size_t levelPtrs[kNumLevels];

	VersionEdit edit;
	std::shared_ptr<Version> inputVersion;
	// Each compaction reads inputs from "level_" and "level_+1"
	std::vector<std::shared_ptr<FileMetaData>> inputs[2];
	// State used to check for number of of overlapping grandparent files
	// (parent == level_ + 1, grandparent == level_ + 2)
	std::vector<std::shared_ptr<FileMetaData>> grandparents;
};

