// auto_route_optimize.cpp - Auto mode route optimizer
#include "auto_route_optimize.h"
#include "../observation/state_base.h"
#include "../observation/traits/auto_state_traits.h"
#include "../utils/planner_utils.h"
#include "common/log/logger.h"
#include <iostream>
#include <queue>
#include <cmath>
#include <random>
#include <functional>

using AutoState = aurora::planner::State<aurora::planner::AutoStateTraits>;


namespace aurora {
namespace planner {

constexpr int AUTO_STATE_DIM = 25;     // Auto mode: 25-dim state
constexpr int HUMANOID_STATE_DIM = 75;  // Humanoid mode: 75-dim state

AutoRoutePlanner::AutoRoutePlanner(double sparse_threshold,
                               double exploration_bonus,
                               double redundancy_penalty)
    : threshold_sparse(sparse_threshold)
    , exploration_bonus(exploration_bonus)
    , redundancy_penalty(redundancy_penalty) {
    // Initialize PPO agent with default configuration
    AutoPPOConfig config;
    ppo_agent_ = std::make_unique<AutoPPOAgent>(config);

    // Set default state_dim to 25 (including reachability)
    if (ppo_agent_) {
        ppo_agent_->setStateDim(AUTO_STATE_DIM);
    }
}

void AutoRoutePlanner::optRoute(const AutoMapData& map, const AutoDataStats& stats) {
    CostMap costmap = map.toCostMap();

    // Adjust costs based on data density statistics
    for (int y = 0; y < costmap.getHeight(); y++) {
        for (int x = 0; x < costmap.getWidth(); x++) {
            Point position(x, y);
            double current_cost = costmap.getCellCost(x, y);

            if (stats.dataDensity(position) < threshold_sparse) {
                costmap.setCellCost(x, y, current_cost - exploration_bonus);
            } else {
                costmap.setCellCost(x, y, current_cost + redundancy_penalty);
            }
        }
    }

    std::cout << "Auto route optimized based on data statistics" << std::endl;
}

std::vector<Point> AutoRoutePlanner::computeAStarPath(const CostMap& costmap,
                                                 const Point& start,
                                                 const Point& goal) {
    struct Node {
        Point pos;
        double g_cost;
        double f_cost;
        Node* parent;
    };

    auto cmp = [](const Node* a, const Node* b) { return a->f_cost > b->f_cost; };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> open_set(cmp);

    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};

    int width = costmap.getWidth();
    int height = costmap.getHeight();

    std::vector<std::vector<bool>> closed_set(height, std::vector<bool>(width, false));
    std::vector<std::vector<Node*>> node_map(height, std::vector<Node*>(width, nullptr));

    int start_x = static_cast<int>(std::round(start.x));
    int start_y = static_cast<int>(std::round(start.y));
    int goal_x = static_cast<int>(std::round(goal.x));
    int goal_y = static_cast<int>(std::round(goal.y));

    if (!costmap.isValidCell(start_x, start_y) || !costmap.isValidCell(goal_x, goal_y)) {
        std::cerr << "Invalid start or goal position" << std::endl;
        return {start, goal};
    }

    auto heuristic = [](int x1, int y1, int x2, int y2) -> double {
        double dx = static_cast<double>(x2 - x1);
        double dy = static_cast<double>(y2 - y1);
        return std::sqrt(dx * dx + dy * dy);
    };

    Node* start_node = new Node{Point(start_x, start_y), 0.0, 0.0, nullptr};
    start_node->f_cost = heuristic(start_x, start_y, goal_x, goal_y);
    open_set.push(start_node);
    node_map[start_y][start_x] = start_node;

    bool found = false;
    Node* goal_node = nullptr;

    while (!open_set.empty()) {
        Node* current = open_set.top();
        open_set.pop();

        int cx = static_cast<int>(current->pos.x);
        int cy = static_cast<int>(current->pos.y);

        if (cx == goal_x && cy == goal_y) {
            goal_node = current;
            found = true;
            break;
        }

        closed_set[cy][cx] = true;

        for (int i = 0; i < 4; i++) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (!costmap.isValidCell(nx, ny) || closed_set[ny][nx]) {
                continue;
            }

            double new_g = current->g_cost + costmap.getEffectiveCost(nx, ny) + 1.0;

            Node* neighbor = node_map[ny][nx];
            if (neighbor != nullptr && new_g >= neighbor->g_cost) {
                continue;
            }

            if (neighbor == nullptr) {
                neighbor = new Node{Point(nx, ny), new_g, 0.0, current};
                neighbor->f_cost = new_g + heuristic(nx, ny, goal_x, goal_y);
                open_set.push(neighbor);
                node_map[ny][nx] = neighbor;
            } else {
                neighbor->g_cost = new_g;
                neighbor->f_cost = new_g + heuristic(nx, ny, goal_x, goal_y);
                neighbor->parent = current;
            }
        }
    }

    std::vector<Point> path;
    if (found && goal_node != nullptr) {
        Node* node = goal_node;
        while (node != nullptr) {
            path.push_back(node->pos);
            node = node->parent;
        }
        std::reverse(path.begin(), path.end());
    } else {
        std::cout << "A* failed to find path, using direct route" << std::endl;
        path.push_back(start);
        path.push_back(goal);
    }

    // Cleanup
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (node_map[y][x] != nullptr) {
                delete node_map[y][x];
                node_map[y][x] = nullptr;
            }
        }
    }

    std::cout << "A* path computed from (" << start.x << "," << start.y
              << ") to (" << goal.x << "," << goal.y << ") with "
              << path.size() << " waypoints" << std::endl;

    return path;
}

std::vector<Point> AutoRoutePlanner::computePPOPath(const CostMap& costmap,
                                               const Point& start,
                                               const Point& goal) {
    std::vector<Point> path;

    if (!ppo_agent_) {
        std::cerr << "PPO agent not initialized!" << std::endl;
        return path;
    }

    Point current_pos = start;
    path.push_back(current_pos);

    const int max_steps = ppo_agent_->getMaxEpisodeSteps();
    int steps = 0;

    int current_orientation = 0;

    std::vector<std::function<Point(int)>> action_transforms = {
        [](int orientation) -> Point {
            switch(orientation) {
                case 0: return Point(1, 0);
                case 1: return Point(0, 1);
                case 2: return Point(-1, 0);
                case 3: return Point(0, -1);
                default: return Point(0, 0);
            }
        },
        [](int orientation) -> Point { return Point(0, 0); },
        [](int orientation) -> Point { return Point(0, 0); },
        [](int orientation) -> Point { return Point(0, 0); }
    };

    std::vector<int> last_actions(4, 0);

    while (steps < max_steps &&
           (std::abs(current_pos.x - goal.x) > 0.5 || std::abs(current_pos.y - goal.y) > 0.5)) {
        AutoState state;

        double norm_x = static_cast<double>(current_pos.x) / costmap.getWidth();
        double norm_y = static_cast<double>(current_pos.y) / costmap.getHeight();
        state.setFeature(AutoStateTraits::NORM_X, norm_x);
        state.setFeature(AutoStateTraits::NORM_Y, norm_y);

        for (int i = 0; i < 16; i++) {
            int offset_x = (i % 4) - 1;
            int offset_y = (i / 4) - 1;
            int check_x = static_cast<int>(current_pos.x) + offset_x;
            int check_y = static_cast<int>(current_pos.y) + offset_y;

            if (costmap.isValidCell(check_x, check_y)) {
                state.setFeature(AutoStateTraits::HEATMAP_START + i, costmap.getDataDensity(check_x, check_y));
            } else {
                state.setFeature(AutoStateTraits::HEATMAP_START + i, 0.0);
            }
        }

        for (size_t i = 0; i < last_actions.size(); i++) {
            state.setFeature(AutoStateTraits::ACTION_0 + i, static_cast<double>(last_actions[i]));
        }

        double remaining_budget = static_cast<double>(max_steps - steps) / max_steps;
        state.setFeature(AutoStateTraits::REMAINING_BUDGET, remaining_budget);

        double local_density = 0.0;
        int count = 0;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int check_x = static_cast<int>(current_pos.x) + dx;
                int check_y = static_cast<int>(current_pos.y) + dy;
                if (costmap.isValidCell(check_x, check_y)) {
                    local_density += costmap.getEffectiveCost(check_x, check_y);
                    count++;
                }
            }
        }
        if (count > 0) {
            local_density /= count;
        }
        state.setFeature(AutoStateTraits::LOCAL_DENSITY, local_density);

        int curr_x = static_cast<int>(current_pos.x);
        int curr_y = static_cast<int>(current_pos.y);
        double reachability = costmap.isValidCell(curr_x, curr_y) ?
                             costmap.getReachabilityScore(curr_x, curr_y) : 0.5;
        state.setFeature(AutoStateTraits::REACHABILITY, reachability);

        AD_DEBUG(AutoPlanner, "State includes reachability: %.3f at (%.1f, %.1f)",
                reachability, current_pos.x, current_pos.y);

        int action_idx = ppo_agent_->selectAction(state, true);

        last_actions.erase(last_actions.begin());
        last_actions.push_back(action_idx);

        switch(action_idx) {
            case 0:
            {
                Point movement = action_transforms[0](current_orientation);
                Point next_pos(current_pos.x + movement.x, current_pos.y + movement.y);

                if (costmap.isValidCell(static_cast<int>(next_pos.x), static_cast<int>(next_pos.y))) {
                    current_pos = next_pos;
                    path.push_back(current_pos);
                }
                break;
            }
            case 1:
                current_orientation = (current_orientation + 1) % 4;
                break;
            case 2:
                current_orientation = (current_orientation + 3) % 4;
                break;
            case 3:
                current_orientation = (current_orientation + 2) % 4;
                break;
            default:
                break;
        }

        steps++;
    }

    std::cout << "PPO path computed from (" << start.x << "," << start.y
              << ") to (" << goal.x << "," << goal.y << ") with "
              << path.size() << " waypoints" << std::endl;

    return path;
}

} // namespace planner
} // namespace aurora
