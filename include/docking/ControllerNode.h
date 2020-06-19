#ifndef CONTROLLERNODE_H
#define CONTROLLERNODE_H

#include <docking/Headers.h>
#include <docking/TFHelpers.h>
#include <docking/Helpers.h>
#include <docking/PCLHelpers.h>
#include <ros/ros.h>
#include <docking/PoseControllerConfig.h>
#include <docking/ControllerNodeConfig.h>
#include <docking/Plan.h>
#include <docking/DockingAction.h>
// Dynamic reconfigure includes.
#include <dynamic_reconfigure/server.h>

#include <tf/transform_listener.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Scalar.h>
#include <tf2/LinearMath/Vector3.h>
#include <tf2/convert.h>
#include <tf2/utils.h>
#include <tf2/impl/utils.h>
#include <tf2/impl/convert.h>
#include <tf2/transform_datatypes.h>

#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Transform.h>
#include <geometry_msgs/TransformStamped.h>
#include <geometry_msgs/Quaternion.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/PoseArray.h>
#include <nav_msgs/Path.h>
#include <angles/angles.h>
#include <math.h>


class ControllerNode
{
public:
  ControllerNode(ros::NodeHandle& nh) :
    nh_(nh), tfListener_(tfBuffer_)
  {
//    nh_ = nh;
    ROS_INFO_STREAM("INITIALIZING CONTROLLER NODE OBJECT");
    ROS_INFO_STREAM("STARTING DYNAMIC RECONFIGURE SERVER");
    startDynamicReconfigureServer();
    startPub();
    startPlanSub();
    initGlobals();
  }
  ~ControllerNode(){
    cmd_vel_pub_.publish(zeroTwist_);
  }

  ros::NodeHandle nh_;
  ros::Publisher cmd_vel_pub_;  // Publisher of commands
  ros::Publisher detectionActivationPub;
  ros::Publisher planningActivationPub;
  ros::Subscriber planSub_;
  ros::Subscriber dockPoseSub_;
  tf2_ros::Buffer tfBuffer_;
  tf2_ros::TransformListener tfListener_;
  tf2_ros::TransformBroadcaster tfBroadcaster_;

  //! Bool of detection node status
  std_msgs::Bool perform_detection_;
  //! Bool of planning node status
  std_msgs::Bool perform_planning_;

  docking::Plan plan_;
  std::string plan_topic_;
  std::string dock_pose_topic_;
  std::string cmd_vel_topic_;
  geometry_msgs::PoseStamped dock_pose_;
  geometry_msgs::Twist zeroTwist_;
  bool publish_twist_;
  double goal_dist_tolerance_;
  bool within_goal_dist_tolerance_;
  double goal_orientation_tolerance_;
  bool within_goal_orientation_tolerance_;
  double time_step_;
  ros::Duration time_step_duration_;
  double entrance_dist_;
  //! Dynamic reconfigure server.
  dynamic_reconfigure::Server<docking::ControllerNodeConfig> dr_srv_;
  //! Dynamic Reconfig Variables


  // Declaration and Definition
  void startDynamicReconfigureServer() {
  // Set up a dynamic reconfigure server.
  // Do this before parameter server, else some of the parameter server values can be overwritten.
    dynamic_reconfigure::Server<docking::ControllerNodeConfig>::CallbackType cb;
    cb = boost::bind(&ControllerNode::configCallback, this, _1, _2);
    dr_srv_.setCallback(cb);
  }

  //! Callback function for dynamic reconfigure server.
  void configCallback(docking::ControllerNodeConfig &config, uint32_t level __attribute__((unused))) {

    if((config.publish_twist == false) && (publish_twist_ == true)){
      cmd_vel_pub_.publish(zeroTwist_);
    }
    cmd_vel_topic_ = config.cmd_vel_topic;
    publish_twist_ = config.publish_twist;
    time_step_ = config.time_step;
    goal_orientation_tolerance_ = config.goal_orientation_tolerance;
    goal_dist_tolerance_ = config.goal_dist_tolerance;
    dock_pose_topic_ = config.dock_pose_topic;
    plan_topic_ = config.plan_topic;
//    frequency_ = config.frequency;
    perform_detection_.data = config.perform_detection;
    ROS_INFO_STREAM("SETTING DETECTION ACTIVATION STATUS TO  " << perform_detection_.data);
    perform_planning_.data = config.perform_planning;
    ROS_INFO_STREAM("SETTING PLANNING ACTIVATION STATUS TO  " << config.perform_planning);
  }

  void publishActivation(){
    ROS_INFO_STREAM("PUBLISHING DETECTION ACTIVATION STATUS AS  " << perform_detection_);
    detectionActivationPub.publish(perform_detection_);
    ROS_INFO_STREAM("PUBLISHING PLANNING ACTIVATION STATUS AS  " << perform_planning_);
    planningActivationPub.publish(perform_planning_);
  }

  void startPub() {
    cmd_vel_pub_ = nh_.advertise<geometry_msgs::Twist>(cmd_vel_topic_, 10);
    detectionActivationPub = nh_.advertise<std_msgs::Bool>("docking/perform_detection", 10);
    planningActivationPub = nh_.advertise<std_msgs::Bool>("docking/perform_planning", 10);
  }


  void startPlanSub()
  {
    planSub_ = nh_.subscribe(plan_topic_, 1, &ControllerNode::planCallback, this);
    ROS_WARN_STREAM("ControllerNode: Subscribed to " << plan_topic_);
  }

  void startPoseSub()
  {
      ROS_WARN_STREAM("PoseControllerNode: Subscribing to " << dock_pose_topic_);
      dockPoseSub_ = nh_.subscribe(dock_pose_topic_, 1, &ControllerNode::dockPoseCallback, this);
      ROS_WARN_STREAM("PoseControllerNode: Subscribed to " << dock_pose_topic_);
  }

  void initGlobals(){
    ROS_INFO_STREAM("ControllerNode: Initializing Globals");
    ROS_INFO_STREAM("Time Step: " << time_step_);
    ros::Duration time_step_duration(time_step_);
    time_step_duration_ = time_step_duration;
    ROS_INFO_STREAM("Time Step Duration: " << time_step_duration);
//    ros::Rate frequencyRate(frequency_);
//    frequencyRate_ = frequencyRate;
    zeroTwist_.linear.x = zeroTwist_.angular.z = 0.0;
    within_goal_dist_tolerance_ = within_goal_orientation_tolerance_ = false;
    ROS_INFO_STREAM("ControllerNode: Initialized Globals");
  }

  void clearGlobals(){
    plan_.path.poses.clear();
    plan_.poseArray.poses.clear();
    plan_.twists.clear();
  }

  void dockPoseCallback(const geometry_msgs::PoseStamped::ConstPtr& msg){
    dock_pose_ = *msg;
  }

  void planCallback(const docking::Plan::ConstPtr& msg){
    plan_ = *msg;
    geometry_msgs::Twist currentTwist = plan_.twists.at(1); // First is zero twist at starting pose

    if(!willOvershoot(plan_)){
      ROS_INFO_STREAM("CUERRENT TWIST VALID");
      if(publish_twist_){
        ROS_INFO_STREAM("PUBLISHING CURRENT TWIST");
        cmd_vel_pub_.publish(currentTwist);
      }
    } else {
      ROS_WARN_STREAM("CURRENT TWIST WILL OVERSHOOT TARGET. NOT SENDING TWIST");
    }

  }


  bool willOvershoot(docking::Plan plan){
    geometry_msgs::Twist twist = plan.twists.at(1);
//    twist.linear.x = 20.0;
//    twist.angular.z = 20;
    geometry_msgs::PoseArray poses = plan.poseArray;


    ROS_INFO_STREAM("**************** CHECKING TOLERANCE ****************");
    tf2::Transform base2ProjectionTF, robotTF, base2TargetTF, base2EntranceTF, proj2TargetTF, deltaTF;
    geometry_msgs::TransformStamped base2ProjectionTFMsg, robotTFMsg, base2TargetTFMsg, base2EntranceTFMsg, proj2TargetTFMsg, deltaTFMsg, tempTFMsg;
    geometry_msgs::PoseStamped base2ProjectionPose, robotPose, base2TargetPose, base2EntrancePose, proj2TargetPose, deltaTFPose, target2EntrancePose;
    std::string projectionFrameID="projection", robotFrameID="base_link", targetFrameID="target", entranceFrameID="entrance";


    // SETUP BASE2PROJECTION
    base2ProjectionPose.pose = poses.poses.at(0);
    syncTFData(base2ProjectionPose,base2ProjectionTF,base2ProjectionTFMsg,projectionFrameID);

    deltaTFMsg = base2ProjectionTFMsg;
//    syncTFData(deltaTFMsg,deltaTF,deltaTFPose);
    ROS_INFO_STREAM("ORIGINAL TF  " << transformString(base2ProjectionTF));

    // STEP CURRENT POSE OVER 1 STEP
    deltaTF = getDeltaTF(base2ProjectionTF,twist,time_step_);
    syncTFData(deltaTF,deltaTFMsg,deltaTFPose,robotFrameID,projectionFrameID);
    ROS_INFO_STREAM("STEPPED TF  " << transformString(deltaTF));

    base2ProjectionTF = deltaTF;
    syncTFData(base2ProjectionTF,base2ProjectionTFMsg,base2ProjectionPose,robotFrameID,projectionFrameID);

    // GET TARGET POSE
    base2TargetPose.pose = poses.poses.back();
    syncTFData(base2TargetPose,base2TargetTF,base2TargetTFMsg,targetFrameID);
    ROS_INFO_STREAM("BASE 2 TARGET TF  " << transformString(base2TargetTF));

    double deltaXProjection2Target = base2TargetTF.getOrigin().x() - base2ProjectionTF.getOrigin().x();

    ROS_INFO_STREAM("DELTA X OF CURRENT TWIST PROJECTED & TARGET  " << deltaXProjection2Target);

    if(deltaXProjection2Target < 0){
      return true;
    } else {
      return false;
    }
//


    std::cout << "___________________________________________________________________________________________________" << std::endl;
  }








};

#endif // CONTROLLERNODE_H