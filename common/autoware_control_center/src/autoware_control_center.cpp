// Copyright 2022 The Autoware Contributors
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

#include "autoware_control_center/autoware_control_center.hpp"

#include "autoware_control_center/node_registry.hpp"

#include <tier4_autoware_utils/ros/uuid_helper.hpp>

#include "autoware_control_center_msgs/srv/autoware_control_center_deregister.hpp"
#include <unique_identifier_msgs/msg/uuid.hpp>

#include <chrono>

using namespace std::chrono_literals;

namespace autoware_control_center
{
unique_identifier_msgs::msg::UUID createDefaultUUID()
{
  unique_identifier_msgs::msg::UUID default_uuid;

  // Use std::generate to fill the UUID with zeros
  std::generate(default_uuid.uuid.begin(), default_uuid.uuid.end(), []() { return 0; });

  return default_uuid;
}

AutowareControlCenter::AutowareControlCenter(const rclcpp::NodeOptions & options)
: LifecycleNode("autoware_control_center", options)
{
  // log info
  RCLCPP_INFO(get_logger(), "AutowareControlCenter is initialized");

  callback_group_mut_ex_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

  using std::placeholders::_1;
  using std::placeholders::_2;
  srv_register_ = create_service<autoware_control_center_msgs::srv::AutowareNodeRegister>(
    "~/srv/autoware_node_register", std::bind(&AutowareControlCenter::register_node, this, _1, _2),
    rmw_qos_profile_services_default, callback_group_mut_ex_);
  srv_deregister_ = create_service<autoware_control_center_msgs::srv::AutowareNodeDeregister>(
    "~/srv/autoware_node_deregister",
    std::bind(&AutowareControlCenter::deregister_node, this, _1, _2),
    rmw_qos_profile_services_default, callback_group_mut_ex_);

  acc_uuid = tier4_autoware_utils::generateUUID();
  countdown = 10;
  startup_timer_ =
    this->create_wall_timer(500ms, std::bind(&AutowareControlCenter::startup_callback, this));
}

void AutowareControlCenter::register_node(
  const autoware_control_center_msgs::srv::AutowareNodeRegister::Request::SharedPtr request,
  const autoware_control_center_msgs::srv::AutowareNodeRegister::Response::SharedPtr response)
{
  RCLCPP_INFO(get_logger(), "register_node is called from %s", request->name_node.c_str());

  std::optional<unique_identifier_msgs::msg::UUID> node_uuid =
    node_registry_.register_node(request->name_node.c_str(), tier4_autoware_utils::generateUUID());

  if (node_uuid == std::nullopt) {
    response->uuid_node = createDefaultUUID();
    response->status.status =
      autoware_control_center_msgs::srv::AutowareNodeRegister::Response::_status_type::FAILURE;
  } else {
    response->uuid_node = node_uuid.value();
    response->status.status =
      autoware_control_center_msgs::srv::AutowareNodeRegister::Response::_status_type::SUCCESS;
  }
}

void AutowareControlCenter::deregister_node(
  const autoware_control_center_msgs::srv::AutowareNodeDeregister::Request::SharedPtr request,
  const autoware_control_center_msgs::srv::AutowareNodeDeregister::Response::SharedPtr response)
{
  RCLCPP_INFO(get_logger(), "deregister_node is called from %s", request->name_node.c_str());

  std::optional<unique_identifier_msgs::msg::UUID> node_uuid =
    node_registry_.deregister_node(request->name_node.c_str());

  if (node_uuid == std::nullopt) {
    response->uuid_node = createDefaultUUID();
    response->status.status =
      autoware_control_center_msgs::srv::AutowareNodeDeregister::Response::_status_type::FAILURE;
  } else {
    response->uuid_node = node_uuid.value();
    response->status.status =
      autoware_control_center_msgs::srv::AutowareNodeDeregister::Response::_status_type::SUCCESS;
  }
}

void AutowareControlCenter::startup_callback()
{
  // wait for 10 sec and
  if (countdown < 1 && node_registry_.is_empty()) {
    RCLCPP_INFO(
      get_logger(), "Startup timeout is over. Map is empty. Start re-registering procedure.");
    // list auwoware nodes
    // iterate list, create client and send calls
    RCLCPP_INFO(get_logger(), "List services.");
    std::map<std::string, std::vector<std::string>> srv_list = this->get_service_names_and_types();
    for (auto const & pair : srv_list) {
      RCLCPP_INFO(get_logger(), pair.first.c_str());
    }
    auto it = srv_list.begin();
    // filter out srv with type autoware_control_center_msgs/srv/AutowareControlCenterDeregister
    while (it != srv_list.end()) {
      if (it->second[0] != "autoware_control_center_msgs/srv/AutowareControlCenterDeregister") {
        srv_list.erase(it++);
      } else {
        ++it;
      }
    }
    RCLCPP_INFO(get_logger(), "Filtered service list");
    for (auto const & pair : srv_list) {
      RCLCPP_INFO(get_logger(), pair.first.c_str());
    }
    // create srv
    rclcpp::Client<autoware_control_center_msgs::srv::AutowareControlCenterDeregister>::SharedPtr
      dereg_client_ =
        create_client<autoware_control_center_msgs::srv::AutowareControlCenterDeregister>(
          "/test_node/srv/acc_deregister");
    // create request
    autoware_control_center_msgs::srv::AutowareControlCenterDeregister::Request::SharedPtr req =
      std::make_shared<
        autoware_control_center_msgs::srv::AutowareControlCenterDeregister::Request>();

    req->uuid_acc = acc_uuid;

    using ServiceResponseFuture = rclcpp::Client<
      autoware_control_center_msgs::srv::AutowareControlCenterDeregister>::SharedFuture;
    // lambda for async request
    auto response_received_callback = [this](ServiceResponseFuture future) {
      auto response = future.get();
      RCLCPP_INFO(
        get_logger(), "response: %d, %s", response->status.status, response->name_node.c_str());

      if (response->status.status == 1) {
        RCLCPP_INFO(get_logger(), "Node was deregistered");
      } else {
        RCLCPP_ERROR(get_logger(), "Failed to deregister node");
      }
    };

    auto future_result = dereg_client_->async_send_request(req, response_received_callback);
    RCLCPP_INFO(get_logger(), "Sent request");
  }
  // check if some node has been registered
  if (node_registry_.is_empty()) {
    RCLCPP_INFO(get_logger(), "Node register map is empty. Countdown is %d", countdown);
  } else {
    RCLCPP_INFO(get_logger(), "Some node was registered. Stop timer.");
    this->startup_timer_->cancel();
  }
  countdown -= 1;
}
}  // namespace autoware_control_center
