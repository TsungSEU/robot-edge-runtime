// planner_utils.cpp
#include "planner_utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <sys/stat.h>
#include <cmath>
#include <limits>
#include <yaml-cpp/yaml.h>
#include <iostream>

// 性能优化工具
#include "common/performance_utils.h"

// Path implementation
void Path::addPoint(const Point& point) {
    waypoints.push_back(point);
    if (waypoints.size() > 1) {
        Point prev = waypoints[waypoints.size()-2];
        length += aurora::planner::PlannerUtils::euclideanDistance(prev, point);
    }
}

void Path::clear() {
    waypoints.clear();
    length = 0.0;
}

namespace aurora::planner {
// PlannerUtils implementation
double PlannerUtils::euclideanDistance(const Point& p1, const Point& p2) {
    double dx = p1.x - p2.x;
    double dy = p1.y - p2.y;
    return std::sqrt(dx * dx + dy * dy);
}

double PlannerUtils::manhattanDistance(const Point& p1, const Point& p2) {
    return std::abs(p1.x - p2.x) + std::abs(p1.y - p2.y);
}

double PlannerUtils::pointToSegmentDistance(const Point& point, 
                                           const Point& seg_start, 
                                           const Point& seg_end) {
    double l2 = euclideanDistance(seg_start, seg_end);
    l2 *= l2;
    
    if (l2 == 0.0) {
        return euclideanDistance(point, seg_start);
    }
    
    double t = std::max(0.0, std::min(1.0, 
        ((point.x - seg_start.x) * (seg_end.x - seg_start.x) + 
         (point.y - seg_start.y) * (seg_end.y - seg_start.y)) / l2));
         
    Point projection(seg_start.x + t * (seg_end.x - seg_start.x),
                     seg_start.y + t * (seg_end.y - seg_start.y));
                     
    return euclideanDistance(point, projection);
}

bool PlannerUtils::pointsAlmostEqual(const Point& p1, const Point& p2, double epsilon) {
    return (std::abs(p1.x - p2.x) < epsilon) && (std::abs(p1.y - p2.y) < epsilon);
}

std::pair<int, int> PlannerUtils::worldToGrid(const Point& world_point, double resolution) {
    int grid_x = static_cast<int>(std::floor(world_point.x / resolution));
    int grid_y = static_cast<int>(std::floor(world_point.y / resolution));
    return std::make_pair(grid_x, grid_y);
}

Point PlannerUtils::gridToWorld(int grid_x, int grid_y, double resolution) {
    return Point((static_cast<double>(grid_x) + 0.5) * resolution,
                 (static_cast<double>(grid_y) + 0.5) * resolution);
}

template<typename T>
T PlannerUtils::clamp(T value, T min_val, T max_val) {
    return std::max(min_val, std::min(value, max_val));
}

double PlannerUtils::normalizeAngle(double angle) {
    while (angle > M_PI) angle -= 2*M_PI;
    while (angle < -M_PI) angle += 2*M_PI;
    return angle;
}

double PlannerUtils::calculatePathLength(const std::vector<Point>& path) {
    if (path.size() < 2) {
        return 0.0;
    }
    
    double length = 0.0;
    for (size_t i = 1; i < path.size(); i++) {
        length += euclideanDistance(path[i-1], path[i]);
    }
    return length;
}

Path PlannerUtils::resamplePath(const std::vector<Point>& path, double spacing) {
    Path resampled_path;
    
    if (path.empty()) {
        return resampled_path;
    }
    
    resampled_path.addPoint(path[0]);
    
    if (path.size() < 2) {
        return resampled_path;
    }
    
    double current_distance = 0.0;
    
    for (size_t i = 1; i < path.size(); i++) {
        double segment_length = euclideanDistance(path[i-1], path[i]);
        current_distance += segment_length;
        
        while (current_distance >= spacing) {
            double ratio = 1.0 - (current_distance - spacing) / segment_length;
            Point new_point(path[i-1].x + ratio * (path[i].x - path[i-1].x),
                           path[i-1].y + ratio * (path[i].y - path[i-1].y));
            resampled_path.addPoint(new_point);
            current_distance -= spacing;
        }
    }
    
    // Add last point if not already added
    if (!pointsAlmostEqual(resampled_path.waypoints.back(), path.back())) {
        resampled_path.addPoint(path.back());
    }
    
    return resampled_path;
}

Path PlannerUtils::smoothPath(const Path& path, int smoothing_iterations) {
    if (path.waypoints.size() < 3) {
        return path;
    }
    
    Path smoothed = path;
    
    for (int iter = 0; iter < smoothing_iterations; iter++) {
        std::vector<Point> new_waypoints = smoothed.waypoints;
        
        // Smooth intermediate points
        for (size_t i = 1; i < smoothed.waypoints.size() - 1; i++) {
            new_waypoints[i].x = (smoothed.waypoints[i-1].x + 
                                 smoothed.waypoints[i].x * 2.0 + 
                                 smoothed.waypoints[i+1].x) / 4.0;
                                 
            new_waypoints[i].y = (smoothed.waypoints[i-1].y + 
                                 smoothed.waypoints[i].y * 2.0 + 
                                 smoothed.waypoints[i+1].y) / 4.0;
        }
        
        smoothed.waypoints = new_waypoints;
    }
    
    // Recalculate path length
    smoothed.length = calculatePathLength(smoothed.waypoints);
    
    return smoothed;
}

bool PlannerUtils::isPathValid(const std::vector<Point>& path, const CostMap& costmap, 
                              double collision_threshold) {
    for (const auto& point : path) {
        std::pair<int, int> grid_coords = worldToGrid(point, costmap.getResolution());
        int grid_x = grid_coords.first;
        int grid_y = grid_coords.second;
        
        if (costmap.isValidCell(grid_x, grid_y)) {
            double cost = costmap.getCellCost(grid_x, grid_y);
            if (cost > collision_threshold) {
                return false; // Collision detected
            }
        } else {
            return false; // Point outside valid map area
        }
    }
    return true;
}

bool PlannerUtils::loadParametersFromYaml(const std::string& filepath,
                                         std::map<std::string, double>& parameters) {
    try {
        YAML::Node config = YAML::LoadFile(filepath);

        // Helper lambda to safely convert scalar to double
        auto safeConvertToDouble = [](const YAML::Node& node) -> bool {
            try {
                node.as<double>();
                return true;
            } catch (const YAML::BadConversion&) {
                return false;  // Not a numeric value, skip
            }
        };

        // Load top-level parameters
        for (const auto& item : config) {
            if (item.second.IsScalar()) {
                std::string key = item.first.as<std::string>();
                if (safeConvertToDouble(item.second)) {
                    double value = item.second.as<double>();
                    parameters[key] = value;
                }
            }
        }

        // Load nested parameters with flattened keys
        auto loadNestedSection = [&](const char* section_name, const char* prefix) {
            if (config[section_name]) {
                const YAML::Node& section = config[section_name];
                for (const auto& item : section) {
                    if (item.second.IsScalar()) {
                        std::string key = std::string(prefix) + item.first.as<std::string>();
                        if (safeConvertToDouble(item.second)) {
                            double value = item.second.as<double>();
                            parameters[key] = value;
                        }
                    }
                }
            }
        };

        // Load from new structured format
        loadNestedSection("common", "");
        loadNestedSection("humanoid", "humanoid_");
        loadNestedSection("auto", "auto_");
        loadNestedSection("data_value", "data_value_");

        return true;
    } catch (const YAML::Exception& e) {
        LogUtils::log(LogUtils::ERROR, "Failed to load parameters from " + filepath + ": " + e.what());
        return false;
    }
}

bool PlannerUtils::saveParametersToYaml(const std::string& filepath, 
                                       const std::map<std::string, double>& parameters) {
    try {
        YAML::Emitter out;
        out << YAML::BeginMap;
        for (const auto& param : parameters) {
            out << YAML::Key << param.first;
            out << YAML::Value << param.second;
        }
        out << YAML::EndMap;
        
        std::ofstream fout(filepath);
        fout << out.c_str();
        fout.close();
        
        return true;
    } catch (const std::exception& e) {
        LogUtils::log(LogUtils::ERROR, "Failed to save parameters to " + filepath + ": " + e.what());
        return false;
    }
}

Pose PlannerUtils::interpolatePose(const Pose& pose1, const Pose& pose2, double ratio) {
    ratio = clamp(ratio, 0.0, 1.0);
    
    Point interpolated_pos(pose1.position.x + ratio * (pose2.position.x - pose1.position.x),
                          pose1.position.y + ratio * (pose2.position.y - pose1.position.y));
                          
    double interpolated_yaw = normalizeAngle(pose1.yaw + ratio * 
                                           normalizeAngle(pose2.yaw - pose1.yaw));
                                           
    return Pose(interpolated_pos, interpolated_yaw);
}

double PlannerUtils::calculateHeading(const Point& from, const Point& to) {
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    return std::atan2(dy, dx);
}

bool PlannerUtils::loadModeSpecificParameters(const std::string& filepath,
                                              const std::string& mode,
                                              std::map<std::string, double>& parameters) {
    try {
        YAML::Node config = YAML::LoadFile(filepath);

        // Helper lambda to safely convert scalar to double
        auto safeConvertToDouble = [](const YAML::Node& node) -> bool {
            try {
                node.as<double>();
                return true;
            } catch (const YAML::BadConversion&) {
                return false;  // Not a numeric value, skip
            }
        };

        // Helper lambda to load a section with a prefix
        auto loadSection = [&](const YAML::Node& root, const std::string& section_name, const std::string& prefix) {
            if (root[section_name]) {
                const YAML::Node& section = root[section_name];
                for (const auto& item : section) {
                    if (item.second.IsScalar() && safeConvertToDouble(item.second)) {
                        std::string key = prefix + item.first.as<std::string>();
                        parameters[key] = item.second.as<double>();
                    } else if (item.second.IsMap()) {
                        // Handle nested maps (e.g., humanoid.ppo.*)
                        std::string nested_prefix = prefix + item.first.as<std::string>() + "_";
                        for (const auto& sub_item : item.second) {
                            if (sub_item.second.IsScalar() && safeConvertToDouble(sub_item.second)) {
                                std::string key = nested_prefix + sub_item.first.as<std::string>();
                                parameters[key] = sub_item.second.as<double>();
                            }
                        }
                    }
                }
            }
        };

        // 1. Load common parameters (no prefix)
        loadSection(config, "common", "");

        // 2. Load top-level parameters (for backward compatibility)
        for (const auto& item : config) {
            if (item.second.IsScalar() && safeConvertToDouble(item.second)) {
                std::string key = item.first.as<std::string>();
                parameters[key] = item.second.as<double>();
            }
        }

        // 3. Load mode-specific parameters based on mode
        if (mode == "humanoid") {
            // Load humanoid.* sections
            loadSection(config, "humanoid", "humanoid_");
        } else if (mode == "auto") {
            // Load auto.* sections
            loadSection(config, "auto", "auto_");
        }

        // 4. Load data_value section (use common.* or data_value.*)
        loadSection(config, "data_value", "data_value_");

        return true;
    } catch (const YAML::Exception& e) {
        LogUtils::log(LogUtils::ERROR, "Failed to load parameters from " + filepath + ": " + e.what());
        return false;
    }
}

// FileUtils implementation
bool FileUtils::fileExists(const std::string& filepath) {
    struct stat buffer;
    return (stat(filepath.c_str(), &buffer) == 0);
}

bool FileUtils::createDirectory(const std::string& dirpath) {
    // Simple implementation - in production, you might want a more robust solution
    return (mkdir(dirpath.c_str(), 0777) == 0 || errno == EEXIST);
}

std::string FileUtils::readFile(const std::string& filepath) {
    std::ifstream file(filepath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool FileUtils::writeFile(const std::string& filepath, const std::string& content) {
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << content;
        file.close();
        return true;
    }
    return false;
}

// LogUtils implementation
static LogUtils::LogLevel g_min_log_level = LogUtils::INFO;

void LogUtils::log(LogLevel level, const std::string& message) {
    if (level < g_min_log_level) {
        return;
    }
    
    std::string prefix;
    switch (level) {
        case DEBUG: prefix = "[DEBUG] "; break;
        case INFO:  prefix = "[INFO] ";  break;
        case WARN:  prefix = "[WARN] ";  break;
        case ERROR: prefix = "[ERROR] "; break;
    }
    
    std::cout << prefix << message << std::endl;
}

void LogUtils::setLogLevel(LogLevel level) {
    g_min_log_level = level;
}

std::string LogUtils::formatPoint(const Point& point) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << "(" << point.x << ", " << point.y << ")";
    return ss.str();
}

} // namespace aurora::planner