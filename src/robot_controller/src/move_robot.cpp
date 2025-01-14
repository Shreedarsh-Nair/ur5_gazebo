#include <moveit/move_group_interface/move_group_interface.h>
#include <moveit/planning_scene_interface/planning_scene_interface.h>
#include <moveit_msgs/msg/display_robot_state.hpp>
#include <moveit_msgs/msg/display_trajectory.hpp>
#include <moveit_msgs/msg/attached_collision_object.hpp>
#include <moveit_msgs/msg/collision_object.hpp>
#include "rclcpp/rclcpp.hpp"
#include <string>
#include "sensor_msgs/msg/joint_state.hpp"

static const rclcpp::Logger LOGGER = rclcpp::get_logger("move_group_demo");
static const std::string PLANNING_GROUP_ARM = "ur_manipulator";

class TestTrajectory : public rclcpp::Node {
public:
  TestTrajectory(std::shared_ptr<rclcpp::Node> move_group_node)
      : Node("test_trajectory"),
        move_group_arm(move_group_node, PLANNING_GROUP_ARM),
        joint_model_group_arm(
            move_group_arm.getCurrentState()->getJointModelGroup(
                PLANNING_GROUP_ARM)) {
    

    joint_state_sub_ = this->create_subscription<sensor_msgs::msg::JointState>(
      "joint_states", 10, std::bind(&TestTrajectory::joint_state_callback, this, std::placeholders::_1));

    this->timer_ =
        this->create_wall_timer(std::chrono::milliseconds(500),
                                std::bind(&TestTrajectory::timer_callback, this));
  }

  void get_info() {
    RCLCPP_INFO(LOGGER, "Planning frame: %s", move_group_arm.getPlanningFrame().c_str());
    RCLCPP_INFO(LOGGER, "End-effector link: %s", move_group_arm.getEndEffectorLink().c_str());
    RCLCPP_INFO(LOGGER, "Available Planning Groups:");
    std::copy(move_group_arm.getJointModelGroupNames().begin(), move_group_arm.getJointModelGroupNames().end(),
                std::ostream_iterator<std::string>(std::cout, ", "));
  }

  void current_state() {
    RCLCPP_INFO(LOGGER, "Get Robot Current State");
    current_state_arm = move_group_arm.getCurrentState(10);
    if (!current_state_arm) {
      RCLCPP_ERROR(LOGGER, "Failed to get current state");
      return;
    }
    current_state_arm->copyJointGroupPositions(this->joint_model_group_arm,
                                               this->joint_group_positions_arm);
  }

  void plan_arm_joint_space() {
    RCLCPP_INFO(LOGGER, "Planning to Joint Space");
    joint_group_positions_arm[0] = 0.00;  // Shoulder Pan
    joint_group_positions_arm[1] = -2.50;  // Shoulder Lift
    joint_group_positions_arm[2] = 1.50;  // Elbow
    joint_group_positions_arm[3] = -1.50;  // Wrist 1
    joint_group_positions_arm[4] = -1.55;  // Wrist 2
    joint_group_positions_arm[5] = 0.00;  // Wrist 3

    move_group_arm.setJointValueTarget(joint_group_positions_arm);

    bool success_arm = (move_group_arm.plan(my_plan_arm) ==
                        moveit::planning_interface::MoveItErrorCode::SUCCESS);
    if (!success_arm) {
      RCLCPP_ERROR(LOGGER, "Planning failed");
      return;
    }
    
    moveit::planning_interface::MoveItErrorCode execute_result = move_group_arm.execute(my_plan_arm);
    if (execute_result != moveit::planning_interface::MoveItErrorCode::SUCCESS) {
      RCLCPP_ERROR(LOGGER, "Execution failed");
    }
  }

  void timer_callback() {
    if (!move_group_arm.getCurrentState()) {
      RCLCPP_WARN(LOGGER, "Failed to get current state. Retrying in 1 second.");
      this->timer_ = this->create_wall_timer(std::chrono::seconds(1),
                                             std::bind(&TestTrajectory::timer_callback, this));
      return;
    }
    this->timer_->cancel();
    get_info();
    current_state();
    plan_arm_joint_space();
  }

  void joint_state_callback(const sensor_msgs::msg::JointState::SharedPtr msg) {
    RCLCPP_INFO(LOGGER, "Received joint states. First joint position: %f", msg->position[0]);
    // You can do more with the joint state data here
  }

private:
  moveit::planning_interface::PlanningSceneInterface planning_scene_interface;
  std::vector<double> joint_group_positions_arm;
  moveit::planning_interface::MoveGroupInterface::Plan my_plan_arm;
  rclcpp::TimerBase::SharedPtr timer_;
  moveit::planning_interface::MoveGroupInterface move_group_arm;
  const moveit::core::JointModelGroup *joint_model_group_arm;
  moveit::core::RobotStatePtr current_state_arm;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_state_sub_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  rclcpp::NodeOptions node_options;
  node_options.automatically_declare_parameters_from_overrides(true);
  auto move_group_node =
      rclcpp::Node::make_shared("move_group_demo", node_options);

  rclcpp::executors::SingleThreadedExecutor planner_executor;
  std::shared_ptr<TestTrajectory> planner_node =
      std::make_shared<TestTrajectory>(move_group_node);
  planner_executor.add_node(planner_node);
  planner_executor.spin();

  rclcpp::shutdown();
  return 0;
}