// path_visualizer.h
#ifndef PATH_VISUALIZER_H
#define PATH_VISUALIZER_H

#include <memory>
#include <string>
#include <vector>
#include <rclcpp/rclcpp.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <nav_msgs/msg/path.hpp>
#include "../maps/costmap.h"

namespace aurora::planner {

/**
 * @brief Path visualization publisher for rviz2
 *
 * - planning_path_vis: 规划路径 (绿色 LINE_STRIP)
 * - planning_traj_vis: 轨迹点 (红色 SPHERE_LIST)
 * - start_goal_markers: 起止点标记
 * - collected_path_vis: 实际采集路径 (蓝色 LINE_STRIP)
 */
class PathVisualizer {
public:
    explicit PathVisualizer(rclcpp::Node::SharedPtr node, double trail_publish_interval_sec = 0.2);
    ~PathVisualizer();

    /**
     * @brief 发布规划路径 - 绿色 LINE_STRIP
     * Apollo: planning_path_vis_pub_
     */
    void publishPlanningPath(const std::vector<Point>& path);

    /**
     * @brief 发布轨迹点 - 红色球体列表
     * Apollo: planning_traj_vis_pub_
     */
    void publishTrajectory(const std::vector<Point>& traj);

    /**
     * @brief 发布起止点标记
     */
    void publishStartGoalMarkers(const Point& start, const Point& goal);

    /**
     * @brief 发布实际采集路径 - 蓝色 LINE_STRIP
     * 显示机器人实际走过的路径，与规划路径进行对比
     * @param path 实际采集的路径点
     */
    void publishCollectedPath(const std::vector<Point>& path);

    /**
     * @brief 更新实际采集路径（增量添加）
     * 添加新的路径点到历史路径中，用于实时显示机器人轨迹
     * @param new_points 新采集的路径点
     */
    void updateCollectedPath(const std::vector<Point>& new_points);

    /**
     * @brief 清除实际采集路径历史
     */
    void clearCollectedPathHistory();

    /**
     * @brief 实时更新机器人轨迹（增量添加单点）
     * 使用 nav_msgs/Path 发布平滑实时轨迹
     * @param pos 当前机器人位置
     */
    void updateRobotTrail(const Point& pos);

    /**
     * @brief 发布采集点标记（青色球体）
     * @param points 实际触发采集的位置列表
     */
    void publishCollectionPoints(const std::vector<Point>& points);

    /**
     * @brief 清除机器人轨迹和采集点标记
     */
    void clearTrailAndCollectionPoints();

    /**
     * @brief 清除所有可视化标记
     */
    void clearMarkers();

    /**
     * @brief 设置坐标系 ID
     */
    void setFrameId(const std::string& frame_id);

    /**
     * @brief 设置可视化启用状态
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    /**
     * @brief 设置调试模式
     */
    void setDebugMode(bool debug) { debug_mode_ = debug; }

    /**
     * @brief 设置轨迹发布间隔
     * @param interval_sec 发布间隔 (秒)
     */
    void setTrailPublishInterval(double interval_sec) { trail_publish_interval_sec_ = interval_sec; }

private:
    // ========== Publisher ==========
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr planning_path_vis_pub_;
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr planning_traj_vis_pub_;
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr start_goal_vis_pub_;
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr collected_path_vis_pub_;
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr robot_trail_pub_;
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr collection_points_pub_;

    /**
     * @brief 创建 LINE_STRIP marker (用于路径)
     * @param ns Marker namespace
     * @param id Marker ID
     * @param r,g,b RGB color [0-1]
     * @param points 路径点
     * @param width 线宽
     */
    visualization_msgs::msg::Marker createLineStrip(
        const std::string& ns, int id,
        float r, float g, float b,
        const std::vector<Point>& points,
        float width = 0.2f);

    /**
     * @brief 创建 SPHERE marker (用于关键点)
     * @param ns Marker namespace
     * @param id Marker ID
     * @param position 位置
     * @param r RGB color red [0-1]
     * @param g RGB color green [0-1]
     * @param b RGB color blue [0-1]
     * @param scale 球体大小
     */
    visualization_msgs::msg::Marker createSphere(
        const std::string& ns, int id,
        const Point& position,
        float r, float g, float b,
        float scale = 0.3f);

    /**
     * @brief 创建 CUBE marker (用于起止点)
     */
    visualization_msgs::msg::Marker createCube(
        const std::string& ns, int id,
        float r, float g, float b,
        const Point& position,
        float scale = 0.5f);

    /**
     * @brief 发布单个 marker 并添加通用 header
     */
    void publishMarker(const visualization_msgs::msg::Marker& marker);

    /**
     * @brief 获取当前时间戳
     */
    rclcpp::Time getCurrentTime() const;

    // ========== 成员变量 ==========
    rclcpp::Node::SharedPtr node_;
    std::string frame_id_ = "odom";
    bool enabled_ = true;
    bool debug_mode_ = false;

    // Marker ID counter (用于避免 ID 冲突)
    int marker_id_ = 0;

    // 实际采集路径历史 (累积显示机器人实际走过的路径)
    std::vector<Point> collected_path_history_;

    // 实时机器人轨迹 (nav_msgs/Path)
    nav_msgs::msg::Path trail_msg_;
    rclcpp::Time last_trail_publish_time_;
    double trail_publish_interval_sec_;  // 轨迹发布间隔 (秒)，默认 0.2 (5Hz)

    // Marker lifetime 设置
    static constexpr double MARKER_LIFETIME_SHORT_SEC = 0.0;   // 0 = 永久显示
    static constexpr double MARKER_LIFETIME_LONG_SEC = 0.0;    // 0 = 永久显示
};

} // namespace aurora::planner

#endif // PATH_VISUALIZER_H
