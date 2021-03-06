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

#include "rcutils/filesystem.h"

#include "rmw_opendds_shared_cpp/guard_condition.hpp"
#include "rmw_opendds_shared_cpp/opendds_include.hpp"
#include "rmw_opendds_shared_cpp/node.hpp"
#include "rmw_opendds_shared_cpp/types.hpp"

#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/impl/cpp/macros.hpp"

#include "dds/DdsDcpsCoreTypeSupportC.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/BuiltInTopicUtils.h"

rmw_node_t *
create_node(
  const char * implementation_identifier,
  const char * name,
  const char * namespace_,
  size_t domain_id,
  const rmw_node_security_options_t * security_options)
{
  if (!security_options) {
    RMW_SET_ERROR_MSG("security_options is null");
    return nullptr;
  }
  DDS::DomainParticipantFactory * dpf_ = TheParticipantFactory;
  if (!dpf_) {
    RMW_SET_ERROR_MSG("failed to get participant factory");
    return NULL;
  }

  // use loopback interface to enable cross vendor communication
  DDS::DomainParticipantQos participant_qos;
  DDS::ReturnCode_t status = dpf_->get_default_participant_qos(participant_qos);
  if (status != DDS::RETCODE_OK) {
    RMW_SET_ERROR_MSG("failed to get default participant qos");
    return NULL;
  }
  // since the participant name is not part of the DDS spec
  // the node name is also set in the user_data
  size_t length = strlen(name) + strlen("name=;") +
    strlen(namespace_) + strlen("namespace=;") + 1;
  participant_qos.user_data.value.length(static_cast<CORBA::Long>(length));

  int written =
    snprintf(reinterpret_cast<char *>(participant_qos.user_data.value.get_buffer()),
      length, "name=%s;namespace=%s;", name, namespace_);
  if (written < 0 || written > static_cast<int>(length) - 1) {
    RMW_SET_ERROR_MSG("failed to populate user_data buffer");
    return NULL;
  }

  rmw_node_t * node_handle = nullptr;
  OpenDDSNodeInfo * node_info = nullptr;
  rmw_guard_condition_t * graph_guard_condition = nullptr;
  CustomPublisherListener * publisher_listener = nullptr;
  CustomSubscriberListener * subscriber_listener = nullptr;
  void * buf = nullptr;

  DDS::DomainParticipant * participant = nullptr;
  DDS::DataReader * data_reader = nullptr;
  DDS::PublicationBuiltinTopicDataDataReader * builtin_publication_datareader = nullptr;
  DDS::SubscriptionBuiltinTopicDataDataReader * builtin_subscription_datareader = nullptr;
  DDS::Subscriber * builtin_subscriber = nullptr;

  rcutils_allocator_t allocator = rcutils_get_default_allocator();

  const char * srp = nullptr;
  char * identity_ca_cert_fn = nullptr;
  char * permissions_ca_cert_fn = nullptr;
  char * cert_fn = nullptr;
  char * key_fn = nullptr;
  char * gov_fn = nullptr;
  char * perm_fn = nullptr;

  if (security_options->security_root_path) {
    // enable some security stuff
    srp = security_options->security_root_path;  // save some typing
    identity_ca_cert_fn = rcutils_join_path(srp, "identity_ca.cert.pem", allocator);
    if (!identity_ca_cert_fn) {
      RMW_SET_ERROR_MSG("failed to allocate memory for 'identity_ca_cert_fn'");
      goto fail;
    }
    permissions_ca_cert_fn = rcutils_join_path(srp, "permissions_ca.cert.pem", allocator);
    if (!permissions_ca_cert_fn) {
      RMW_SET_ERROR_MSG("failed to allocate memory for 'permissions_ca_cert_fn'");
      goto fail;
    }
    cert_fn = rcutils_join_path(srp, "cert.pem", allocator);
    if (!cert_fn) {
      RMW_SET_ERROR_MSG("failed to allocate memory for 'cert_fn'");
      goto fail;
    }
    key_fn = rcutils_join_path(srp, "key.pem", allocator);
    if (!key_fn) {
      RMW_SET_ERROR_MSG("failed to allocate memory for 'key_fn'");
      goto fail;
    }
    gov_fn = rcutils_join_path(srp, "governance.p7s", allocator);
    if (!gov_fn) {
      RMW_SET_ERROR_MSG("failed to allocate memory for 'gov_fn'");
      goto fail;
    }
    perm_fn = rcutils_join_path(srp, "permissions.p7s", allocator);
    if (!perm_fn) {
      RMW_SET_ERROR_MSG("failed to allocate memory for 'perm_fn'");
      goto fail;
    }
  }

  participant = dpf_->create_participant(
    static_cast<DDS::DomainId_t>(domain_id), participant_qos, NULL,
    0);
  if (!participant) {
    RMW_SET_ERROR_MSG("failed to create participant");
    goto fail;
  }

  builtin_subscriber = participant->get_builtin_subscriber();
  if (!builtin_subscriber) {
    RMW_SET_ERROR_MSG("builtin subscriber handle is null");
    goto fail;
  }

  // setup publisher listener
  data_reader = builtin_subscriber->lookup_datareader(OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC);
  builtin_publication_datareader =
    dynamic_cast<DDS::PublicationBuiltinTopicDataDataReader *>(data_reader);
  if (!builtin_publication_datareader) {
    RMW_SET_ERROR_MSG("builtin publication datareader handle is null");
    goto fail;
  }

  graph_guard_condition = create_guard_condition(implementation_identifier);
  if (!graph_guard_condition) {
    RMW_SET_ERROR_MSG("failed to create graph guard condition");
    goto fail;
  }

  buf = rmw_allocate(sizeof(CustomPublisherListener));
  if (!buf) {
    RMW_SET_ERROR_MSG("failed to allocate memory");
    goto fail;
  }
  RMW_TRY_PLACEMENT_NEW(
    publisher_listener, buf, goto fail, CustomPublisherListener,
    implementation_identifier, graph_guard_condition)
  buf = nullptr;
  builtin_publication_datareader->set_listener(publisher_listener, DDS::DATA_AVAILABLE_STATUS);

  data_reader = builtin_subscriber->lookup_datareader(OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC);
  builtin_subscription_datareader =
    dynamic_cast<DDS::SubscriptionBuiltinTopicDataDataReader *>(data_reader);
  if (!builtin_subscription_datareader) {
    RMW_SET_ERROR_MSG("builtin subscription datareader handle is null");
    goto fail;
  }

  // setup subscriber listener
  buf = rmw_allocate(sizeof(CustomSubscriberListener));
  if (!buf) {
    RMW_SET_ERROR_MSG("failed to allocate memory");
    goto fail;
  }
  RMW_TRY_PLACEMENT_NEW(
    subscriber_listener, buf, goto fail, CustomSubscriberListener,
    implementation_identifier, graph_guard_condition)
  buf = nullptr;
  builtin_subscription_datareader->set_listener(subscriber_listener, DDS::DATA_AVAILABLE_STATUS);

  node_handle = rmw_node_allocate();
  if (!node_handle) {
    RMW_SET_ERROR_MSG("failed to allocate memory for node handle");
    goto fail;
  }
  node_handle->implementation_identifier = implementation_identifier;
  node_handle->data = participant;

  node_handle->name =
    reinterpret_cast<const char *>(rmw_allocate(sizeof(char) * strlen(name) + 1));
  if (!node_handle->name) {
    RMW_SET_ERROR_MSG("failed to allocate memory for node name");
    goto fail;
  }
  memcpy(const_cast<char *>(node_handle->name), name, strlen(name) + 1);

  node_handle->namespace_ =
    reinterpret_cast<const char *>(rmw_allocate(sizeof(char) * strlen(namespace_) + 1));
  if (!node_handle->namespace_) {
    RMW_SET_ERROR_MSG("failed to allocate memory for node namespace");
    goto fail;
  }
  memcpy(const_cast<char *>(node_handle->namespace_), namespace_, strlen(namespace_) + 1);

  buf = rmw_allocate(sizeof(OpenDDSNodeInfo));
  if (!buf) {
    RMW_SET_ERROR_MSG("failed to allocate memory");
    goto fail;
  }
  RMW_TRY_PLACEMENT_NEW(node_info, buf, goto fail, OpenDDSNodeInfo, )
  buf = nullptr;
  node_info->participant = participant;
  node_info->publisher_listener = publisher_listener;
  node_info->subscriber_listener = subscriber_listener;
  node_info->graph_guard_condition = graph_guard_condition;

  node_handle->implementation_identifier = implementation_identifier;
  node_handle->data = node_info;
  return node_handle;
fail:
  status = dpf_->delete_participant(participant);
  if (status != DDS::RETCODE_OK) {
    std::stringstream ss;
    ss << "leaking participant while handling failure at " <<
      __FILE__ << ":" << __LINE__;
    (std::cerr << ss.str()).flush();
  }
  if (graph_guard_condition) {
    rmw_ret_t ret = destroy_guard_condition(implementation_identifier, graph_guard_condition);
    if (ret != RMW_RET_OK) {
      std::stringstream ss;
      ss << "failed to destroy guard condition while handling failure at " <<
        __FILE__ << ":" << __LINE__;
      (std::cerr << ss.str()).flush();
    }
  }
  if (publisher_listener) {
    RMW_TRY_DESTRUCTOR_FROM_WITHIN_FAILURE(
      publisher_listener->~CustomPublisherListener(), CustomPublisherListener)
    rmw_free(publisher_listener);
  }
  if (subscriber_listener) {
    RMW_TRY_DESTRUCTOR_FROM_WITHIN_FAILURE(
      subscriber_listener->~CustomSubscriberListener(), CustomSubscriberListener)
    rmw_free(subscriber_listener);
  }
  if (node_handle) {
    if (node_handle->name) {
      rmw_free(const_cast<char *>(node_handle->name));
    }
    if (node_handle->namespace_) {
      rmw_free(const_cast<char *>(node_handle->namespace_));
    }
    rmw_free(node_handle);
  }
  if (node_info) {
    RMW_TRY_DESTRUCTOR_FROM_WITHIN_FAILURE(
      node_info->~OpenDDSNodeInfo(), OpenDDSNodeInfo)
    rmw_free(node_info);
  }
  if (buf) {
    rmw_free(buf);
  }
  // Note: allocator.deallocate(nullptr, ...); is allowed.
  allocator.deallocate(identity_ca_cert_fn, allocator.state);
  allocator.deallocate(permissions_ca_cert_fn, allocator.state);
  allocator.deallocate(cert_fn, allocator.state);
  allocator.deallocate(key_fn, allocator.state);
  allocator.deallocate(gov_fn, allocator.state);
  allocator.deallocate(perm_fn, allocator.state);
  return NULL;
}

rmw_ret_t
destroy_node(const char * implementation_identifier, rmw_node_t * node)
{
  if (!node) {
    RMW_SET_ERROR_MSG("node handle is null");
    return RMW_RET_ERROR;
  }
  RMW_CHECK_TYPE_IDENTIFIERS_MATCH(
    node handle,
    node->implementation_identifier, implementation_identifier,
    return RMW_RET_ERROR)

  DDS::DomainParticipantFactory * dpf_ = TheParticipantFactory;
  if (!dpf_) {
    RMW_SET_ERROR_MSG("failed to get participant factory");
    return RMW_RET_ERROR;
  }

  auto node_info = static_cast<OpenDDSNodeInfo *>(node->data);
  if (!node_info) {
    RMW_SET_ERROR_MSG("node info handle is null");
    return RMW_RET_ERROR;
  }
  auto participant = static_cast<DDS::DomainParticipant *>(node_info->participant);
  if (!participant) {
    RMW_SET_ERROR_MSG("participant handle is null");
  }
  // This unregisters types and destroys topics which were shared between
  // publishers and subscribers and could not be cleaned up in the delete functions.
  if (participant->delete_contained_entities() != DDS::RETCODE_OK) {
    RMW_SET_ERROR_MSG("failed to delete contained entities of participant");
    return RMW_RET_ERROR;
  }

  DDS::ReturnCode_t ret = dpf_->delete_participant(participant);
  if (ret != DDS::RETCODE_OK) {
    RMW_SET_ERROR_MSG("failed to delete participant");
    return RMW_RET_ERROR;
  }

  if (node_info->publisher_listener) {
    RMW_TRY_DESTRUCTOR_FROM_WITHIN_FAILURE(
      node_info->publisher_listener->~CustomPublisherListener(), CustomPublisherListener)
    rmw_free(node_info->publisher_listener);
    node_info->publisher_listener = nullptr;
  }
  if (node_info->subscriber_listener) {
    RMW_TRY_DESTRUCTOR_FROM_WITHIN_FAILURE(
      node_info->subscriber_listener->~CustomSubscriberListener(), CustomSubscriberListener)
    rmw_free(node_info->subscriber_listener);
    node_info->subscriber_listener = nullptr;
  }
  if (node_info->graph_guard_condition) {
    rmw_ret_t rmw_ret =
      destroy_guard_condition(implementation_identifier, node_info->graph_guard_condition);
    if (rmw_ret != RMW_RET_OK) {
      RMW_SET_ERROR_MSG("failed to delete graph guard condition");
      return RMW_RET_ERROR;
    }
    node_info->graph_guard_condition = nullptr;
  }

  rmw_free(node_info);
  node->data = nullptr;
  rmw_free(const_cast<char *>(node->name));
  node->name = nullptr;
  rmw_free(const_cast<char *>(node->namespace_));
  node->namespace_ = nullptr;
  rmw_node_free(node);

  return RMW_RET_OK;
}

RMW_OPENDDS_SHARED_CPP_PUBLIC
const rmw_guard_condition_t *
node_get_graph_guard_condition(const rmw_node_t * node)
{
  // node argument is checked in calling function.

  OpenDDSNodeInfo * node_info = static_cast<OpenDDSNodeInfo *>(node->data);
  if (!node_info) {
    RMW_SET_ERROR_MSG("node info handle is null");
    return nullptr;
  }

  return node_info->graph_guard_condition;
}
