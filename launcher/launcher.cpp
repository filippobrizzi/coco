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
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "loader.h"
#include "input_parser.h"
#include "xml_creator.h"
#include "util/timing.h"

coco::CocoLauncher *launcher = {nullptr};
std::atomic<bool> stop_execution = {false};
std::mutex launcher_mutex, statistics_mutex;
std::condition_variable launcher_condition_variable, statistics_condition_variable;

void handler(int sig)
{
  void *array[10];
  size_t size;

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

void terminate(int sig)
{
    if (launcher)
        launcher->killApp();
    stop_execution = true;
    launcher_condition_variable.notify_all();
    statistics_condition_variable.notify_all();
}

void printStatistics(int interval)
{
    while(!stop_execution)
    {
        std::unique_lock<std::mutex> mlock(statistics_mutex);
        statistics_condition_variable.wait_for(mlock, std::chrono::seconds(interval));

        COCO_PRINT_ALL_TIME
    }
}

void launchApp(std::string confing_file_path, bool profiling, const std::string &graph)
{
    launcher = new coco::CocoLauncher(confing_file_path.c_str());
    launcher->createApp(profiling);
    if (!graph.empty())
        launcher->createGraph(graph);
    launcher->startApp();

    std::unique_lock<std::mutex> mlock(launcher_mutex);
    launcher_condition_variable.wait(mlock);
}

int main(int argc, char **argv)
{
    signal(SIGSEGV, handler);
    signal(SIGBUS, handler);
    signal(SIGINT, terminate);

    InputParser options(argc, argv);

    std::string config_file = options.getString("config_file");
    if (!config_file.empty())
    {
        std::thread statistics;
        
        bool profiling = options.get("profiling");
        if (profiling)
        {
            int interval = options.getInt("profiling");
            statistics = std::thread(printStatistics, interval);
        }
        
        std::string graph = options.getString("graph");;

        std::cout << "GRAPH: graph \n\n";
        launchApp(config_file, profiling, graph);

        if (statistics.joinable())
            statistics.join();
        return 0;
    }

    std::string library_name = options.getString("lib");
    if (!library_name.empty())
    {
        COCO_INIT_LOG();
        coco::printXMLSkeletonLibrary(library_name, "", true, true);
        return 0;
    }

    if (options.get("help"))
    {
        options.print();
        return 0;
    }

    options.print();
    return 1;
}
