/**
 * Project: CoCo  
 * Copyright (c) 2015-2016, Scuola Superiore Sant'Anna
 * 
 * Authors: Emanuele Ruffaldi <e.ruffaldi@sssup.it>, Filippo Brizzi
 * 
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE.txt', which is part of this source code package.
 * 
 * --
 */
#pragma once
#include <string>
#include <unordered_map>
#include <ctime>
#include <chrono>
#include <vector>
#include <mutex>

#define COCO_START_TIMER(x) coco::util::TimerManager::instance()->startTimer(x);
#define COCO_STOP_TIMER(x) coco::util::TimerManager::instance()->stopTimer(x);
#define COCO_CLEAR_TIMER(x) coco::util::TimerManager::instance()->removeTimer(x);
#define COCO_TIME_COUNT(x) coco::util::TimerManager::instance()->getTimeCount(x);
#define COCO_TIME(x) coco::util::TimerManager::instance()->getTime(x);
#define COCO_TIME_MEAN(x) coco::util::TimerManager::instance()->getTimeMean(x);
#define COCO_TIME_VARIANCE(x) coco::util::TimerManager::instance()->getTimeVariance(x);
#define COCO_SERVICE_TIME(x) coco::util::TimerManager::instance()->getServiceTime(x);
#define COCO_SERVICE_TIME_VARIANCE(x) coco::util::TimerManager::instance()->getServiceTimeVariance(x);
#define COCO_PRINT_ALL_TIME coco::util::TimerManager::instance()->printAllTime();

namespace coco
{
namespace util
{

struct Timer
{
public:	
	using time_t = std::chrono::system_clock::time_point;

	Timer(std::string timer_name = "")
	: name(timer_name)
	{
		start_time = std::chrono::system_clock::now();
	}
	
	void start()
	{
		auto now = std::chrono::system_clock::now();
		if (iteration != 0)
		{
			auto time = std::chrono::duration_cast<std::chrono::microseconds>(
					    now - start_time).count() / 1000000.0;
			service_time += time;
			service_time_square += time * time;
		}
		start_time = now;
		++iteration;
	}
	void stop()
	{
		double time = std::chrono::duration_cast<std::chrono::microseconds>(
				  std::chrono::system_clock::now() - start_time).count() / 1000000.0;
		elapsed_time += time;
		elapsed_time_square += time * time;
	}
    
	std::string name;
	time_t start_time;
	int iteration = 0;
	double elapsed_time = 0;
	double elapsed_time_square = 0;
	//time_t start_time;
	double service_time = 0;
	double service_time_square = 0;
};

class TimerManager
{
public:
	using time_t = std::chrono::system_clock::time_point;

	static TimerManager* instance()
	{
		static TimerManager timer_manager;
		return &timer_manager;
	}

    void startTimer(std::string name)
    {
    	std::unique_lock<std::mutex> mlock(timer_mutex_);
		if (timer_list_.find(name) == timer_list_.end())
			timer_list_[name] = Timer(name);
	    timer_list_[name].start();
    }
    void stopTimer(std::string name)
    {
    	std::unique_lock<std::mutex> mlock(timer_mutex_);
    
    	auto t = timer_list_.find(name);
    	if (t != timer_list_.end())
    		t->second.stop();
    }
    void removeTimer(std::string name)
    {
    	std::unique_lock<std::mutex> mlock(timer_mutex_);
		auto t = timer_list_.find(name);
		if (t != timer_list_.end())
			timer_list_.erase(t);
    }
    //void addElapsedTime(std::string name, double time);
    //void addTime(std::string name, time_t time);
	double getTime(std::string name)
	{
		std::unique_lock<std::mutex> mlock(timer_mutex_);
		auto t = timer_list_.find(name);
		if (t == timer_list_.end())
			return -1;
		return t->second.elapsed_time;
	}
	int getTimeCount(std::string name)
	{
		std::unique_lock<std::mutex> mlock(timer_mutex_);
		auto t = timer_list_.find(name);
		if (t == timer_list_.end())
			return -1;
		return t->second.iteration;
	}
	double getTimeMean(std::string name)
	{
		std::unique_lock<std::mutex> mlock(timer_mutex_);
		auto t = timer_list_.find(name);
		if (t == timer_list_.end())
			return -1;
		return t->second.elapsed_time / t->second.iteration;
	}
	double getTimeVariance(std::string name)
	{
		std::unique_lock<std::mutex> mlock(timer_mutex_);
		auto t = timer_list_.find(name);
		if (t == timer_list_.end())
			return -1;
		return (t->second.elapsed_time_square / t->second.iteration) -
				std::pow(t->second.elapsed_time / t->second.iteration, 2);
	}
	double getServiceTime(std::string name)
	{
		std::unique_lock<std::mutex> mlock(timer_mutex_);
		auto t = timer_list_.find(name);
		if (t == timer_list_.end())
			return -1;
		return t->second.service_time / (t->second.iteration - 1);
	}
    double getServiceTimeVariance(std::string name)
    {
    	std::unique_lock<std::mutex> mlock(timer_mutex_);
		auto t = timer_list_.find(name);
		if (t == timer_list_.end())
			return -1;
		return (t->second.service_time_square / (t->second.iteration - 1)) -
				std::pow(t->second.service_time / (t->second.iteration - 1), 2);
    }
    void printAllTime()
    {
    	std::cout << std::endl;
		COCO_LOG(1) << "Printing time information for " << timer_list_.size() << " tasks";
		for (auto &t : timer_list_)
		{
			auto &name = t.first;
			COCO_LOG(1) << "Task: " << name;
			COCO_LOG(1) << "Number of iterations: " << getTimeCount(name);
			COCO_LOG(1) << "\tTotal    : " << getTime(name);
			COCO_LOG(1) << "\tMean     : " << getTimeMean(name);
			COCO_LOG(1) << "\tVariance : " << getTimeVariance(name);
			COCO_LOG(1) << "\tService time mean    : " << getServiceTime(name);
			COCO_LOG(1) << "\tService time variance: " << getServiceTimeVariance(name);
		}
		std::cout << std::endl;
    }

private:
	TimerManager()
	{ }

    std::unordered_map<std::string, Timer> timer_list_;
    std::mutex timer_mutex_;
};

}
}
