/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/detail/service/locator/routing.hpp"

#include "cocaine/logging.hpp"

#include <math.h>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/adjacent_find.hpp>
#include <boost/range/numeric.hpp>

#define PROTOTYPES
#include <mutils/mincludes.h>
#include <mutils/mhash.h>

using namespace cocaine::service;

continuum_t::continuum_t(std::unique_ptr<logging::log_t> log, const stored_type& group):
    m_log(std::move(log))
{
    const size_t length = group.size();
    const double weight = boost::accumulate(group | boost::adaptors::map_values, 0.0f);

    COCAINE_LOG_DEBUG(m_log, "populating continuum based on %d group elements, total weight: %d",
        length,
        weight
    );

    // Each item in a routing group has its own positive integer weight, so the total weight must
    // be more than 0.
    if(!length || weight < std::numeric_limits<double>::epsilon()) {
        throw cocaine::error_t("the total weight of the routing group must be positive");
    }

    union digest_t {
        char       hashed[16];
        point_type points[sizeof(hashed) / sizeof(point_type)];
    } digest;

    auto builder = std::back_inserter(m_elements);

    for(auto it = group.begin(); it != group.end(); ++it) {
        const double slice = it->second / weight;

        // Given a group element with a 100% weight, derive 64 content-based 16-byte hashes and
        // create four 4-byte points, based on those hashes. To support various weights, figure out
        // the proportional number of required hashes for this element.
        const size_t steps = ::lround(slice * (64 * length));
        const auto&  value = it->first;

        for(size_t step = 0; step < steps; ++step) {
            MHASH thread = mhash_init(MHASH_MD5);
            mhash(thread, value.data(), value.size());
            mhash(thread, &step, sizeof(step));
            mhash_deinit(thread, digest.hashed);

            // Generate four 4-byte points out of a 16-byte hash.
            std::transform(std::begin(digest.points), std::end(digest.points), builder,
                [&](const point_type& point) -> element_t
            {
                return {point, value};
            });
        }

        COCAINE_LOG_DEBUG(m_log, "added %d quads for %s, weight: %.02f%%, %d/%d", steps, value,
            slice * 100.0f,
            steps, length * 64
        );
    }

    // Sort the ring to enable binary searching.
    std::sort(m_elements.begin(), m_elements.end());

    COCAINE_LOG_DEBUG(m_log, "resulting continuum population: %d points, unique: %s",
        m_elements.size(),
        boost::adjacent_find(m_elements) == m_elements.end() ? "true" : "false"
    );

    // Prepare the RNG.
    std::random_device rd; m_rng.seed(rd());
}

std::string
continuum_t::get(const std::string& key) const {
    union digest_t {
        char       hashed[16];
        point_type points[sizeof(hashed) / sizeof(point_type)];
    } digest;

    MHASH thread = mhash_init(MHASH_MD5);
    mhash(thread, key.data(), key.size());
    mhash_deinit(thread, digest.hashed);

    // Derive the target point by XORing each 4-byte part of the hash.
    const point_type point = boost::accumulate(digest.points, 0, std::bit_xor<point_type>());

    // Return the next biggest number on the continuum relative to the hashed value, or the first
    // continuum element, if the hashed value is above all the other elements in the continuum.
    auto  it = std::upper_bound(m_elements.begin(), m_elements.end(), point);
    auto& rv = it != m_elements.end() ? *it : m_elements.front();

    COCAINE_LOG_DEBUG(m_log, "hashed key '%s' -> point %d mapped to %d, value: %s", key, point,
        rv.point, rv.value
    );

    return rv.value;
}

std::string
continuum_t::get() const {
    const point_type point = m_distribution(m_rng);

    // Return the next biggest number on the continuum relative to the random value, or the first
    // continuum element, if the random value is above all the other elements in the continuum.
    auto  it = std::upper_bound(m_elements.begin(), m_elements.end(), point);
    auto& rv = it != m_elements.end() ? *it : m_elements.front();

    COCAINE_LOG_DEBUG(m_log, "randomized keyless point %d mapped to %d, value: %s", point,
        rv.point, rv.value
    );

    return rv.value;
}

auto
continuum_t::all() const -> std::vector<std::tuple<point_type, std::string>> {
    typedef std::vector<std::tuple<point_type, std::string>> result_type;

    result_type tuples;
    tuples.reserve(m_elements.size());

    std::transform(m_elements.begin(), m_elements.end(), std::back_inserter(tuples),
        [](const element_t& element) -> result_type::value_type
    {
        // NOTE: Tuple constructor is explicit for some reason, so have to use full form.
        return std::make_tuple(element.point, element.value);
    });

    return tuples;
}
