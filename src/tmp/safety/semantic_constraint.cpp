// semantic_constraint.cpp
#include "semantic_constraint.h"
#include <cmath>

namespace aurora::planner {
std::vector<ConstraintViolation> SemanticConstraintChecker::checkPathConstraints(
    const std::vector<Point>& path) const {
    std::vector<ConstraintViolation> violations;
    
    for (const auto& point : path) {
        auto point_violations = checkPointConstraints(point);
        violations.insert(violations.end(), point_violations.begin(), point_violations.end());
    }
    
    return violations;
}

std::vector<ConstraintViolation> SemanticConstraintChecker::checkPointConstraints(
    const Point& point) const {
    std::vector<ConstraintViolation> violations;
    
    // Check traffic rules
    auto traffic_violations = checkTrafficRules(point);
    violations.insert(violations.end(), traffic_violations.begin(), traffic_violations.end());
    
    // Check data collection constraints
    auto data_violations = checkDataCollectionConstraints(point);
    violations.insert(violations.end(), data_violations.begin(), data_violations.end());
    
    return violations;
}

std::vector<ConstraintViolation> SemanticConstraintChecker::checkTrafficRules(
    const Point& point) const {
    std::vector<ConstraintViolation> violations;
    
    // Get nearby traffic-related objects
    auto traffic_lights = semantic_map.getObjectsInRadius(point, 5.0);
    
    for (const auto& obj : traffic_lights) {
        if (obj.type == TRAFFIC_LIGHT || obj.type == TRAFFIC_SIGN) {
            // Simple distance check - in reality this would be more complex
            double dx = obj.position.x - point.x;
            double dy = obj.position.y - point.y;
            double distance = std::sqrt(dx*dx + dy*dy);
            
            if (distance < obj.radius) {
                ConstraintViolation violation;
                violation.object = obj;
                violation.violation_point = point;
                violation.description = "Violated traffic rule: " + obj.label;
                violation.severity = 1.0 - (distance / obj.radius); // Closer = more severe
                violations.push_back(violation);
            }
        }
    }
    
    return violations;
}

std::vector<ConstraintViolation> SemanticConstraintChecker::checkDataCollectionConstraints(
    const Point& point) const {
    std::vector<ConstraintViolation> violations;
    
    // Check if we're in a valid data collection zone
    auto data_zones = semantic_map.getObjectsByType(DATA_COLLECTION_ZONE);
    
    bool in_valid_zone = false;
    for (const auto& zone : data_zones) {
        double dx = zone.position.x - point.x;
        double dy = zone.position.y - point.y;
        double distance = std::sqrt(dx*dx + dy*dy);
        
        if (distance <= zone.radius) {
            in_valid_zone = true;
            break;
        }
    }
    
    // If not in a valid data collection zone, this might be a constraint violation
    // depending on the mission objectives
    if (!in_valid_zone) {
        // This is a soft constraint - not necessarily a violation but worth noting
        // In some contexts, being outside data collection zones might be penalized
    }
    
    return violations;
}

void SemanticConstraintChecker::applyConstraintsToCostmap(CostMap& costmap) const {
    int width = costmap.getWidth();
    int height = costmap.getHeight();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Point point(x, y);
            double penalty = getConstraintPenalty(point);
            
            double current_cost = costmap.getCellCost(x, y);
            costmap.setCellCost(x, y, current_cost + penalty);
        }
    }
}

double SemanticConstraintChecker::getConstraintPenalty(const Point& point) const {
    double penalty = 0.0;
    
    // Get nearby objects that might impose constraints
    auto nearby_objects = semantic_map.getObjectsInRadius(point, 3.0);
    
    for (const auto& obj : nearby_objects) {
        double dx = obj.position.x - point.x;
        double dy = obj.position.y - point.y;
        double distance = std::sqrt(dx*dx + dy*dy);
        
        // Different penalties for different object types
        switch (obj.type) {
            case TRAFFIC_LIGHT:
            case TRAFFIC_SIGN:
                // Higher penalty for traffic-related objects
                if (distance < obj.radius) {
                    penalty += 50.0 * (1.0 - distance/obj.radius);
                }
                break;
                
            case BUILDING:
            case VEGETATION:
                // Medium penalty for obstacles
                if (distance < obj.radius) {
                    penalty += 30.0 * (1.0 - distance/obj.radius);
                }
                break;
                
            case PEDESTRIAN_CROSSING:
                // Special handling for pedestrian crossings
                if (distance < obj.radius) {
                    penalty += 20.0 * (1.0 - distance/obj.radius);
                }
                break;
                
            default:
                break;
        }
    }
    
    return penalty;
}

} // namespace aurora::planner