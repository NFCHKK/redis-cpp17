#pragma once
#include "all.h"
#include "channel.h"
#include "callback.h"
#include "timer.h"

class EventLoop;
class TimerId;

class TimerQueue
{
public:
	TimerQueue(EventLoop *loop);
	~TimerQueue();

	void cancelTimer(const TimerPtr &timer);
	void handleRead();
	size_t getTimerSize();
	TimerPtr addTimer(double when,bool repeat,TimerCallback &&cb);
  	TimerPtr getTimerBegin();

private:
  	TimerQueue(const TimerQueue&);
  	void operator=(const TimerQueue&);

	EventLoop *loop;
	int32_t timerfd;
#ifdef __linux__
	Channel timerfdChannel;
#endif
	int64_t sequence;

	void cancelInloop(const TimerPtr &timer);
	void addTimerInLoop(const TimerPtr &timer);

	typedef std::multimap<int64_t,TimerPtr> TimerList;
	typedef std::map<int64_t,TimerPtr> ActiveTimer;

	std::multimap<int64_t,TimerPtr> expired;
	TimerList timers;

	void getExpired(const TimeStamp &now);
	void reset(const TimeStamp &now);
	bool insert(const TimerPtr &timer);

	ActiveTimer activeTimers;
	bool callingExpiredTimers;
	ActiveTimer cancelingTimers;
};


