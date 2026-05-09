// semantic_map.cpp
#include "semantic_map.h"
#include <algorithm>
#include <cmath>

namespace aurora::planner {
void SemanticMap::addObject(const SemanticObject& obj) {
    objects.push_back(obj);
}

void SemanticMap::removeObject(const SemanticObject& obj) {
    objects.erase(
        std::remove_if(objects.begin(), objects.end(),
                      [&obj](const SemanticObject& o) {
                          return o.type == obj.type && 
                                 o.position.x == obj.position.x && 
                                 o.position.y == obj.position.y;
                      }), 
        objects.end()
    );
}

std::vector<SemanticObject> SemanticMap::getObjectsByType(SemanticType type) const {
    std::vector<SemanticObject> result;
    
    for (const auto& obj : objects) {
        if (obj.type == type) {
            result.push_back(obj);
        }
    }
    
    return result;
}

std::vector<SemanticObject> SemanticMap::getObjectsInRadius(const Point& center, double radius) const {
    std::vector<SemanticObject> result;
    
    for (const auto& obj : objects) {
        double dx = obj.position.x - center.x;
        double dy = obj.position.y - center.y;
        double distance = std::sqrt(dx*dx + dy*dy);
        
        if (distance <= radius) {
            result.push_back(obj);
        }
    }
    
    return result;
}

bool SemanticMap::isInSemanticRegion(const Point& point, SemanticType type) const {
    auto type_objects = getObjectsByType(type);
    
    for (const auto& obj : type_objects) {
        double dx = obj.position.x - point.x;
        double dy = obj.position.y - point.y;
        double distance = std::sqrt(dx*dx + dy*dy);
        
        if (distance <= obj.radius) {
            return true;
        }
    }
    
    return false;
}

double SemanticMap::getSemanticCost(const Point& position) const {
    double cost = 0.0;
    
    // Iterate through all objects and accumulate cost based on proximity
    for (const auto& obj : objects) {
        double dx = obj.position.x - position.x;
        double dy = obj.position.y - position.y;
        double distance = std::sqrt(dx*dx + dy*dy);
        
        // Only consider objects within their radius of influence
        if (distance <= obj.radius) {
            // Different cost calculation based on object type
            switch (obj.type) {
                case ROAD:
                    // Roads have negative cost (preferred)
                    cost -= 10.0 * (1.0 - distance/obj.radius);
                    break;
                    
                case BUILDING:
                case VEGETATION:
                    // Obstacles have positive cost (avoided)
                    cost += 50.0 * (1.0 - distance/obj.radius);
                    break;
                    
                case TRAFFIC_SIGN:
                case TRAFFIC_LIGHT:
                    // Traffic elements have moderate cost
                    cost += 30.0 * (1.0 - distance/obj.radius);
                    break;
                    
                case DATA_COLLECTION_ZONE:
                    // Data collection zones have negative cost for data missions
                    cost -= 20.0 * (1.0 - distance/obj.radius);
                    break;
                    
                default:
                    // Default small cost
                    cost += 5.0 * (1.0 - distance/obj.radius);
                    break;
            }
        }
    }
    
    return cost;
}

void SemanticMap::updateMap(const std::vector<SemanticObject>& new_objects) {
    objects = new_objects;
}

void SemanticMap::clear() {
    objects.clear();
}

} // namespace aurora::planner