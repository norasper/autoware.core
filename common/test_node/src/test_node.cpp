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

#include "test_node/test_node.hpp"

#include "autoware_node/autoware_node.hpp"

namespace test_node
{

TestNode::TestNode(const rclcpp::NodeOptions & options)
: autoware_node::AutowareNode("test_node", "", options)
{
  RCLCPP_INFO(get_logger(), "TestNode constructor with name %s", autoware_node::AutowareNode::self_name.c_str());
}

}  // namespace test_node

#include "rclcpp_components/register_node_macro.hpp"

// Register the component with class_loader.
// This acts as a sort of entry point, allowing the component to be discoverable when its library
// is being loaded into a running process.
RCLCPP_COMPONENTS_REGISTER_NODE(test_node::TestNode)
