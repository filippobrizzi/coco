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
#include "register.h"
#include <sys/stat.h>

inline bool fileExist(const char *name)
{
  struct stat   buffer;
  return (stat (name, &buffer) == 0);
}

// dlopen cross platform
#ifdef WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	typedef HANDLE dlhandle;
	#define DLLEXT ".dll"
	#define DLLPREFIX "lib"
	#define dlopen(x,y) LoadLibrary(x)
	#define dlsym(x,y) GetProcAddress((HMODULE)x,y)
	#define dlerror() GetLastError()
	#define RTLD_NOW 0
	#define PATHSEP ';'
	#define DIRSEP '\\'
#else
	#include <dlfcn.h>
	typedef void* dlhandle;
	#define PATHSEP ':'
	#define DIRSEP '/'

	#ifdef __APPLE__
		#define DLLEXT ".dylib"
		#define DLLPREFIX "lib"
	#else
		#define DLLEXT ".so"
		#define DLLPREFIX "lib"
	#endif
#endif

/// as pointer to avoid issues of order in ctors
static coco::ComponentRegistry *singleton;

namespace coco
{

TypeSpec::TypeSpec(const char * name, const std::type_info & type, std::function<bool(std::ostream&,void*)>  out_fx)
	: name_(name), out_fx_(out_fx), type_(type)
{
	COCO_DEBUG("Registry") << "[coco] " << this << " typespec selfregistering " << name_;
	ComponentRegistry::addType(this);
}


ComponentSpec::ComponentSpec(const std::string &classname, const std::string &name, make_fx_t fx)
	: name_(name), classname_(classname), fx_(fx)
{
	COCO_DEBUG("Registry") << "[coco] " << this << " spec selfregistering " << name;
	ComponentRegistry::addSpec(this);
}

ComponentRegistry & ComponentRegistry::get()
{
	if (!singleton)
		singleton = new ComponentRegistry();
	return *singleton;
}

// static
TaskContext * ComponentRegistry::create(const std::string &name,
										const std::string &instantiation_name)
{
	return get().createImpl(name, instantiation_name);
}
TaskContext * ComponentRegistry::createImpl(const std::string &name,
										    const std::string &instantiation_name)
{
    auto it = specs_.find(name);
	if(it == specs_.end())
		return 0;
	//return it->second->fx_();
	tasks_[instantiation_name] = it->second->fx_();
    return tasks_[instantiation_name];
}

// static
void ComponentRegistry::addType(TypeSpec * s)
{
	get().addTypeImpl(s);
}

// static
void ComponentRegistry::addSpec(ComponentSpec * s)
{
	get().addSpecImpl(s);
}

void ComponentRegistry::addTypeImpl(TypeSpec * s)
{
	COCO_DEBUG("Registry") << "[coco] " << this << " adding type spec " << s->name_ << " " << s;	
	typespecs_[s->type_.name()] = s;
}

void ComponentRegistry::addSpecImpl(ComponentSpec * s)
{
	COCO_DEBUG("Registry") << "[coco] " << this << " adding spec " << s->name_ << " " << s;	
	specs_[s->name_] = s;
}

// static
void ComponentRegistry::alias(const std::string &new_name,const std::string &old_name)
{
	return get().aliasImpl(new_name, old_name);
}
void ComponentRegistry::aliasImpl(const std::string &new_name,const std::string &old_name)
{
	auto it = specs_.find(old_name);
	if(it != specs_.end())
		specs_[new_name] = it->second;
}

// static
bool ComponentRegistry::addLibrary(const std::string &l, const std::string &path)
{
	return get().addLibraryImpl(l, path);
}


bool ComponentRegistry::addLibraryImpl(const std::string &lib, const std::string &path)
{
	// path + "/" + DLLPREFIX + lib + DLLEXT
    std::string library_path;
    if (!path.empty())
    {
        library_path = path;
        if (library_path[library_path.size() - 1] != DIRSEP)
            library_path += (DIRSEP);
        library_path += DLLPREFIX + lib + DLLEXT;
    }
    else
    {
        library_path = DLLPREFIX + lib + DLLEXT;
    }

	if(libs_.find(library_path) != libs_.end())
		return true; // already loaded

	if(!fileExist(library_path.c_str()))
		return false;
	
	dlhandle dl_handle = dlopen(library_path.c_str(), RTLD_NOW);
	if(!dl_handle)
	{
		COCO_ERR() << "Error opening library: " << lib << " as " << library_path << "\nError: " << dlerror();
		return false;		
	}

	typedef ComponentRegistry ** (*getRegistry_fx)();
	getRegistry_fx get_registry_fx = (getRegistry_fx)dlsym(dl_handle, "getComponentRegistry");
	if(!get_registry_fx)
		return false;

	ComponentRegistry ** other_registry = get_registry_fx();
	if(!*other_registry)
	{
		COCO_DEBUG("Registry") << "[coco] " << this << " propagating to " << other_registry;
		*other_registry = this;
	}
	else if(*other_registry != this)
	{
		COCO_DEBUG("Registry") << "[coco] " << this << " embedding other " << *other_registry << " stored in " << other_registry;
		// import the specs and the destroy the imported registry and replace it inside the shared library
		for(auto&& i : (*other_registry)->specs_)
		{
			specs_[i.first] = i.second;
		}		
		for(auto&& i : (*other_registry)->typespecs_)
		{
			typespecs_[i.first] = i.second;
		}		
		for(auto&& i : (*other_registry)->typespecs2_)
		{
			typespecs2_[i.first] = i.second;
		}		
		delete *other_registry;
		*other_registry = this;
	}
	else
	{
		COCO_DEBUG("Registry") << "[coco] " << this << " skipping self stored in " << other_registry;
	}
	
	libs_.insert(library_path);
	return true;
}

const std::unordered_map<std::string, ComponentSpec*> & ComponentRegistry::components()
{
    return get().componentsImpl();
}

const std::unordered_map<std::string, ComponentSpec*> & ComponentRegistry::componentsImpl() const
{
    return specs_;
}

TypeSpec *ComponentRegistry::type(std::string name)
{
	return get().typeImpl(name);
}

TypeSpec *ComponentRegistry::type(const std::type_info & ti)
{
	return get().typeImpl(ti);
}

TypeSpec *ComponentRegistry::typeImpl(std::string name)
{
	auto t = typespecs_.find(name);
	if (t == typespecs_.end())
		return nullptr;
	else
		return t->second;
}

TypeSpec *ComponentRegistry::typeImpl(const std::type_info & impl)
{
	auto t = typespecs2_.find((std::uintptr_t)(void*)&impl);
	if (t == typespecs2_.end())
		return nullptr;
	else
		return t->second;
}

TaskContext *ComponentRegistry::task(std::string name)
{
	return get().taskImpl(name);
}
TaskContext *ComponentRegistry::taskImpl(std::string name)
{
	auto t = tasks_.find(name);
	if (t == tasks_.end())
		return nullptr;
	return t->second;
}

const std::unordered_map<std::string, TaskContext *> & ComponentRegistry::tasks()
{
	return get().tasksImpl();
}
const std::unordered_map<std::string, TaskContext *> & ComponentRegistry::tasksImpl() const
{
	return tasks_;
}

bool ComponentRegistry::profilingEnabled()
{
	return get().profilingEnabledImpl();
}

bool ComponentRegistry::profilingEnabledImpl()
{
	return profiling_enabled_;
}

void ComponentRegistry::enableProfiling(bool enable)
{
	get().enableProfilingImpl(enable);
}

void ComponentRegistry::enableProfilingImpl(bool enable)
{
	profiling_enabled_ = enable;
}

} // end of namespace

extern "C" 
{
	coco::ComponentRegistry ** getComponentRegistry() { return &singleton; }
}
