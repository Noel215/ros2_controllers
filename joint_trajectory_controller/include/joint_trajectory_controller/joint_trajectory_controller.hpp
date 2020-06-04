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

#ifndef JOINT_TRAJECTORY_CONTROLLER__JOINT_TRAJECTORY_CONTROLLER_HPP_
#define JOINT_TRAJECTORY_CONTROLLER__JOINT_TRAJECTORY_CONTROLLER_HPP_

#include <memory>
#include <string>
#include <vector>

#include "controller_interface/controller_interface.hpp"

#include "control_msgs/msg/joint_trajectory_controller_state.hpp"
#include "control_msgs/action/follow_joint_trajectory.hpp"

#include "hardware_interface/joint_command_handle.hpp"
#include "hardware_interface/joint_state_handle.hpp"
#include "hardware_interface/operation_mode_handle.hpp"
#include "hardware_interface/robot_hardware.hpp"

#include "joint_trajectory_controller/trajectory.hpp"
#include "joint_trajectory_controller/tolerances.hpp"
#include "joint_trajectory_controller/visibility_control.h"

#include "rclcpp_action/rclcpp_action.hpp"

#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "rcutils/time.h"

#include "realtime_tools/realtime_buffer.h"
#include "realtime_tools/realtime_publisher.h"
#include "realtime_tools/realtime_server_goal_handle.h"

#include "trajectory_msgs/msg/joint_trajectory.hpp"
#include "trajectory_msgs/msg/joint_trajectory_point.hpp"

namespace joint_trajectory_controller
{

class JointTrajectoryController : public controller_interface::ControllerInterface
{
public:
  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  JointTrajectoryController();

  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  JointTrajectoryController(
    const std::vector<std::string> & joint_names,
    const std::vector<std::string> & write_op_names);

  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  controller_interface::controller_interface_ret_t
  init(
    std::weak_ptr<hardware_interface::RobotHardware> robot_hardware,
    const std::string & controller_name) override;

  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  controller_interface::controller_interface_ret_t
  update() override;

  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_configure(const rclcpp_lifecycle::State & previous_state) override;

  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_activate(const rclcpp_lifecycle::State & previous_state) override;

  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_deactivate(const rclcpp_lifecycle::State & previous_state) override;

  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_cleanup(const rclcpp_lifecycle::State & previous_state) override;

  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_error(const rclcpp_lifecycle::State & previous_state) override;

  JOINT_TRAJECTORY_CONTROLLER_PUBLIC
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn
  on_shutdown(const rclcpp_lifecycle::State & previous_state) override;

private:
  std::vector<std::string> joint_names_;
  std::vector<std::string> write_op_names_;

  std::vector<hardware_interface::JointCommandHandle *> registered_joint_cmd_handles_;
  std::vector<const hardware_interface::JointStateHandle *> registered_joint_state_handles_;
  std::vector<hardware_interface::OperationModeHandle *> registered_operation_mode_handles_;

  // TODO(karsten1987): eventually activate and deactive subscriber directly when its supported
  bool subscriber_is_active_ = false;
  rclcpp::Subscription<trajectory_msgs::msg::JointTrajectory>::SharedPtr
    joint_command_subscriber_ = nullptr;

  std::shared_ptr<Trajectory> * traj_point_active_ptr_ = nullptr;
  std::shared_ptr<Trajectory> traj_external_point_ptr_ = nullptr;
  std::shared_ptr<Trajectory> traj_home_point_ptr_ = nullptr;
  std::shared_ptr<trajectory_msgs::msg::JointTrajectory> traj_msg_home_ptr_ = nullptr;

  bool is_halted = false;

  using ControllerStateMsg = control_msgs::msg::JointTrajectoryControllerState;
  using StatePublisher = realtime_tools::RealtimePublisher<ControllerStateMsg>;
  using StatePublisherPtr = std::unique_ptr<StatePublisher>;
  rclcpp_lifecycle::LifecyclePublisher<ControllerStateMsg>::SharedPtr publisher_;
  StatePublisherPtr state_publisher_;

  rclcpp::Duration state_publisher_period_ = rclcpp::Duration(RCUTILS_MS_TO_NS(20));
  rclcpp::Time last_state_publish_time_;

  using FollowJTrajAction = control_msgs::action::FollowJointTrajectory;
  using RealtimeGoalHandle = realtime_tools::RealtimeServerGoalHandle<FollowJTrajAction>;
  using RealtimeGoalHandlePtr = std::shared_ptr<RealtimeGoalHandle>;

  rclcpp_action::Server<FollowJTrajAction>::SharedPtr action_server_;
  bool allow_partial_joints_goal_;
  RealtimeGoalHandlePtr rt_active_goal_;     ///< Currently active action goal, if any.
  rclcpp::TimerBase::SharedPtr goal_handle_timer_;
  rclcpp::Duration action_monitor_period_ = rclcpp::Duration(RCUTILS_MS_TO_NS(50));
  std::mutex trajectory_mtx_;

  // callbacks for action_server_
  rclcpp_action::GoalResponse goal_callback(
    const rclcpp_action::GoalUUID & uuid,
    std::shared_ptr<const FollowJTrajAction::Goal> goal);
  rclcpp_action::CancelResponse cancel_callback(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<FollowJTrajAction>> goal_handle);
  void feedback_setup_callback(
    std::shared_ptr<rclcpp_action::ServerGoalHandle<FollowJTrajAction>> goal_handle);

  SegmentTolerances default_tolerances_;

  void preempt_active_goal();
  void set_hold_position();

  bool reset();
  void set_op_mode(const hardware_interface::OperationMode & mode);
  void halt();

  using JointTrajectoryPoint = trajectory_msgs::msg::JointTrajectoryPoint;
  void publish_state(
    const JointTrajectoryPoint & desired_state,
    const JointTrajectoryPoint & current_state,
    const JointTrajectoryPoint & state_error);
};

}  // namespace joint_trajectory_controller

#endif  // JOINT_TRAJECTORY_CONTROLLER__JOINT_TRAJECTORY_CONTROLLER_HPP_