// path_visualizer.cpp - Apollo style visualization
#include "path_visualizer.h"
#include "common/log/logger.h"
#include "common/ros2/qos_profiles.h"
#include <cmath>

namespace aurora::planner {

PathVisualizer::PathVisualizer(rclcpp::Node::SharedPtr node, double trail_publish_interval_sec)
    : node_(node)
    , enabled_(true)
    , debug_mode_(false)
    , marker_id_(0)
    , trail_publish_interval_sec_(trail_publish_interval_sec) {

    // ========== 创建 Publisher ==========
    planning_path_vis_pub_ = node_->create_publisher<visualization_msgs::msg::Marker>(
        "/planning_path_vis", aurora::common::qos::visualization());

    planning_traj_vis_pub_ = node_->create_publisher<visualization_msgs::msg::Marker>(
        "/planning_traj_vis", aurora::common::qos::visualization());

    start_goal_vis_pub_ = node_->create_publisher<visualization_msgs::msg::Marker>(
        "/start_goal_markers", aurora::common::qos::visualization());

    collected_path_vis_pub_ = node_->create_publisher<visualization_msgs::msg::Marker>(
        "/collected_path_vis", aurora::common::qos::visualization());

    robot_trail_pub_ = node_->create_publisher<nav_msgs::msg::Path>(
        "/robot/trail", aurora::common::qos::visualization());

    collection_points_pub_ = node_->create_publisher<visualization_msgs::msg::Marker>(
        "/collection_points_vis", aurora::common::qos::visualization());

    // Initialize trail message
    trail_msg_.header.frame_id = frame_id_;
    last_trail_publish_time_ = node_->now();

    AD_INFO(PathVisualizer, "PathVisualizer initialized");
    AD_INFO(PathVisualizer, " Topics:");
    AD_INFO(PathVisualizer, "    /planning_path_vis - green path line");
    AD_INFO(PathVisualizer, "    /planning_traj_vis - red trajectory points");
    AD_INFO(PathVisualizer, "    /start_goal_markers - start/goal markers");
    AD_INFO(PathVisualizer, "    /collected_path_vis - blue collected path");
    AD_INFO(PathVisualizer, "    /robot/trail - real-time robot trail (nav_msgs/Path)");
    AD_INFO(PathVisualizer, "    /collection_points_vis - cyan collection point markers");
}

PathVisualizer::~PathVisualizer() {
    clearMarkers();
}

void PathVisualizer::setFrameId(const std::string& frame_id) {
    frame_id_ = frame_id;
}

rclcpp::Time PathVisualizer::getCurrentTime() const {
    return node_->now();
}

visualization_msgs::msg::Marker PathVisualizer::createLineStrip(
        const std::string& ns, int id,
        float r, float g, float b,
        const std::vector<Point>& points,
        float width) {

    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = frame_id_;
    marker.header.stamp = getCurrentTime();
    marker.ns = ns;
    marker.id = id;
    marker.type = visualization_msgs::msg::Marker::LINE_STRIP;
    marker.action = visualization_msgs::msg::Marker::ADD;

    // 设置颜色
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;
    marker.color.a = 1.0f;

    // 设置线宽
    marker.scale.x = width;
    marker.scale.y = 0.0f;
    marker.scale.z = 0.0f;

    // 设置方向 (单位四元数)
    marker.pose.orientation.x = 0.0f;
    marker.pose.orientation.y = 0.0f;
    marker.pose.orientation.z = 0.0f;
    marker.pose.orientation.w = 1.0f;

    // 转换点
    for (const auto& pt : points) {
        geometry_msgs::msg::Point p;
        p.x = static_cast<double>(pt.x);
        p.y = static_cast<double>(pt.y);
        p.z = 0.0;
        marker.points.push_back(p);
    }

    return marker;
}

visualization_msgs::msg::Marker PathVisualizer::createSphere(
        const std::string& ns, int id,
        const Point& position,
        float r, float g, float b,
        float scale) {

    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = frame_id_;
    marker.header.stamp = getCurrentTime();
    marker.ns = ns;
    marker.id = id;
    marker.type = visualization_msgs::msg::Marker::SPHERE;
    marker.action = visualization_msgs::msg::Marker::ADD;

    // 设置颜色
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;
    marker.color.a = 1.0f;

    // 设置大小
    marker.scale.x = scale;
    marker.scale.y = scale;
    marker.scale.z = scale;

    // 设置位置
    marker.pose.position.x = static_cast<double>(position.x);
    marker.pose.position.y = static_cast<double>(position.y);
    marker.pose.position.z = 0.0;

    // 设置方向
    marker.pose.orientation.x = 0.0f;
    marker.pose.orientation.y = 0.0f;
    marker.pose.orientation.z = 0.0f;
    marker.pose.orientation.w = 1.0f;

    return marker;
}

void PathVisualizer::publishMarker(const visualization_msgs::msg::Marker& marker) {
    // 设置 lifetime
    visualization_msgs::msg::Marker m = marker;
    m.lifetime = rclcpp::Duration::from_seconds(MARKER_LIFETIME_LONG_SEC);

    // 发布到对应的 topic
    if (m.ns == "local_planning_path") {
        planning_path_vis_pub_->publish(m);
    } else if (m.ns == "local_planning_trajectory") {
        planning_traj_vis_pub_->publish(m);
    } else if (m.ns == "start_goal_marker") {
        start_goal_vis_pub_->publish(m);
    } else if (m.ns == "collected_path") {
        collected_path_vis_pub_->publish(m);
    }
}

void PathVisualizer::publishPlanningPath(const std::vector<Point>& path) {
    if (!enabled_ || path.empty()) {
        return;
    }

    if (debug_mode_) {
        AD_DEBUG(PathVisualizer, "Publishing planning path with %zu points", path.size());
    }

    // 绿色 LINE_STRIP (RGB: 0.0, 0.8, 0.0)
    auto marker = createLineStrip("local_planning_path", marker_id_++,
                                      0.0f, 0.8f, 0.0f, path, 0.15f);
    publishMarker(marker);

    if (debug_mode_) {
        AD_INFO(PathVisualizer, "Published planning path to /planning_path_vis");
    }
}

void PathVisualizer::publishTrajectory(const std::vector<Point>& traj) {
    if (!enabled_ || traj.empty()) {
        return;
    }

    if (debug_mode_) {
        AD_DEBUG(PathVisualizer, "Publishing trajectory with %zu points", traj.size());
    }

    // 红色 SPHERE 用于轨迹点
    size_t step = std::max(size_t(1), traj.size() / 10);

    for (size_t i = 0; i < traj.size(); i += step) {
        auto marker = createSphere("local_planning_trajectory", static_cast<int>(i),
                                       traj[i], 1.0f, 0.0f, 0.0f, 0.15f);
        publishMarker(marker);
    }

    if (debug_mode_) {
        AD_INFO(PathVisualizer, "Published %zu trajectory points to /planning_traj_vis", traj.size() / step);
    }
}

void PathVisualizer::publishStartGoalMarkers(const Point& start, const Point& goal) {
    if (!enabled_) {
        return;
    }

    if (debug_mode_) {
        AD_DEBUG(PathVisualizer, "Publishing start (%.2f, %.2f) -> goal (%.2f, %.2f)",
                 start.x, start.y, goal.x, goal.y);
    }

    // 使用 SPHERE 标记起止点
    // 起点：绿色 SPHERE
    auto start_marker = createSphere("start_goal_marker", 0, start, 0.0f, 1.0f, 0.4f);
    publishMarker(start_marker);

    // 终点：红色 SPHERE
    auto goal_marker = createSphere("start_goal_marker", 1, goal, 1.0f, 0.0f, 0.4f);
    publishMarker(goal_marker);

    if (debug_mode_) {
        AD_INFO(PathVisualizer, "Published start/goal markers to /start_goal_markers");
    }
}

void PathVisualizer::clearMarkers() {
    if (debug_mode_) {
        AD_DEBUG(PathVisualizer, "Clearing all markers");
    }

    // 创建 DELETEALL marker
    visualization_msgs::msg::Marker delete_marker;
    delete_marker.header.frame_id = frame_id_;
    delete_marker.header.stamp = getCurrentTime();
    delete_marker.ns = "";  // 匹配所有 namespace
    delete_marker.action = visualization_msgs::msg::Marker::DELETEALL;

    // 发布到所有 publisher
    planning_path_vis_pub_->publish(delete_marker);
    planning_traj_vis_pub_->publish(delete_marker);
    start_goal_vis_pub_->publish(delete_marker);
    collected_path_vis_pub_->publish(delete_marker);

    // 重置 ID 计数器
    marker_id_ = 0;
}

void PathVisualizer::publishCollectedPath(const std::vector<Point>& path) {
    if (!enabled_ || path.empty()) {
        return;
    }

    if (debug_mode_) {
        AD_DEBUG(PathVisualizer, "Publishing collected path with %zu points", path.size());
    }

    // 蓝色 LINE_STRIP (RGB: 0.0, 0.5, 1.0) 用于实际采集路径
    // 使用较粗的线宽以区别于规划路径
    auto marker = createLineStrip("collected_path", marker_id_++,
                                  0.0f, 0.5f, 1.0f, path, 0.3f);
    publishMarker(marker);

    if (debug_mode_) {
        AD_INFO(PathVisualizer, "Published collected path to /collected_path_vis");
    }
}

void PathVisualizer::updateCollectedPath(const std::vector<Point>& new_points) {
    if (!enabled_ || new_points.empty()) {
        return;
    }

    // 将新点添加到历史路径
    for (const auto& point : new_points) {
        // 避免重复添加相同点
        if (collected_path_history_.empty()) {
            collected_path_history_.push_back(point);
        } else {
            const auto& last_point = collected_path_history_.back();
            double dist = std::sqrt(std::pow(point.x - last_point.x, 2) +
                                   std::pow(point.y - last_point.y, 2));
            // 只有距离超过阈值才添加（避免过于密集）
            if (dist > 0.1) {
                collected_path_history_.push_back(point);
            }
        }
    }

    // 发布完整的历史路径
    if (!collected_path_history_.empty()) {
        publishCollectedPath(collected_path_history_);
    }

    if (debug_mode_) {
        AD_DEBUG(PathVisualizer, "Updated collected path history: total %zu points",
                 collected_path_history_.size());
    }
}

void PathVisualizer::clearCollectedPathHistory() {
    if (debug_mode_) {
        AD_DEBUG(PathVisualizer, "Clearing collected path history (%zu points)",
                 collected_path_history_.size());
    }

    collected_path_history_.clear();

    // 删除可视化标记
    visualization_msgs::msg::Marker delete_marker;
    delete_marker.header.frame_id = frame_id_;
    delete_marker.header.stamp = getCurrentTime();
    delete_marker.ns = "collected_path";
    delete_marker.action = visualization_msgs::msg::Marker::DELETEALL;
    collected_path_vis_pub_->publish(delete_marker);
}

void PathVisualizer::updateRobotTrail(const Point& pos) {
    if (!enabled_) {
        return;
    }

    // 避免过密的点（最小间距 5cm）
    if (!trail_msg_.poses.empty()) {
        const auto& last_pose = trail_msg_.poses.back().pose.position;
        double dx = pos.x - last_pose.x;
        double dy = pos.y - last_pose.y;
        if (dx * dx + dy * dy < 0.05 * 0.05) {
            return;
        }
    }

    // 追加点到 trail
    geometry_msgs::msg::PoseStamped pose_stamped;
    pose_stamped.header.frame_id = frame_id_;
    pose_stamped.header.stamp = getCurrentTime();
    pose_stamped.pose.position.x = pos.x;
    pose_stamped.pose.position.y = pos.y;
    pose_stamped.pose.position.z = 0.0;
    pose_stamped.pose.orientation.w = 1.0;
    trail_msg_.poses.push_back(pose_stamped);

    // 限制最大点数（避免消息过大）
    if (trail_msg_.poses.size() > 2000) {
        trail_msg_.poses.erase(trail_msg_.poses.begin(),
                               trail_msg_.poses.begin() + 500);
    }

    // 按频率发布
    auto now = getCurrentTime();
    double elapsed = (now - last_trail_publish_time_).seconds();
    if (elapsed >= trail_publish_interval_sec_) {
        trail_msg_.header.frame_id = frame_id_;
        trail_msg_.header.stamp = now;
        robot_trail_pub_->publish(trail_msg_);
        last_trail_publish_time_ = now;
    }
}

void PathVisualizer::publishCollectionPoints(const std::vector<Point>& points) {
    if (!enabled_ || points.empty()) {
        return;
    }

    // 先清除旧的采集点标记
    visualization_msgs::msg::Marker delete_marker;
    delete_marker.header.frame_id = frame_id_;
    delete_marker.header.stamp = getCurrentTime();
    delete_marker.ns = "collection_point";
    delete_marker.action = visualization_msgs::msg::Marker::DELETEALL;
    collection_points_pub_->publish(delete_marker);

    // 发布青色球体标记每个采集点
    for (size_t i = 0; i < points.size(); ++i) {
        auto marker = createSphere("collection_point", static_cast<int>(i),
                                   points[i], 0.0f, 1.0f, 1.0f, 0.2f);
        marker.lifetime = rclcpp::Duration::from_seconds(0.0);  // 永久
        collection_points_pub_->publish(marker);
    }
}

void PathVisualizer::clearTrailAndCollectionPoints() {
    // 清除 trail
    trail_msg_.poses.clear();

    // 清除采集点标记
    visualization_msgs::msg::Marker delete_marker;
    delete_marker.header.frame_id = frame_id_;
    delete_marker.header.stamp = getCurrentTime();
    delete_marker.ns = "collection_point";
    delete_marker.action = visualization_msgs::msg::Marker::DELETEALL;
    collection_points_pub_->publish(delete_marker);

    // 发布空 trail
    trail_msg_.header.frame_id = frame_id_;
    trail_msg_.header.stamp = getCurrentTime();
    robot_trail_pub_->publish(trail_msg_);
}

} // namespace aurora::planner
