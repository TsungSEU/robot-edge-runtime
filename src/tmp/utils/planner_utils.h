// planner_utils.h
#ifndef PLANNER_UTILS_H
#define PLANNER_UTILS_H

#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <iostream>
#include "../maps/costmap.h"
#include "../safety/semantic_map.h"

struct Pose {
    Point position;
    double yaw; // Orientation in radians
    
    Pose(const Point& pos = Point(), double orientation = 0.0)
        : position(pos), yaw(orientation) {}
};

struct Path {
    std::vector<Point> waypoints;
    double length;
    
    Path() : length(0.0) {}
    
    void addPoint(const Point& point);
    void clear();
    bool empty() const { return waypoints.empty(); }
    size_t size() const { return waypoints.size(); }
};

namespace aurora::planner {
class PlannerUtils {
public:
    /**
     * @brief Calculate Euclidean distance between two points
     */
    static double euclideanDistance(const Point& p1, const Point& p2);
    
    /**
     * @brief Calculate Manhattan distance between two points
     */
    static double manhattanDistance(const Point& p1, const Point& p2);
    
    /**
     * @brief Calculate distance from a point to a line segment
     */
    static double pointToSegmentDistance(const Point& point, 
                                        const Point& seg_start, 
                                        const Point& seg_end);
    
    /**
     * @brief Check if two points are approximately equal within epsilon
     */
    static bool pointsAlmostEqual(const Point& p1, const Point& p2, double epsilon = 1e-6);
    
    /**
     * @brief Convert world coordinates to grid coordinates
     */
    static std::pair<int, int> worldToGrid(const Point& world_point, double resolution);
    
    /**
     * @brief Convert grid coordinates to world coordinates
     */
    static Point gridToWorld(int grid_x, int grid_y, double resolution);
    
    /**
     * @brief Clamp a value between min and max
     */
    template<typename T>
    static T clamp(T value, T min_val, T max_val);
    
    /**
     * @brief Normalize angle to [-π, π]
     */
    static double normalizeAngle(double angle);
    
    /**
     * @brief Calculate path length
     */
    static double calculatePathLength(const std::vector<Point>& path);
    
    /**
     * @brief Resample path to have evenly spaced waypoints
     */
    static Path resamplePath(const std::vector<Point>& path, double spacing);
    
    /**
     * @brief Smooth path using simple averaging
     */
    static Path smoothPath(const Path& path, int smoothing_iterations = 5);
    
    /**
     * @brief Check if a path collides with obstacles in the costmap
     */
    static bool isPathValid(const std::vector<Point>& path, const CostMap& costmap, 
                           double collision_threshold = 100.0);
    
    /**
     * @brief Load parameters from YAML file
     * @deprecated Use loadModeSpecificParameters instead for better structure
     */
    static bool loadParametersFromYaml(const std::string& filepath,
                                      std::map<std::string, double>& parameters);

    /**
     * @brief Load mode-specific parameters from YAML file
     * @param filepath Path to YAML config file
     * @param mode Planner mode ("auto" or "humanoid")
     * @param parameters Output map of flattened parameters
     *
     * This function loads parameters based on the mode:
     * - For "humanoid": loads from humanoid.* sections
     * - For "auto": loads from auto.* sections
     * - Also loads common.* and top-level parameters
     *
     * Parameter keys are flattened with underscores, e.g.:
     *   - humanoid.ppo.learning_rate -> humanoid_ppo_learning_rate
     *   - common.sparse_threshold -> sparse_threshold
     */
    static bool loadModeSpecificParameters(const std::string& filepath,
                                          const std::string& mode,
                                          std::map<std::string, double>& parameters);

    /**
     * @brief Save parameters to YAML file
     */
    static bool saveParametersToYaml(const std::string& filepath,
                                    const std::map<std::string, double>& parameters);
    
    /**
     * @brief Interpolate between two poses
     */
    static Pose interpolatePose(const Pose& pose1, const Pose& pose2, double ratio);
    
    /**
     * @brief Calculate heading from two points
     */
    static double calculateHeading(const Point& from, const Point& to);
};

// Utility functions for file operations
namespace FileUtils {
    /**
     * @brief Check if file exists
     */
    bool fileExists(const std::string& filepath);
    
    /**
     * @brief Create directory if it doesn't exist
     */
    bool createDirectory(const std::string& dirpath);
    
    /**
     * @brief Read text file content
     */
    std::string readFile(const std::string& filepath);
    
    /**
     * @brief Write content to text file
     */
    bool writeFile(const std::string& filepath, const std::string& content);
}

// Utility functions for logging and debugging
namespace LogUtils {
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3
    };
    
    /**
     * @brief Log message with level
     */
    void log(LogLevel level, const std::string& message);
    
    /**
     * @brief Set minimum log level
     */
    void setLogLevel(LogLevel level);
    
    /**
     * @brief Format point for logging
     */
    std::string formatPoint(const Point& point);
}

} // namespace aurora::planner
#endif // PLANNER_UTILS_H