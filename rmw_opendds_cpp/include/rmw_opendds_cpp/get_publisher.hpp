// Copyright 2017 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RMW_OPENDDS_CPP__GET_PUBLISHER_HPP_
#define RMW_OPENDDS_CPP__GET_PUBLISHER_HPP_

#include "rmw_opendds_shared_cpp/opendds_include.hpp"
#include "rmw/rmw.h"
#include "rmw_opendds_cpp/visibility_control.h"

namespace rmw_opendds_cpp
{

/// Return a native OpenDDS data writer handle.
/**
 * The function returns `NULL` when either the publisher handle is `NULL` or
 * when the publisher handle is from a different rmw implementation.
 *
 * \return native OpenDDS data writer handle if successful, otherwise `NULL`
 */
RMW_OPENDDS_CPP_PUBLIC
DDS::DataWriter *
get_data_writer(rmw_publisher_t * publisher);

}  // namespace rmw_opendds_cpp

#endif  // RMW_OPENDDS_CPP__GET_PUBLISHER_HPP_
