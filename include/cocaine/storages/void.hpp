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

#ifndef COCAINE_VOID_STORAGE_HPP
#define COCAINE_VOID_STORAGE_HPP

#include "cocaine/storages/base.hpp"

namespace cocaine { namespace storages {

class void_storage_t:
    public storage_t
{
    public:
        void_storage_t(context_t& ctx) { }

        virtual void put(const std::string& ns, const std::string& key, const Json::Value& value) { }

        virtual bool exists(const std::string& ns, const std::string& key) { return false; }
        virtual Json::Value get(const std::string& ns, const std::string& key) { return Json::Value(); }
        virtual Json::Value all(const std::string& ns) { return Json::Value(); }

        virtual void remove(const std::string& ns, const std::string& key) { }
        virtual void purge(const std::string& ns) { }
};

}}

#endif
