// semantic_map.h
#ifndef SEMANTIC_MAP_H
#define SEMANTIC_MAP_H

#include <vector>
#include <string>
#include "../maps/costmap.h"

enum SemanticType {
    ROAD = 0,
    LANE_MARKER = 1,
    TRAFFIC_SIGN = 2,
    TRAFFIC_LIGHT = 3,
    PEDESTRIAN_CROSSING = 4,
    PARKING_SPOT = 5,
    BUILDING = 6,
    VEGETATION = 7,
    DATA_COLLECTION_ZONE = 8,
    UNKNOWN = 9
};

struct SemanticObject {
    SemanticType type;
    Point position;
    double radius; // Bounding radius
    std::string label;
    double confidence;
    
    SemanticObject(SemanticType t = UNKNOWN, Point p = Point(), 
                   double r = 0.0, const std::string& l = "", 
                   double conf = 0.0)
        : type(t), position(p), radius(r), label(l), confidence(conf) {}
};

namespace aurora::planner {

class SemanticMap {
private:
    std::vector<SemanticObject> objects;
    int width, height;
    double resolution;
    
public:
    SemanticMap(int w = 100, int h = 100, double res = 1.0)
        : width(w), height(h), resolution(res) {}
    
    /**
     * @brief Add a semantic object to the map
     */
    void addObject(const SemanticObject& obj);
    
    /**
     * @brief Remove a semantic object from the map
     */
    void removeObject(const SemanticObject& obj);
    
    /**
     * @brief Get all semantic objects of a specific type
     */
    std::vector<SemanticObject> getObjectsByType(SemanticType type) const;
    
    /**
     * @brief Get all semantic objects within a radius of a point
     */
    std::vector<SemanticObject> getObjectsInRadius(const Point& center, double radius) const;
    
    /**
     * @brief Check if a point is in a specific semantic region
     */
    bool isInSemanticRegion(const Point& point, SemanticType type) const;
    
    /**
     * @brief Get semantic cost modifier for a position
     */
    double getSemanticCost(const Point& position) const;
    
    /**
     * @brief Update the semantic map with new objects
     */
    void updateMap(const std::vector<SemanticObject>& new_objects);
    
    /**
     * @brief Clear all objects from the map
     */
    void clear();
    
    // Getters
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    double getResolution() const { return resolution; }
    size_t getObjectCount() const { return objects.size(); }
};

} // namespace aurora::planner

#endif // SEMANTIC_MAP_H