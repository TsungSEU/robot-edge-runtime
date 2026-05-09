// semantic_constraint.h
#ifndef SEMANTIC_CONSTRAINT_H
#define SEMANTIC_CONSTRAINT_H

#include "semantic_map.h"
#include <vector>

struct ConstraintViolation {
    SemanticObject object;
    Point violation_point;
    std::string description;
    double severity;
    
    ConstraintViolation(const SemanticObject& obj = SemanticObject(),
                       const Point& point = Point(),
                       const std::string& desc = "",
                       double sev = 0.0)
        : object(obj), violation_point(point), description(desc), severity(sev) {}
};

namespace aurora::planner {

class SemanticConstraintChecker {
private:
    const SemanticMap& semantic_map;
    
public:
    SemanticConstraintChecker(const SemanticMap& map) : semantic_map(map) {}
    
    /**
     * @brief Check if a path violates any semantic constraints
     * @param path Planned path to check
     * @return List of constraint violations
     */
    std::vector<ConstraintViolation> checkPathConstraints(
        const std::vector<Point>& path) const;
    
    /**
     * @brief Check if a point violates any semantic constraints
     */
    std::vector<ConstraintViolation> checkPointConstraints(const Point& point) const;
    
    /**
     * @brief Check traffic rule constraints (e.g., traffic lights, stop signs)
     */
    std::vector<ConstraintViolation> checkTrafficRules(const Point& point) const;
    
    /**
     * @brief Check data collection constraints
     */
    std::vector<ConstraintViolation> checkDataCollectionConstraints(
        const Point& point) const;
    
    /**
     * @brief Apply semantic constraints to costmap
     */
    void applyConstraintsToCostmap(CostMap& costmap) const;
    
    /**
     * @brief Get cost penalty for violating semantic constraints at a point
     */
    double getConstraintPenalty(const Point& point) const;
};

} // namespace aurora::planner

#endif // SEMANTIC_CONSTRAINT_H