#include "types/lifetimes.hpp"
#include <iostream>

using namespace arena::sema;
using namespace arena;

namespace {
    struct LifetimeNode {
        LifetimeId lifetime;
        size_t tarjan_id = 0;
        LifetimeNode *parent = nullptr;
        const error::Cause *parent_link = nullptr;
        const LifetimeConstraint *reached_by_constraint = nullptr;
        size_t lowlink = std::numeric_limits<size_t>::max();
        size_t order = 0;
        bool on_stack = false;
    };

    struct Scc {
        std::vector<LifetimeNode> nodes;
        std::vector<LifetimeConstraint> constraints;
    };

    struct LifetimeGraph {
        std::vector<LifetimeNode> nodes_by_tjid;
        std::vector<size_t> ltid_to_tjid;
    };

    /**
     * Use a variant of Tarjan's strongly connected components algorithm and union-find to produce
     * a topological ordering of lifetimes, plus unioning cycles.
     *
     * In order to account for the mixture of `=`, `<`, and `>` constraints (and eventually '>=',
     * "<="), we use a stack based top-down approach, where in addition to the standard Tarjan's
     * stack, we also keep a separate stack of equal and outlives nodes pending traversal. When
     * these stacks are empty, we can pop Tarjan's stack. This ensures that all equal nodes are
     * visited before outlives nodes in our topological sort.
     */
    class SccSolver {
        struct QueuedNode {
            LifetimeId lifetime;
            const LifetimeConstraint *constraint;
        };

    public:
        void build(const LifetimeGroup &group) {
            graph.nodes_by_tjid.reserve(group.get_max_lifetime_id().lt_id + 1);
            graph.ltid_to_tjid = std::vector<size_t>(group.get_max_lifetime_id().lt_id + 1,
                                                     std::numeric_limits<size_t>::max());

            auto final_lifetime_id = group.get_max_lifetime_id();
            for (size_t i = 0; i <= final_lifetime_id.lt_id; i++) {
                auto lifetime = LifetimeId{i};
                if (is_indexed(lifetime)) {
                    continue;
                }

                std::cout << "Processing root lifetime " << i << std::endl;
                process_node(lifetime, group);
            }

            for (auto &finished_node : graph.nodes_by_tjid) {
                auto root = union_find(finished_node);
                std::cout << "Updating order of lifetime " << finished_node.lifetime.lt_id
                          << " from " << finished_node.order << " to root order " << root.order
                          << std::endl;
                finished_node.order = root.order;
            }
        }

        void process_node(LifetimeId id,
                          const LifetimeGroup &group,
                          const LifetimeConstraint *reached_by_constraint = nullptr) {
            std::cout << "Processing lifetime " << id.lt_id << std::endl;

            auto node = &get_node(id);
            node->lowlink = node->tarjan_id;
            node->reached_by_constraint = reached_by_constraint;
            node->on_stack = true;
            tarjan_stack.push(node);

            union_equal_successors(id, group);

            std::vector<QueuedNode> pending_outlives;
            while (!outlives_stack.empty()) {
                auto *queued = outlives_stack.top();
                outlives_stack.pop();
                auto queued_lifetime = queued->lifetime;
                std::cout << "Popped outlives lifetime " << queued_lifetime.lt_id << " of lifetime "
                          << id.lt_id << " from stack" << std::endl;
                if (!is_indexed(queued_lifetime)) {
                    pending_outlives.push_back(*queued);
                    continue;
                }

                auto queued_node = &get_node(queued_lifetime);
                auto queued_root = &union_find(*queued_node);
                std::cout << "Already indexed lifetime has root " << queued_root->lifetime.lt_id
                          << std::endl;
                if (!queued_root->on_stack) {
                    std::cout << "Not on stack, skipping\n";
                    continue;
                }

                node->lowlink = std::min(node->lowlink, queued_root->tarjan_id);
                std::cout << "Updated lowlink of lifetime " << id.lt_id << " to " << node->lowlink
                          << std::endl;
            }

            for (const auto &queued : pending_outlives) {
                auto queued_lifetime = queued.lifetime;
                auto queued_constraint = queued.constraint;
                if (is_indexed(queued_lifetime)) {
                    std::cout << "Already indexed lifetime " << queued_lifetime.lt_id
                              << " from pending outlives, skipping\n";
                    continue;
                }

                process_node(queued_lifetime, group, queued_constraint);
            }

            if (node->lowlink == node->tarjan_id) {
                std::cout << "Found root of SCC with lifetime " << id.lt_id << std::endl;
                auto next_sort_id = sorted_id++;
                while (true) {
                    auto top = tarjan_stack.top();
                    tarjan_stack.pop();
                    top->on_stack = false;
                    top->order = next_sort_id;
                    std::cout << "Unifying lifetime " << top->lifetime.lt_id
                              << " with root lifetime " << id.lt_id << " and assigning order "
                              << next_sort_id << std::endl;
                    unify(*node, *top, top->reached_by_constraint);
                    if (top->lifetime == id) {
                        break;
                    }
                }
            } else {
                std::cout << "Lifetime " << id.lt_id
                          << " is not root of SCC, skipping Tarjan stack pop\n";
            }
        }

        void union_equal_successors(LifetimeId id, const LifetimeGroup &group) {
            auto lifetime = group.get_lifetime_by_id(id);
            auto node = &get_node(id);

            for (const auto &[eq_id, constraint] : lifetime->equals) {
                if (is_indexed(eq_id)) {
                    std::cout << "Skipped equal lifetime " << eq_id.lt_id << " of lifetime "
                              << id.lt_id << " because it's already indexed" << std::endl;
                    continue;
                }
                unify(*node, get_node(eq_id), &constraint);
                union_equal_successors(eq_id, group);
                std::cout << "Unifying equal lifetime " << eq_id.lt_id << " and lifetime "
                          << id.lt_id << std::endl;
            }

            for (const auto &[less_id, constraint] : lifetime->outlives) {
                if (!is_indexed(less_id)) {
                    std::cout << "Pushing outlives lifetime " << less_id.lt_id << " of lifetime "
                              << id.lt_id << " to stack" << std::endl;
                    outlives_stack.push(new QueuedNode{less_id, &constraint});
                    continue;
                }

                auto less_root = union_find(get_node(less_id));
                std::cout << "Union find of outlived lifetime " << less_id.lt_id << " is "
                          << less_root.lifetime.lt_id << std::endl;
                if (!less_root.on_stack) {
                    std::cout << "Not on stack, skipping\n";
                    continue;
                }

                node->lowlink = std::min(node->lowlink, less_root.tarjan_id);
                std::cout << "Updated lowlink of lifetime " << id.lt_id << " to " << node->lowlink
                          << std::endl;
            }
        }

        void unify(LifetimeNode &left,
                   LifetimeNode &right,
                   const LifetimeConstraint *constraint = nullptr) {
            const error::Cause *origin = nullptr;
            if (constraint != nullptr && constraint->origin.has_value()) {
                origin = &constraint->origin.value();
            }

            auto left_root = &union_find(left);
            auto right_root = &union_find(right);
            if (left_root == right_root) {
                return;
            }

            right_root->parent = &left;
            right_root->parent_link = origin;
        }

        LifetimeNode &get_node(LifetimeId id) {
            auto tarjan_id = graph.ltid_to_tjid[id.lt_id];
            if (tarjan_id != std::numeric_limits<size_t>::max()) {
                return graph.nodes_by_tjid[tarjan_id];
            }

            auto node = LifetimeNode{id, graph.nodes_by_tjid.size()};
            graph.nodes_by_tjid.push_back(node);
            graph.ltid_to_tjid[id.lt_id] = node.tarjan_id;
            return graph.nodes_by_tjid.back();
        }

        bool is_indexed(LifetimeId id) const {
            auto tarjan_id = graph.ltid_to_tjid[id.lt_id];
            return tarjan_id != std::numeric_limits<size_t>::max();
        }

        LifetimeNode &union_find(LifetimeNode &node) {
            if (node.parent == nullptr) {
                return node;
            }
            return union_find(*node.parent);
        }

        std::vector<error::Cause> get_union_path(const LifetimeNode &node) const {
            std::vector<error::Cause> path;
            auto current = &node;
            while (current->parent != nullptr) {
                if (current->parent_link != nullptr) {
                    path.push_back(*current->parent_link);
                }

                current = current->parent;
            }
            return path;
        }

        void detect_conflicts(LifetimeConstraint constraint,
                              const LifetimeGroup *lifetimes,
                              error::Reporter *errors) {
            auto left = get_node(constraint.left_id);
            auto right = get_node(constraint.right_id);

            if (constraint.relation == LifetimeRelation::Greater && left.order <= right.order) {
                report_outlives_conflict(&left, &right, constraint, lifetimes, errors);
            } else if (constraint.relation == LifetimeRelation::GreaterEqual && left.order < right.order) {
                report_outlives_conflict(&left, &right, constraint, lifetimes, errors);
            } else if (constraint.relation == LifetimeRelation::Less && left.order >= right.order) {
                report_outlives_conflict(&right, &left, constraint, lifetimes, errors);
            } else if (constraint.relation == LifetimeRelation::LessEqual && left.order > right.order) {
                report_outlives_conflict(&right, &left, constraint, lifetimes, errors);
            }
        }

        void report_outlives_conflict(LifetimeNode *lifetime,
                                      LifetimeNode *outlives,
                                      const LifetimeConstraint &constraint,
                                      const LifetimeGroup *lifetimes,
                                      error::Reporter *errors) {
            std::vector<error::Cause> path;
            if (constraint.origin.has_value()) {
                path.push_back(constraint.origin.value());
            }
            std::cout << "Left has parent_link " << (lifetime->parent_link != nullptr ? lifetime->parent_link->description : "none")
                      << " and right has parent_link "
                      << (outlives->parent_link != nullptr ? outlives->parent_link->description : "none")
                      << std::endl;
            auto union_path_left = get_union_path(*lifetime);
            auto union_path_right = get_union_path(*outlives);
            path.insert(path.end(), union_path_left.begin(), union_path_left.end());
            path.insert(path.end(), union_path_right.rbegin(), union_path_right.rend());

            auto name_left = lifetimes->get_lifetime_by_id(lifetime->lifetime)->get_debug_name();
            auto name_right = lifetimes->get_lifetime_by_id(outlives->lifetime)->get_debug_name();

            std::optional<error::Location> l = std::nullopt;

            for (const auto &p : path) {
                if (p.location.has_value()) {
                    l = p.location.value();
                    break;
                }
            }

            if (l.has_value()) {
                errors->E_L_OUTLV_VIOL(l.value(), name_left, name_right, path);
            } else {
                std::cout << "No location found for conflict between " << name_left << " and " << name_right
                          << ", reporting without location\n";
            }
        }

        const LifetimeGraph &get_graph() const { return graph; }

    private:
        LifetimeGraph graph;
        std::stack<QueuedNode *> outlives_stack;
        std::stack<LifetimeNode *> tarjan_stack;
        int sorted_id = 0;
    };

} // namespace

void LifetimeSolver::solve(const LifetimeGroup &group) {
    SccSolver solver;
    solver.build(group);
    auto graph = solver.get_graph();

    for (auto &node : graph.nodes_by_tjid) {
        std::cout << "Lifetime " << node.lifetime.lt_id << ": tarjan_id " << node.tarjan_id
                  << ", order " << node.order << ", parent tarjan_id: " << node.parent
                  << ", parent relation: "
                  << (node.parent_link != nullptr ? node.parent_link->description : "none")
                  << std::endl;
        auto lifetime = group.get_lifetime_by_id(node.lifetime);
    }

    for (auto &constraint : group.get_constraints()) {
        solver.detect_conflicts(constraint, &group, errors);
    }
}