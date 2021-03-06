// Copyright 2019 Intelligent Robotics Lab
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
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "plansys2_executor/ActionExecutorClient.hpp"

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_cascade_lifecycle/rclcpp_cascade_lifecycle.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

#include "gtest/gtest.h"

class ActionExecutorClientTest : public plansys2::ActionExecutorClient
{
public:
  ActionExecutorClientTest()
  : ActionExecutorClient("assemble", 2.0f),
    counter_(0), executions_(0)
  {
  }

  void atStart()
  {
    executions_++;
    counter_ = 0;
  }

  void actionStep()
  {
    RCLCPP_INFO(get_logger(), "Executing assemble");
    counter_++;
  }

  bool isFinished()
  {
    return counter_ > 5;
  }

  int counter_;
  int executions_;
};

TEST(executor_client, normal_client)
{
  using ExecuteAction = plansys2_msgs::action::ExecuteAction;
  using GoalHandleExecuteAction = rclcpp_action::ClientGoalHandle<ExecuteAction>;

  auto exc = std::make_shared<ActionExecutorClientTest>();
  auto test_node = std::make_shared<rclcpp::Node>("test_node");
  auto action_client = rclcpp_action::create_client<ExecuteAction>(test_node, "assemble");

  rclcpp::executors::MultiThreadedExecutor executor;
  executor.add_node(exc->get_node_base_interface());
  executor.add_node(test_node);


  RCLCPP_INFO(test_node->get_logger(), "Waiting for action server");
  action_client->wait_for_action_server();

  auto goal_msg = ExecuteAction::Goal();
  goal_msg.action = "assemble";
  auto send_goal_options = rclcpp_action::Client<ExecuteAction>::SendGoalOptions();
  auto future_goal_handle = action_client->async_send_goal(goal_msg, send_goal_options);

  ASSERT_EQ(exc->get_current_state().id(), lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE);

  ASSERT_EQ(exc->executions_, 0);
  ASSERT_EQ(exc->counter_, 0);

  rclcpp::Rate rate(10);
  auto start = test_node->now();
  while (rclcpp::ok() && (test_node->now() - start).seconds() < 3) {
    executor.spin_some();
    rate.sleep();

    if (!exc->isFinished()) {
      // ASSERT_EQ(exc->get_current_state().id(), lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE);
    }
  }

  ASSERT_EQ(exc->get_current_state().id(), lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE);
  ASSERT_EQ(exc->executions_, 1);
  ASSERT_EQ(exc->counter_, 6);

  future_goal_handle = action_client->async_send_goal(goal_msg, send_goal_options);

  start = test_node->now();
  while (rclcpp::ok() && (test_node->now() - start).seconds() < 3) {
    executor.spin_some();
    rate.sleep();

    if (!exc->isFinished()) {
      ASSERT_EQ(exc->get_current_state().id(), lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE);
    }
  }

  ASSERT_EQ(exc->get_current_state().id(), lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE);
  ASSERT_EQ(exc->executions_, 2);
  ASSERT_EQ(exc->counter_, 6);
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
