// semantic_filter.h
#ifndef SEMANTIC_FILTER_H
#define SEMANTIC_FILTER_H

#include "semantic_map.h"
#include <vector>


namespace aurora::planner {
class SemanticFilter {
private:
    double confidence_threshold;
    std::vector<SemanticType> filter_types;
    
public:
    SemanticFilter(double conf_threshold = 0.5);
    
    /**
     * @brief Filter semantic objects by confidence threshold
     */
    std::vector<SemanticObject> filterByConfidence(
        const std::vector<SemanticObject>& objects) const;
    
    /**
     * @brief Filter semantic objects by type
     */
    std::vector<SemanticObject> filterByType(
        const std::vector<SemanticObject>& objects, 
        const std::vector<SemanticType>& types) const;
    
    /**
     * @brief Filter semantic objects by both confidence and type
     */
    std::vector<SemanticObject> filterObjects(
        const std::vector<SemanticObject>& objects) const;
    
    /**
     * @brief Update confidence threshold
     */
    void setConfidenceThreshold(double threshold);
    
    /**
     * @brief Set filter types
     */
    void setFilterTypes(const std::vector<SemanticType>& types);
    
    /**
     * @brief Add a type to filter
     */
    void addFilterType(SemanticType type);
    
    /**
     * @brief Remove a type from filter
     */
    void removeFilterType(SemanticType type);
};

} // namespace aurora::planner
#endif // SEMANTIC_FILTER_H