// Copyright 2015-2017 Open Source Robotics Foundation, Inc.
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

#include <string>

#include "rmw_opendds_shared_cpp/guid_helper.hpp"
#include "rmw_opendds_shared_cpp/types.hpp"
#include "dds/DdsDcpsCoreTypeSupportC.h"
// Uncomment this to get extra console output about discovery.
// #define DISCOVERY_DEBUG_LOGGING 1

void CustomPublisherListener::on_data_available(DDS::DataReader * reader)
{
  DDS::PublicationBuiltinTopicDataDataReader * builtin_reader =
    dynamic_cast<DDS::PublicationBuiltinTopicDataDataReader *>(reader);

  DDS::PublicationBuiltinTopicDataSeq data_seq;
  DDS::SampleInfoSeq info_seq;
  DDS::ReturnCode_t retcode = builtin_reader->take(
    data_seq, info_seq, DDS::LENGTH_UNLIMITED,
    DDS::ANY_SAMPLE_STATE, DDS::ANY_VIEW_STATE, DDS::ANY_INSTANCE_STATE);

  if (retcode == DDS::RETCODE_NO_DATA) {
    return;
  }
  if (retcode != DDS::RETCODE_OK) {
    fprintf(stderr, "failed to access data from the built-in reader\n");
    return;
  }

  for (CORBA::ULong i = 0; i < data_seq.length(); ++i) {
    if (info_seq[i].valid_data &&
      info_seq[i].instance_state == DDS::ALIVE_INSTANCE_STATE)
    {
      DDS::InstanceHandle_t participant_guid;
      // @todo jwi
      //DDS::BuiltinTopicKey_to_GUID(&participant_guid, data_seq[i].participant_key);
      add_information(
        participant_guid,
        info_seq[i].instance_handle,
        data_seq[i].topic_name.in (),
        data_seq[i].type_name.in (),
        EntityType::Publisher);
    } else {
      remove_information(
        info_seq[i].instance_handle,
        EntityType::Publisher);
    }
  }

  if (data_seq.length() > 0) {
    this->trigger_graph_guard_condition();
  }

  builtin_reader->return_loan(data_seq, info_seq);
}
