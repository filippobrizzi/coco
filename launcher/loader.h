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
#include <unordered_map>
#include <unordered_set>
#include "core.h"
#include "register.h"
#include "tinyxml2/tinyxml2.h"
#include <exception>

namespace coco
{
    struct LComponentBase
    {

        virtual PortBase * getInPort(const std::string & x) = 0;
        virtual PortBase * getOutPort(const std::string & x) = 0;
        virtual std::string name() const = 0;
        PortBase * getPort(const std::string & x)
        {
            PortBase * p = getInPort(x);
            if(p) return p;
            return getOutPort(x);
        }
    };

    struct LRealComponent: public LComponentBase
    {
        LRealComponent(TaskContext * t): real(t) {}

        TaskContext * real;

        virtual std::string name() const { return real->name(); }
        virtual PortBase * getInPort(const std::string & x) { return real->port(x); }
        virtual PortBase * getOutPort(const std::string & x) { return real->port(x); }
    };

    struct LVirtualComponent: public LComponentBase
    {
    public:
        std::string name_;
        std::map<std::string,PortBase*> inports;
        std::map<std::string,PortBase*> outports;

        virtual std::string name() const { return name_; }
        PortBase * getInPort(const std::string & x) { return inports[x]; }

        PortBase * getOutPort(const std::string & x) { return outports[x]; }

    };


/**
 * Launcher class that takes a XML file and creates the network
 */
class CocoLauncher
{
public:
    CocoLauncher(const std::string &config_file);

    bool createApp(bool profiling = false);
    void startApp();
    void createGraph(const std::string& filename) const;
    void waitToComplete();
    void killApp();
    
    std::string lookupResource(const std::string &value);

private:
    void discoverTasks();
    void addResourcePath(std::string pa);
    bool parseFile(tinyxml2::XMLDocument & doc, bool top);
    void parseLogConfig(tinyxml2::XMLElement *logconfig);
    void parsePaths(tinyxml2::XMLElement *paths);
    void parseInclude(tinyxml2::XMLElement *include);
    void parseActivity(tinyxml2::XMLElement *activity);
    void parseComponent(tinyxml2::XMLElement *component, Activity *activity, bool is_peer = false);
    void parseSchedule(tinyxml2::XMLElement *schedule_policy, SchedulePolicy &policy, bool &is_parallel);
    void parseAttribute(tinyxml2::XMLElement *attributes, TaskContext *t);
    void parsePeers(tinyxml2::XMLElement *peers, TaskContext *t);
    void parseConnection(tinyxml2::XMLElement *connection);
    void createGraphPort(coco::PortBase *port, std::ofstream &dot_file,
                         std::unordered_map<std::string, int> &graph_port_nodes,
                         int &node_count) const;
    void createGraphPeer(coco::TaskContext *peer, std::ofstream &dot_file, 
                         std::unordered_map<std::string, int> &graph_port_nodes,
                         int &subgraph_count, int &node_count) const;
    void createGraphConnection(coco::TaskContext *task, std::ofstream &dot_file,
                               std::unordered_map<std::string, int> &graph_port_nodes) const;

    const std::string &config_file_;
    std::string config_file_path_;
    tinyxml2::XMLDocument doc_;

    //std::map<std::string, std::shared_ptr<LComponentBase> > tasks_;
    std::unordered_map<std::string, TaskContext*> tasks_;
    std::vector<Activity *> activities_;
    //std::map<std::string, TaskContext*> real_tasks_;
    std::list<std::string> peers_;

    std::vector<std::string> resources_paths_;
    std::vector<std::string> libraries_paths_;

    std::unordered_set<int> assigned_core_id_;
};

class CocoLoader
{
public:
    std::unordered_map<std::string, TaskContext *> addLibrary(std::string library_file_name);
    void clearTasks() { tasks_.clear(); }
    std::unordered_map<std::string, TaskContext *> tasks() { return tasks_; }
    TaskContext* task(std::string name)
    {
        if (tasks_.find(name) != tasks_.end())
            return tasks_[name];
        return nullptr;
    }
private:
    std::unordered_map<std::string, TaskContext *> tasks_;
};
    
} // end of namespace coco

