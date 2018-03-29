#pragma once
#include "xSds.h"
#include "xTcpConnection.h"
#include "xObject.h"
#include "xTcpClient.h"
#include "xSocket.h"
#include "xLog.h"
#include "xThreadPool.h"

class xRedisAsyncContext;
typedef struct redisReply : noncopyable
{
    int32_t type;
    int64_t integer;
    int32_t len;
    char *str;
    size_t elements;
    struct redisReply **element;
};

typedef std::function<void(const RedisAsyncContextPtr &ac,redisReply*,const std::any)> RedisCallbackFn;
typedef struct redisReadTask : noncopyable
{
    int32_t type;
    int32_t elements;
    int32_t idx;
    redisReply *obj;
    std::any privdata;
    struct redisReadTask *parent;
} redisReadTask;

redisReply *createReplyObject(int32_t type);
redisReply *createString(const redisReadTask *task, const char *str, size_t len);
redisReply *createArray(const redisReadTask *task, int32_t elements);
redisReply *createInteger(const redisReadTask *task, int64_t value);
redisReply *createNil(const redisReadTask *task);
void freeReply(redisReply *reply);

typedef struct redisReplyObjectFunctions : noncopyable
{
	redisReplyObjectFunctions()
	{
		createStringFuc = createString;
	   	createArrayFuc = createArray;
		createIntegerFuc = createInteger;
		createNilFuc = createNil;
		freeObjectFuc = freeReply;
	}
	
	std::function<redisReply*(const redisReadTask*,const char*,size_t)> createStringFuc;
	std::function<redisReply*(const redisReadTask*, int32_t)> createArrayFuc;
	std::function<redisReply*(const redisReadTask*, int64_t)> createIntegerFuc;
	std::function<redisReply*(const redisReadTask*)> createNilFuc;
	std::function<void (redisReply*)> freeObjectFuc;
} redisFunc;

class xRedisReader : noncopyable
{
public:
	xRedisReader();
	xRedisReader(xBuffer *buffer);
	
	void clear();
	int32_t redisReaderGetReply(redisReply **reply);
	void redisReaderSetError(int32_t type, const char *str);
	void redisReaderSetErrorProtocolByte(char byte);
	void redisReaderSetErrorOOM();
	void moveToNextTask();
	int32_t processLineItem();
	int32_t processBulkItem();
	int32_t processMultiBulkItem();
	int32_t processItem();

	int64_t readLongLong(const char *s);
	const char *readBytes(uint32_t bytes);
	const char *readLine(int32_t *_len);

	char errstr[128];
	redisReply *reply;
	std::any privdata;

	int32_t ridx;
	int32_t err;
	size_t pos;
	redisReadTask rstack[9];
	redisFunc fn;
	xBuffer *buffer;
};

typedef struct redisCallback
{
	RedisCallbackFn fn;
    std::any privdata;
} RedisCallback;

typedef std::list<redisCallback> RedisCallbackList;
typedef struct redisAsyncCallback
{
	redisAsyncCallback()
	:data(nullptr),
	 len(0)
	{

	}
	char *data;
	int32_t  len;
	RedisCallback cb;
}RedisAsyncCallback;

typedef std::list<redisAsyncCallback> RedisAsyncCallbackList;
class xRedisContext : noncopyable
{
public:
	xRedisContext();
	xRedisContext(xBuffer *buffer,int32_t sockfd);
	~xRedisContext();

	int32_t redisvAppendCommand(const char *format, va_list ap);
	int32_t redisAppendCommand(const char *cmd, size_t len);
	redisReply *redisCommand(xBuffer *buffer);
	redisReply *redisCommand(const char *format, ...);
	redisReply *redisvCommand(const char *format, va_list ap);
	redisReply *redisCommandArgv(int32_t argc, const char **argv, const size_t *argvlen);
	int32_t redisAppendFormattedCommand(const char *cmd, size_t len);
	int32_t redisAppendCommandArgv(int32_t argc, const char **argv, const size_t *argvlen);
	void redisSetError(int32_t type, const char *str);
	redisReply *redisBlockForReply();
	int32_t redisContextWaitReady(int32_t msec);
	int32_t redisCheckSocketError();
	int32_t redisBufferRead();
	int32_t redisBufferWrite(int32_t *done);
	int32_t redisGetReply(redisReply **reply);
	int32_t redisGetReplyFromReader(redisReply **reply);
	int32_t redisContextConnectTcp(const char *addr, int16_t port, const struct timeval *timeout);
	int32_t redisAppendCommand(const char *format, ...);

	void setBlock();
	void setConnected();
	void clear();

	char errstr[128];
	const char *ip;
	int32_t err;
	int32_t fd;
	int8_t flags;
	int16_t p;
	RedisReaderPtr reader;
	xBuffer sender;
};

class xRedisAsyncContext : noncopyable
{
public:
	xRedisAsyncContext(xBuffer *buffer,const TcpConnectionPtr &conn,int32_t sockfd);
	~xRedisAsyncContext();

	int32_t __redisAsyncCommand(const RedisCallbackFn &fn,const std::any &privdata,char *cmd, size_t len);
	int32_t redisvAsyncCommand(const RedisCallbackFn &fn,const std::any &privdata,const char *format, va_list ap);
	int32_t redisAsyncCommand(const RedisCallbackFn &fn,const std::any &privdata,const char *format, ...);

	int32_t redisGetReply(redisReply **reply) { return c->redisGetReply(reply); }
	RedisContextPtr getRedisContext() { return c; }
	TcpConnectionPtr getServerConn() { return serverConn; }
	std::mutex &getMutex() { return mtx;}
	RedisAsyncCallbackList &getCb() { return asyncCb; }

private:
	int32_t err;
	char *errstr;
	std::any data;
	RedisContextPtr c;
	TcpConnectionPtr serverConn;
	RedisAsyncCallbackList asyncCb;
	std::mutex mtx;

	struct
	{
		RedisCallbackList invalid;
		std::unordered_map<rObj*,redisCallback> channels;
		std::unordered_map<rObj*,redisCallback> patterns;
	}sub;
};

class xHiredis : noncopyable
{
public:
	xHiredis(xEventLoop *loop);
	xHiredis(xEventLoop *loop,bool clusterMode);

	void clusterAskConnCallBack(const TcpConnectionPtr &conn);
	void clusterMoveConnCallBack(const TcpConnectionPtr &conn);
	void clusterErrorConnCallBack(const std::any &context);
	void redisReadCallBack(const TcpConnectionPtr &conn,xBuffer *buffer);
	void eraseTcpMap(int32_t context);
	void eraseRedisMap(int32_t sockfd);
	void insertRedisMap(int32_t sockfd, const RedisAsyncContextPtr &ac);
	void insertTcpMap(int32_t data,const TcpClientPtr &tc);

	xThreadPool &getPool() { return pool; }
	int32_t setCount() { return ++count; }
	int32_t getCount() { return count; }
	std::mutex &getMutex() { return rtx; }

	std::unordered_map<int32_t,RedisAsyncContextPtr> &getRedisMap() { return redisAsyncs; }
	std::unordered_map<int32_t,TcpClientPtr> &getClientMap() { return tcpClients; }

private:
	std::unordered_map<int32_t,TcpClientPtr> tcpClients;
	std::unordered_map<int32_t,RedisAsyncContextPtr> redisAsyncs;
	std::unordered_map<int32_t,redisAsyncCallback> clusters;

	xThreadPool pool;
	bool clusterMode;
	std::atomic<int> count;
	std::mutex rtx;
};

int32_t redisFormatCommand(char **target, const char *format, ...);
int32_t redisFormatCommandArgv(char **target, int32_t argc, const char **argv, const size_t *argvlen);
int32_t redisvFormatCommand(char **target, const char *format, va_list ap);

RedisContextPtr redisConnectWithTimeout(const char *ip, int16_t port, const struct timeval tv);
RedisContextPtr redisConnect(const char *ip, int16_t port);

