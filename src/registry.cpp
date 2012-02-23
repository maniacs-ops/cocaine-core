//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <boost/filesystem.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/algorithm/string/join.hpp>

#include "cocaine/context.hpp"
#include "cocaine/registry.hpp"

using namespace cocaine;
using namespace cocaine::core;
using namespace cocaine::plugin;

namespace fs = boost::filesystem;

struct is_regular_file {
    template<typename T> bool operator()(T entry) {
        return fs::is_regular(entry);
    }
};

registry_t::registry_t(context_t& context):
    m_context(context),
    m_log(context, "registry")
{
    if(lt_dlinit() != 0) {
        throw std::runtime_error("unable to initialize the module loader");
    }

    fs::path path(m_context.config.core.plugins);

    if(!fs::exists(path)) {
        throw std::runtime_error(path.string() + " does not exist");
    } else if(fs::exists(path) && !fs::is_directory(path)) {
        throw std::runtime_error(path.string() + " is not a directory");
    }

    lt_dladvise advice;
    lt_dladvise_init(&advice);
    lt_dladvise_global(&advice);

    lt_dlhandle plugin;
    initialize_fn_t initialize;
    std::vector<std::string> types;

    // Directory iterator
    typedef boost::filter_iterator<is_regular_file, fs::directory_iterator> file_iterator;
    file_iterator it = file_iterator(is_regular_file(), fs::directory_iterator(path)), end;

    while(it != end) {
        // Load the plugin
#if BOOST_FILESYSTEM_VERSION == 3
        std::string plugin_path = it->path().string();
#else
        std::string plugin_path = it->string();
#endif

        plugin = lt_dlopenadvise(plugin_path.c_str(), advice);

        if(plugin) {
            // Get the plugin info
            initialize = reinterpret_cast<initialize_fn_t>(lt_dlsym(plugin, "initialize"));

            if(initialize) {
                const module_info_t* info = initialize();
                m_plugins.push_back(plugin);

                // Fetch all the available modules from it
                while(info->type && info->factory) {
                    m_factories.insert(std::make_pair(
                        info->type,
                        info->factory));
                    types.push_back(info->type);
                    info++;
                }
            } else {
                m_log.error(
                    "invalid interface in '%s' - %s",
#if BOOST_FILESYSTEM_VERSION == 3
                    it->path().string().c_str(), 
#else
                    it->string().c_str(),
#endif
                    lt_dlerror()
                );

                lt_dlclose(plugin);
            }
        } else {
            m_log.error(
                "unable to load '%s' - %s",
#if BOOST_FILESYSTEM_VERSION == 3
                it->path().string().c_str(), 
#else
                it->string().c_str(),
#endif
                lt_dlerror()
            );
        }

        ++it;
    }

    if(!m_factories.size()) {
        throw std::runtime_error("no plugins found");
    }

    std::string plugins(boost::algorithm::join(types, ", "));
    m_log.info("available modules - %s", plugins.c_str());
}

registry_t::~registry_t() {
    for(std::vector<lt_dlhandle>::iterator it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        lt_dlclose(*it);
    }

    lt_dlexit();
}

bool registry_t::exists(const std::string& type) {
    return (m_factories.find(type) != m_factories.end());
}
