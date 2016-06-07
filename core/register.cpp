/**
Copyright 2015, Filippo Brizzi"
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

@Author 
Filippo Brizzi, PhD Student
fi.brizzi@sssup.it
Emanuele Ruffaldi, PhD
e.ruffaldi@sssup.it

PERCRO, (Laboratory of Perceptual Robotics)
Scuola Superiore Sant'Anna
via Luigi Alamanni 13D, San Giuliano Terme 56010 (PI), Italy
*/

#include "register.h"

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

ComponentSpec::ComponentSpec(const std::string &class_name,
                             const std::string &name,
                             make_fx_t fx)
	: class_name_(class_name), name_(name), fx_(fx)
{
    COCO_DEBUG("Registry") << this << " spec selfregistering " << name;
	ComponentRegistry::addSpec(this);
}

TypeSpec::TypeSpec(const char * name, const std::type_info & type,
                   std::function<bool(std::ostream&,void*)>  out_fx)
    : name_(name), out_fx_(out_fx), type_(type)
{
    COCO_DEBUG("Registry") << this << " typespec selfregistering " << name_;
    ComponentRegistry::addType(this);
}

ComponentRegistry & ComponentRegistry::get()
{
	if (!singleton)
		singleton = new ComponentRegistry();
	return *singleton;
}

// static
std::shared_ptr<TaskContext> ComponentRegistry::create(const std::string &name,
                                                       const std::string &instantiation_name)
{
	return get().createImpl(name, instantiation_name);
}
std::shared_ptr<TaskContext> ComponentRegistry::createImpl(const std::string &name,
                                                           const std::string &instantiation_name)
{
    // TODO what happens if I create the same task twice?
    auto it = specs_.find(name);
	if(it == specs_.end())
		return 0;
	tasks_[instantiation_name] = it->second->fx_();

    if(!std::dynamic_pointer_cast<PeerTask>(tasks_[instantiation_name]))
		num_tasks_ += 1;
    return tasks_[instantiation_name];
}

// static
void ComponentRegistry::addSpec(ComponentSpec * s)
{
	get().addSpecImpl(s);
}
void ComponentRegistry::addSpecImpl(ComponentSpec * s)
{
    COCO_DEBUG("Registry") << this << " adding spec " << s->name_ << " " << s;
    specs_[s->name_] = s;
}

// static
void ComponentRegistry::alias(const std::string &new_name,
                              const std::string &old_name)
{
	return get().aliasImpl(new_name, old_name);
}
void ComponentRegistry::aliasImpl(const std::string &new_name,
                                  const std::string &old_name)
{
	auto it = specs_.find(old_name);
	if(it != specs_.end())
		specs_[new_name] = it->second;
}

bool ComponentRegistry::addLibrary(const std::string &library_name)
{
    return get().addLibraryImpl(library_name);
}
bool ComponentRegistry::addLibraryImpl(const std::string &library_name)
{
    dlhandle dl_handle = dlopen(library_name.c_str(), RTLD_NOW);
    if(!dl_handle)
    {
        COCO_ERR() << "Error opening library: " << library_name << "\nError: " << dlerror();
        return false;
    }

    typedef ComponentRegistry ** (*getRegistry_fx)();
    getRegistry_fx get_registry_fx = (getRegistry_fx)dlsym(dl_handle, "getComponentRegistry");
    if(!get_registry_fx)
        return false;

    ComponentRegistry ** other_registry = get_registry_fx();
    if(!*other_registry)
    {
        COCO_DEBUG("Registry") << this << " propagating to " << other_registry;
        *other_registry = this;
    }
    else if(*other_registry != this)
    {
        COCO_DEBUG("Registry") << this << " embedding other "
                               << *other_registry << " stored in " << other_registry;
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
        COCO_DEBUG("Registry") << this << " skipping self stored in " << other_registry;
    }

    libs_.insert(library_name);
    return true;
}

// static
bool ComponentRegistry::addLibrary(const std::string &l, const std::string &path)
{
	return get().addLibraryImpl(l, path);
}
bool ComponentRegistry::addLibraryImpl(const std::string &lib, const std::string &path)
{
    // Concatenate the library name with the path and the extensions
    std::string library_name;
    if (!path.empty())
    {
        library_name = std::string(path);
        if (library_name[library_name.size() - 1] != DIRSEP)
            library_name += (DIRSEP);
        library_name += DLLPREFIX + lib + DLLEXT;
    }
    else
    {
        library_name = lib;
    }
	if(libs_.find(library_name) != libs_.end())
		return true; // already loaded

    return addLibraryImpl(library_name);
}

const std::unordered_map<std::string, ComponentSpec*> & ComponentRegistry::components()
{
    return get().componentsImpl();
}

const std::unordered_map<std::string, ComponentSpec*> & ComponentRegistry::componentsImpl() const
{
    return specs_;
}

// static
void ComponentRegistry::addType(TypeSpec * s)
{
    get().addTypeImpl(s);
}
void ComponentRegistry::addTypeImpl(TypeSpec * s)
{
    COCO_DEBUG("Registry") << this << " adding type spec " << s->name_ << " " << s;
    typespecs_[s->type_.name()] = s;
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

std::shared_ptr<TaskContext> ComponentRegistry::task(std::string name)
{
	return get().taskImpl(name);
}
std::shared_ptr<TaskContext> ComponentRegistry::taskImpl(std::string name)
{
	auto t = tasks_.find(name);
	if (t == tasks_.end())
		return nullptr;
	return t->second;
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

int ComponentRegistry::numTasks()
{
	return get().numTasksImpl();
}
int ComponentRegistry::numTasksImpl() const
{
	return get().num_tasks_;
}

int ComponentRegistry::increaseConfigCompleted()
{
	return get().increaseConfigCompletedImpl();
}
int ComponentRegistry::increaseConfigCompletedImpl()
{
	return ++(get().tasks_config_ended_);
}

int ComponentRegistry::numConfigCompleted()
{
	return get().numConfigCompletedImpl();
}
int ComponentRegistry::numConfigCompletedImpl() const
{
	return get().tasks_config_ended_;
}

void ComponentRegistry::setResourcesPath(const std::vector<std::string> & resources_path)
{
	return get().setResourcesPathImpl(resources_path);
}
void ComponentRegistry::setResourcesPathImpl(const std::vector<std::string> & resources_path)
{
	resources_paths_ = resources_path;
} 

std::string ComponentRegistry::resourceFinder(const std::string &value)
{
	return get().resourceFinderImpl(value);
}
std::string ComponentRegistry::resourceFinderImpl(const std::string &value)
{
    std::ifstream stream;
    stream.open(value);
    if (stream.is_open())
    {
        return value;
    }
    else
    {
        for (auto &rp : resources_paths_)
        {
            if (rp.back() != '/' && value[0] != '/')
                rp += std::string("/");
            std::string tmp = rp + value;
            stream.open(tmp);
            if (stream.is_open())
            {
                return tmp;
            }
        }
    }
    stream.close();
    return "";
}

const std::unordered_map<std::string, std::shared_ptr<TaskContext> >& ComponentRegistry::tasks()
{
	return get().tasks_;
}

} // end of namespace

extern "C" 
{
	coco::ComponentRegistry ** getComponentRegistry() { return &singleton; }
}
