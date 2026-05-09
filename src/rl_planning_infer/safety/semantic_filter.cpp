// semantic_filter.cpp
#include "semantic_filter.h"
#include <algorithm>

namespace aurora::planner {
SemanticFilter::SemanticFilter(double conf_threshold)
    : confidence_threshold(conf_threshold) {
    // By default, don't filter any specific types
}

std::vector<SemanticObject> SemanticFilter::filterByConfidence(
    const std::vector<SemanticObject>& objects) const {
    std::vector<SemanticObject> filtered;
    
    for (const auto& obj : objects) {
        if (obj.confidence >= confidence_threshold) {
            filtered.push_back(obj);
        }
    }
    
    return filtered;
}

std::vector<SemanticObject> SemanticFilter::filterByType(
    const std::vector<SemanticObject>& objects, 
    const std::vector<SemanticType>& types) const {
    std::vector<SemanticObject> filtered;
    
    for (const auto& obj : objects) {
        // Check if object type is in the filter list
        if (std::find(types.begin(), types.end(), obj.type) != types.end()) {
            filtered.push_back(obj);
        }
    }
    
    return filtered;
}

std::vector<SemanticObject> SemanticFilter::filterObjects(
    const std::vector<SemanticObject>& objects) const {
    // First filter by confidence
    auto conf_filtered = filterByConfidence(objects);
    
    // If no specific types to filter, return confidence-filtered objects
    if (filter_types.empty()) {
        return conf_filtered;
    }
    
    // Otherwise, also filter by type
    return filterByType(conf_filtered, filter_types);
}

void SemanticFilter::setConfidenceThreshold(double threshold) {
    confidence_threshold = threshold;
}

void SemanticFilter::setFilterTypes(const std::vector<SemanticType>& types) {
    filter_types = types;
}

void SemanticFilter::addFilterType(SemanticType type) {
    if (std::find(filter_types.begin(), filter_types.end(), type) == filter_types.end()) {
        filter_types.push_back(type);
    }
}

void SemanticFilter::removeFilterType(SemanticType type) {
    filter_types.erase(
        std::remove(filter_types.begin(), filter_types.end(), type), 
        filter_types.end()
    );
}
}