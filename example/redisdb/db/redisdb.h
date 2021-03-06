#pragma once
#include<string>
#include<map>
#include<list>
#include<queue>
#include<vector>
#include<unistd.h>
#include<vector>
#include<functional>
#include<cassert>
#include<iostream>

#include "option.h"
#include "cache.h"
#include "coding.h"
#include "db.h"
#include "redistring.h"
#include "redishash.h"
#include "rediszset.h"

class RedisDB {
public:
	RedisDB(const Options& options, const std::string& path);
	~RedisDB();

	Status open();

	// Strings Commands

	// Set key to hold the string value. if key
	// already holds a value, it is overwritten
	Status set(const std::string_view& key, const std::string_view& value);

	// Set key to hold the string value. if key exist
	Status setxx(const std::string_view& key,
			const std::string_view& value, int32_t* ret, const int32_t ttl = 0);

	// Get the value of key. If the key does not exist
	// the special value nil is returned
	Status get(const std::string_view& key, std::string* value);

	// Atomically sets key to value and returns the old value stored at key
	// Returns an error when key exists but does not hold a string value.
	Status getset(const std::string_view& key, const std::string_view& value, std::string* oldValue);

	// Sets or clears the bit at offset in the string value stored at key
	Status setbit(const std::string_view& key, int64_t offset, int32_t value, int32_t* ret);

	// Returns the bit value at offset in the string value stored at key
	Status getbit(const std::string_view& key, int64_t offset, int32_t* ret);

	// Sets the given keys to their respective values
	// MSET replaces existing values with new values
	Status mset(const std::vector<KeyValue>& kvs);

	// Returns the values of all specified keys. For every key
	// that does not hold a string value or does not exist, the
	// special value nil is returned
	Status mget(const std::vector<std::string>& keys, std::vector<ValueStatus>* vss);

	// Set key to hold string value if key does not exist
	// return 1 if the key was set
	// return 0 if the key was not set
	Status setnx(const std::string_view& key,
			const std::string& value, int32_t* ret, const int32_t ttl = 0);

	// Sets the given keys to their respective values.
	// MSETNX will not perform any operation at all even
	// if just a single key already exists.
	Status msetnx(const std::vector<KeyValue>& kvs, int32_t* ret);

	// Set key to hold string new_value if key currently hold the give value
	// return 1 if the key currently hold the give value And override success
	// return 0 if the key doesn't exist And override fail
	// return -1 if the key currently does not hold the given value And override fail
	Status setvx(const std::string_view& key,
			const std::string_view& value, const std::string_view& newValue, int32_t* ret, const int32_t ttl = 0);

	// Removes the specified keys
	// return -1 operation exception errors happen in database
	// return >=0 the number of keys that were removed
	int64_t del(const std::vector<std::string>& keys,
		std::map<DataType, Status>* typeStatus);

	// delete the key that holds a given value
	// return 1 if the key currently hold the give value And delete success
	// return 0 if the key doesn't exist And del fail
	// return -1 if the key currently does not hold the given value And del fail
	Status delvx(const std::string_view& key, const std::string_view& value, int32_t* ret);

	// Set key to hold string value if key does not exist
	// return the length of the string after it was modified by the command
	Status setrange(const std::string_view& key, int64_t startOffset,
		const std::string_view& value, int32_t* ret);

	// Returns the substring of the string value stored at key,
	// determined by the offsets start and end (both are inclusive)
	Status getrange(const std::string_view& key, int64_t startOffset, int64_t endOffset,
		std::string* ret);



	// Hashes Commands

	// Sets field in the hash stored at key to value. If key does not exist, a new
	// key holding a hash is created. If field already exists in the hash, it is
	// overwritten.
	Status hset(const std::string_view& key,
			const std::string_view& field, const std::string_view& value, int32_t* res);

	// Returns the value associated with field in the hash stored at key.
	// the value associated with field, or nil when field is not present in the
	// hash or key does not exist.
	Status hget(const std::string_view& key, const std::string_view& field, std::string* value);

	// Sets the specified fields to their respective values in the hash stored at
	// key. This command overwrites any specified fields already existing in the
	// hash. If key does not exist, a new key holding a hash is created.
	Status hmset(const std::string_view& key,
		const std::vector<FieldValue>& fvs);

	// Returns the values associated with the specified fields in the hash stored
	// at key.
	// For every field that does not exist in the hash, a nil value is returned.
	// Because a non-existing keys are treated as empty hashes, running HMGET
	// against a non-existing key will return a list of nil values.
	Status hmget(const std::string_view& key,
		const std::vector<std::string>& fields,
		std::vector<ValueStatus>* vss);

	// Returns all fields and values of the hash stored at key. In the returned
	// value, every field name is followed by its value, so the length of the
	// reply is twice the size of the hash.
	Status hgetall(const std::string_view& key, std::vector<FieldValue>* fvs);


	// Zsets Commands

  // Adds all the specified members with the specified scores to the sorted set
  // stored at key. It is possible to specify multiple score / member pairs. If
  // a specified member is already a member of the sorted set, the score is
  // updated and the element reinserted at the right position to ensure the
  // correct ordering.
  //
  // If key does not exist, a new sorted set with the specified members as sole
  // members is created, like if the sorted set was empty. If the key exists but
  // does not hold a sorted set, an error is returned.
  //
  // The score values should be the string representation of a double precision
  // floating point number. +inf and -inf values are valid values as well.
	Status zadd(const std::string_view& key,
		const std::vector<ScoreMember>& scoremembers, int32_t* ret);

	// Returns the sorted set cardinality (number of elements) of the sorted set
	// stored at key.
	Status zcard(const std::string_view& key, int32_t* ret);


	// Returns the number of elements in the sorted set at key with a score
	// between min and max.
	//
	// The min and max arguments have the same semantic as described for
	// ZRANGEBYSCORE.
	//
	// Note: the command has a complexity of just O(log(N)) because it uses
	// elements ranks (see ZRANK) to get an idea of the range. Because of this
	// there is no need to do a work proportional to the size of the range.
	Status zcount(const std::string_view& key, double min,
		double max, bool leftclose, bool rightclose, int32_t* ret);

	// Increments the score of member in the sorted set stored at key by
	// increment. If member does not exist in the sorted set, it is added with
	// increment as its score (as if its previous score was 0.0). If key does not
	// exist, a new sorted set with the specified member as its sole member is
	// created.
	//
	// An error is returned when key exists but does not hold a sorted set.
	//
	// The score value should be the string representation of a numeric value, and
	// accepts double precision floating point numbers. It is possible to provide
	// a negative value to decrement the score.
	Status zincrby(const std::string_view& key,
		const std::string_view& member, double increment, double* ret);

	// Returns the specified range of elements in the sorted set stored at key.
	// The elements are considered to be ordered from the lowest to the highest
	// score. Lexicographical order is used for elements with equal score.
	//
	// See ZREVRANGE when you need the elements ordered from highest to lowest
	// score (and descending lexicographical order for elements with equal score).
	//
	// Both start and stop are zero-based indexes, where 0 is the first element, 1
	// is the next element and so on. They can also be negative numbers indicating
	// offsets from the end of the sorted set, with -1 being the last element of
	// the sorted set, -2 the penultimate element and so on.
	//
	// start and stop are inclusive ranges, so for example ZRANGE myzset 0 1 will
	// return both the first and the second element of the sorted set.
	//
	// Out of range indexes will not produce an error. If start is larger than the
	// largest index in the sorted set, or start > stop, an empty list is
	// returned. If stop is larger than the end of the sorted set Redis will treat
	// it like it is the last element of the sorted set.
	//
	// It is possible to pass the WITHSCORES option in order to return the scores
	// of the elements together with the elements. The returned list will contain
	// value1,score1,...,valueN,scoreN instead of value1,...,valueN. Client
	// libraries are free to return a more appropriate data type (suggestion: an
	// array with (value, score) arrays/tuples).
	Status zrange(const std::string_view& key,
		int32_t start,
		int32_t stop,
		std::vector<ScoreMember>* scoremembers);

	// Returns all the elements in the sorted set at key with a score between min
	// and max (including elements with score equal to min or max). The elements
	// are considered to be ordered from low to high scores.
	//
	// The elements having the same score are returned in lexicographical order
	// (this follows from a property of the sorted set implementation in Redis and
	// does not involve further computation).
	//
	// The optional LIMIT argument can be used to only get a range of the matching
	// elements (similar to SELECT LIMIT offset, count in SQL). Keep in mind that
	// if offset is large, the sorted set needs to be traversed for offset
	// elements before getting to the elements to return, which can add up to O(N)
	// time complexity.
	//
	// The optional WITHSCORES argument makes the command return both the element
	// and its score, instead of the element alone. This option is available since
	// Redis 2.0.
	//
	// Exclusive intervals and infinity
	// min and max can be -inf and +inf, so that you are not required to know the
	// highest or lowest score in the sorted set to get all elements from or up to
	// a certain score.
	//
	// By default, the interval specified by min and max is closed (inclusive). It
	// is possible to specify an open interval (exclusive) by prefixing the score
	// with the character (. For example:
	//
	// ZRANGEBYSCORE zset (1 5
	// Will return all elements with 1< score<= 5 while:
	//
	// ZRANGEBYSCORE zset (5 (10
	// Will return all the elements with 5< score< 10 (5 and 10 excluded).
	//
	// Return value
	// Array reply: list of elements in the specified score range (optionally with
	// their scores).
	Status zrangebyscore(const std::string_view& key,
		double min,
		double max,
		bool leftclose,
		bool rightclose,
		std::vector<ScoreMember>* scoremembers);

	// Returns the rank of member in the sorted set stored at key, with the scores
	// ordered from low to high. The rank (or index) is 0-based, which means that
	// the member with the lowest score has rank 0.
	//
	// Use ZREVRANK to get the rank of an element with the scores ordered from
	// high to low.
	Status zrank(const std::string_view& key,
		const std::string_view& member, int32_t* rank);

	// Removes the specified members from the sorted set stored at key. Non
	// existing members are ignored.
	//
	// An error is returned when key exists and does not hold a sorted set.
	Status zrem(const std::string_view& key,
		std::vector<std::string> members,
		int32_t* ret);

	// Removes all elements in the sorted set stored at key with rank between
	// start and stop. Both start and stop are 0 -based indexes with 0 being the
	// element with the lowest score. These indexes can be negative numbers, where
	// they indicate offsets starting at the element with the highest score. For
	// example: -1 is the element with the highest score, -2 the element with the
	// second highest score and so forth.
	Status zremrangebyrank(const std::string_view& key,
		int32_t start, int32_t stop, int32_t* ret);


	// Removes all elements in the sorted set stored at key with a score between
	// min and max (inclusive).
	Status zremrangebyscore(const std::string_view& key,
		double min, double max, bool leftclose, bool rightclose, int32_t* ret);

	// Returns the specified range of elements in the sorted set stored at key.
	// The elements are considered to be ordered from the highest to the lowest
	// score. Descending lexicographical order is used for elements with equal
	// score.
	//
	// Apart from the reversed ordering, ZREVRANGE is similar to ZRANGE.
	Status zrevrange(const std::string_view& key,
		int32_t start, int32_t stop, std::vector<ScoreMember>* scoremembers);

	// Returns all the elements in the sorted set at key with a score between max
	// and min (including elements with score equal to max or min). In contrary to
	// the default ordering of sorted sets, for this command the elements are
	// considered to be ordered from high to low scores.
	//
	// The elements having the same score are returned in reverse lexicographical
	// order.
	//
	// Apart from the reversed ordering, ZREVRANGEBYSCORE is similar to
	// ZRANGEBYSCORE.
	Status zrevrangebyscore(const std::string_view& key,
		double min,
		double max,
		bool leftclose,
		bool rightclose,
		std::vector<ScoreMember>* scoremembers);

	// Returns the rank of member in the sorted set stored at key, with the scores
	// ordered from high to low. The rank (or index) is 0-based, which means that
	// the member with the highest score has rank 0.
	Status zrevrank(const std::string_view& key,
		const std::string_view& member, int32_t* rank);

	// Returns the score of member in the sorted set at key.
	//
	// If member does not exist in the sorted set, or key does not exist, nil is
	// returned.
	Status zscore(const std::string_view& key, const std::string_view& member, double* ret);

	// Computes the union of numkeys sorted sets given by the specified keys, and
	// stores the result in destination. It is mandatory to provide the number of
	// input keys (numkeys) before passing the input keys and the other (optional)
	// arguments.
	//
	// By default, the resulting score of an element is the sum of its scores in
	// the sorted sets where it exists.
	//
	// Using the WEIGHTS option, it is possible to specify a multiplication factor
	// for each input sorted set. This means that the score of every element in
	// every input sorted set is multiplied by this factor before being passed to
	// the aggregation function. When WEIGHTS is not given, the multiplication
	// factors default to 1.
	//
	// With the AGGREGATE option, it is possible to specify how the results of the
	// union are aggregated. This option defaults to SUM, where the score of an
	// element is summed across the inputs where it exists. When this option is
	// set to either MIN or MAX, the resulting set will contain the minimum or
	// maximum score of an element across the inputs where it exists.
	//
	// If destination already exists, it is overwritten.
	Status zunionstore(const std::string_view& destination,
		const std::vector<std::string>& keys,
		const std::vector<double>& weights,
		const AGGREGATE agg,
		int32_t* ret);

	// Computes the intersection of numkeys sorted sets given by the specified
	// keys, and stores the result in destination. It is mandatory to provide the
	// number of input keys (numkeys) before passing the input keys and the other
	// (optional) arguments.
	//
	// By default, the resulting score of an element is the sum of its scores in
	// the sorted sets where it exists. Because intersection requires an element
	// to be a member of every given sorted set, this results in the score of
	// every element in the resulting sorted set to be equal to the number of
	// input sorted sets.
	//
	// For a description of the WEIGHTS and AGGREGATE options, see ZUNIONSTORE.
	//
	// If destination already exists, it is overwritten.
	Status zinterstore(const std::string_view& destination,
		const std::vector<std::string>& keys,
		const std::vector<double>& weights,
		const AGGREGATE agg,
		int32_t* ret);

	// When all the elements in a sorted set are inserted with the same score, in
	// order to force lexicographical ordering, this command returns all the
	// elements in the sorted set at key with a value between min and max.
	//
	// If the elements in the sorted set have different scores, the returned
	// elements are unspecified.
	//
	// The elements are considered to be ordered from lower to higher strings as
	// compared byte-by-byte using the memcmp() C function. Longer strings are
	// considered greater than shorter strings if the common part is identical.
	//
	// The optional LIMIT argument can be used to only get a range of the matching
	// elements (similar to SELECT LIMIT offset, count in SQL). Keep in mind that
	// if offset is large, the sorted set needs to be traversed for offset
	// elements before getting to the elements to return, which can add up to O(N)
	// time complexity.
	Status zrangebylex(const std::string_view& key,
		const std::string_view& min,
		const std::string_view& max,
		bool leftclose,
		bool rightclose,
		std::vector<std::string>* members);

	// When all the elements in a sorted set are inserted with the same score, in
	// order to force lexicographical ordering, this command returns the number of
	// elements in the sorted set at key with a value between min and max.
	//
	// The min and max arguments have the same meaning as described for
	// ZRANGEBYLEX.
	//
	// Note: the command has a complexity of just O(log(N)) because it uses
	// elements ranks (see ZRANK) to get an idea of the range. Because of this
	// there is no need to do a work proportional to the size of the range.
	Status zlexcount(const std::string_view& key,
		const std::string_view& min,
		const std::string_view& max,
		bool leftclose,
		bool rightclose,
		int32_t* ret);

	// When all the elements in a sorted set are inserted with the same score, in
	// order to force lexicographical ordering, this command removes all elements
	// in the sorted set stored at key between the lexicographical range specified
	// by min and max.
	//
	// The meaning of min and max are the same of the ZRANGEBYLEX command.
	// Similarly, this command actually returns the same elements that ZRANGEBYLEX
	// would return if called with the same min and max arguments.
	Status zremrangebylex(const std::string_view& key,
		const std::string_view& min,
		const std::string_view& max,
		bool leftclose,
		bool rightclose,
		int32_t* ret);

private:
	std::shared_ptr<RedisString> redisstring;
	std::shared_ptr<RedisHash> redishash;
	std::shared_ptr<RedisZset> rediszset;
	std::shared_ptr<Env> env;
	const Options options;
	std::string path;
};

