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

        LifetimeNode &get_node(LifetimeId id) {
            auto tarjan_id = ltid_to_tjid[id.lt_id];
            if (tarjan_id != std::numeric_limits<size_t>::max()) {
                return nodes_by_tjid[tarjan_id];
            }

            auto node = LifetimeNode{id, nodes_by_tjid.size()};
            nodes_by_tjid.push_back(node);
            ltid_to_tjid[id.lt_id] = node.tarjan_id;
            return nodes_by_tjid.back();
        }

        const LifetimeNode &get_node(LifetimeId id) const {
            auto tarjan_id = ltid_to_tjid[id.lt_id];
            return nodes_by_tjid[tarjan_id];
        }

        bool is_indexed(LifetimeId id) const {
            auto tarjan_id = ltid_to_tjid[id.lt_id];
            return tarjan_id != std::numeric_limits<size_t>::max();
        }
    };

    /**
     * A djikstra search to find the shortest possible causal chain that violated a given lifetime
     * constraint.
     *
     * The search is driven by the `Target` class, which determines whether we search nodes with
     * greater, less, or equal lifetimes and what the upper/lower bound on those lifetimes are.
     *
     * Note that finding the shortest causal chain is not enough in itself for good error messages.
     * Some edges represent hard-coded constraints that have no source location, and some
     * constraints are only violated due to cycles. Therefore, we require the path traverses at
     * least one edge with a source location, and a change in node `order` value. Otherwise, our
     * best path is the shortest detected cycle from root to target back to itself. See
     * `CandidatePath::State` for more.
     */
    class DjikstraPathFinder {

        /**
         * To find the shortest causal chain from A to B, we only need to search in a certain
         * direction and in a certain range of `order`s.
         *
         * For example, if the root has `order` 2, and was supposed to be greater than the target
         * which has `order` 5, then we only need to search edges that have `order` between 2 and 5
         * inclusive, and we only need to search in the direction of greater lifetimes.
         *
         * This class manages the logic of determining the subset of edges that need to be
         * considered for a given search target.
         */
        struct Target {
            enum class Direction {
                Greater,
                Less,
            };

            Target(LifetimeNode begin, LifetimeNode end) : begin(begin), end(end) {
                min_order = std::min(begin.order, end.order);
                max_order = std::max(begin.order, end.order);
                if (begin.order < end.order) {
                    direction = Direction::Greater;
                } else {
                    direction = Direction::Less;
                }
            }

            bool should_traverse(LifetimeNode prev,
                                 LifetimeNode next,
                                 LifetimeConstraint constraint) const {
                if (next.order < min_order || next.order > max_order) {
                    return false;
                }

                switch (direction) {
                case Direction::Greater:
                    return next.order >= prev.order;
                case Direction::Less:
                    return next.order <= prev.order;
                }
            }

            size_t min_order;
            size_t max_order;
            Direction direction;
            LifetimeNode begin;
            LifetimeNode end;
        };

        struct CandidatePath {

            /**
             * The path we ultimately select must be the shortest path that fits certain criteria,
             * represented in this state enum.
             *
             * The final path must include at least one edge that corresponds to a source location
             * (i.e., an edge with an `error::Cause`), and it must either cross a change in `order`
             * value, or it must be a loop from the root to the target and back to itself.
             *
             * This enum encodes the state progression of a candidate in this manner.
             */
            enum class State : uint8_t {
                Initial = 0,
                HasLocation = 1,
                HasOrderChange = 2,
                PassesTarget = 4,
            };

            std::vector<error::Cause> path;
            LifetimeNode location;
            size_t distance;
            uint8_t state = static_cast<uint8_t>(State::Initial);

            CandidatePath with_step(const LifetimeConstraint &constraint,
                                    LifetimeNode next_location,
                                    bool reaches_target) const {
                std::vector<error::Cause> new_path = path;
                auto new_state = state;
                if (constraint.origin.has_value()) {
                    new_path.push_back(constraint.origin.value());
                    if (constraint.origin->location.has_value()) {
                        new_state |= static_cast<uint8_t>(State::HasLocation);
                    }
                }

                if (next_location.order != location.order) {
                    new_state |= static_cast<uint8_t>(State::HasOrderChange);
                }

                if (reaches_target) {
                    new_state |= static_cast<uint8_t>(State::PassesTarget);
                }

                return CandidatePath{new_path, next_location, distance + 1, new_state};
            }

            bool complete_at_target() {
                return ((state & static_cast<uint8_t>(State::HasLocation)) != 0) &&
                       ((state & static_cast<uint8_t>(State::HasOrderChange)) != 0);
            }

            bool complete_at_root() {
                return ((state & static_cast<uint8_t>(State::HasLocation)) != 0) &&
                       ((state & static_cast<uint8_t>(State::PassesTarget)) != 0);
            }
        };

        /**
         * A set of best-known distances required to reach a given node with a certain state.
         *
         * If we only needed to find the shortest path, we could just track the best distance to
         * reach each node. However, we may need to take a "detour" through additional nodes in
         * order to progress the `CandidatePath::State` to a completed path state. Therefore, we
         * need to track the shortest distance to reach each node in each state.
         *
         * Since `State` is a `uint8`, we can track a `std::array<..., 8>` of distances for each
         * node, one per possible state configuration.
         */
        struct NodeDistance {
            NodeDistance() { distances.fill(std::numeric_limits<size_t>::max()); }

            size_t get_for(uint8_t state) const { return distances[state]; }

            void set_for(uint8_t state, size_t distance) { distances[state] = distance; }

            std::array<size_t, 8> distances;
        };

    public:
        DjikstraPathFinder(const LifetimeGraph *graph,
                           const LifetimeGroup *lifetimes,
                           LifetimeNode begin,
                           LifetimeNode end)
            : graph(graph), lifetimes(lifetimes), target(begin, end),
              distances(graph->nodes_by_tjid.size()) {}

        std::vector<error::Cause> find_path() {
            auto base_path = CandidatePath{{}, target.begin, 0};
            candidate_paths.push(base_path);

            while (!candidate_paths.empty() && !best_path.has_value()) {
                auto candidate = candidate_paths.front();
                candidate_paths.pop();
                continue_path(candidate);
            }

            if (best_path.has_value()) {
                // TODO: We shouldn't have to do a dedupe stage, and this may break the causal
                // chain. 
                return dedupe(best_path->path);
            } else {
                return {};
            }
        }

        std::vector<error::Cause> dedupe(std::vector<error::Cause> input) {
            std::vector<error::Cause> result;

            for (auto &cause : input) {
                auto it = std::find(result.begin(), result.end(), cause);

                if (it == result.end()) {
                    result.push_back(cause);
                }
            }

            return result;
        }

        void continue_path(CandidatePath &candidate) {
            auto node = graph->get_node(candidate.location.lifetime);
            auto lifetime = lifetimes->get_lifetime_by_id(node.lifetime);
            for (const auto &[less_id, constraint] : lifetime->outlives) {
                explore_edge(candidate, less_id, constraint);
            }

            for (const auto &[greater_id, constraint] : lifetime->outlived_by) {
                explore_edge(candidate, greater_id, constraint);
            }

            for (const auto &[equal_id, constraint] : lifetime->equals) {
                explore_edge(candidate, equal_id, constraint);
            }
        }

        void explore_edge(CandidatePath &path, LifetimeId next, LifetimeConstraint constraint) {
            auto &prev = path.location;
            auto &node = graph->get_node(next);

            // Check that the edge is in the right direction and within the right bounds to
            // be a potential link in the causal chain.
            if (!target.should_traverse(prev, node, constraint)) {
                return;
            }

            bool is_target = node.lifetime == target.end.lifetime;
            bool is_root = node.lifetime == target.begin.lifetime;

            // Construct the new path, which may have a new state of overall progress.
            auto new_path = path.with_step(constraint, node, is_target);
            auto &node_distance = distances[node.tarjan_id];

            if (node_distance.get_for(new_path.state) <= new_path.distance) {
                return;
            }

            node_distance.set_for(new_path.state, new_path.distance);

            if ((is_target && new_path.complete_at_target()) ||
                (is_root && new_path.complete_at_root())) {
                best_path = std::move(new_path);
            } else {
                candidate_paths.push(std::move(new_path));
            }
        }

    private:
        const LifetimeGroup *lifetimes;
        const LifetimeGraph *graph;
        Target target;
        LifetimeRelation violated_relation;
        std::vector<NodeDistance> distances;
        std::queue<CandidatePath> candidate_paths;
        std::optional<CandidatePath> best_path;
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
                if (graph.is_indexed(lifetime)) {
                    continue;
                }

                // std::cout << "Processing root lifetime " << i << std::endl;
                process_node(lifetime, group);
            }

            for (auto &finished_node : graph.nodes_by_tjid) {
                auto root = union_find(finished_node);
                // std::cout << "Updating order of lifetime " << finished_node.lifetime.lt_id
                //           << " from " << finished_node.order << " to root order " << root.order
                //           << std::endl;
                finished_node.order = root.order;
            }
        }

        void process_node(LifetimeId id,
                          const LifetimeGroup &group,
                          const LifetimeConstraint *reached_by_constraint = nullptr) {
            // std::cout << "Processing lifetime " << id.lt_id << std::endl;

            auto node = &graph.get_node(id);
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
                // std::cout << "Popped outlives lifetime " << queued_lifetime.lt_id << " of
                // lifetime "
                //           << id.lt_id << " from stack" << std::endl;
                if (!graph.is_indexed(queued_lifetime)) {
                    pending_outlives.push_back(*queued);
                    continue;
                }

                auto queued_node = &graph.get_node(queued_lifetime);
                auto queued_root = &union_find(*queued_node);
                // std::cout << "Already indexed lifetime has root " << queued_root->lifetime.lt_id
                //           << std::endl;
                if (!queued_root->on_stack) {
                    // std::cout << "Not on stack, skipping\n";
                    continue;
                }

                node->lowlink = std::min(node->lowlink, queued_root->tarjan_id);
                // std::cout << "Updated lowlink of lifetime " << id.lt_id << " to " <<
                // node->lowlink
                //           << std::endl;
            }

            for (const auto &queued : pending_outlives) {
                auto queued_lifetime = queued.lifetime;
                auto queued_constraint = queued.constraint;
                if (graph.is_indexed(queued_lifetime)) {
                    // std::cout << "Already indexed lifetime " << queued_lifetime.lt_id
                    //           << " from pending outlives, skipping\n";
                    continue;
                }

                process_node(queued_lifetime, group, queued_constraint);
            }

            if (node->lowlink == node->tarjan_id) {
                // std::cout << "Found root of SCC with lifetime " << id.lt_id << std::endl;
                auto next_sort_id = sorted_id++;
                while (true) {
                    auto top = tarjan_stack.top();
                    tarjan_stack.pop();
                    top->on_stack = false;
                    top->order = next_sort_id;
                    // std::cout << "Unifying lifetime " << top->lifetime.lt_id
                    //           << " with root lifetime " << id.lt_id << " and assigning order "
                    //           << next_sort_id << std::endl;
                    unify(*node, *top, top->reached_by_constraint);
                    if (top->lifetime == id) {
                        break;
                    }
                }
            } else {
                // std::cout << "Lifetime " << id.lt_id
                //           << " is not root of SCC, skipping Tarjan stack pop\n";
            }
        }

        void union_equal_successors(LifetimeId id, const LifetimeGroup &group) {
            auto lifetime = group.get_lifetime_by_id(id);
            auto node = &graph.get_node(id);

            for (const auto &[eq_id, constraint] : lifetime->equals) {
                if (graph.is_indexed(eq_id)) {
                    // std::cout << "Skipped equal lifetime " << eq_id.lt_id << " of lifetime "
                    //           << id.lt_id << " because it's already indexed" << std::endl;
                    continue;
                }
                unify(*node, graph.get_node(eq_id), &constraint);
                union_equal_successors(eq_id, group);
                // std::cout << "Unifying equal lifetime " << eq_id.lt_id << " and lifetime "
                //           << id.lt_id << std::endl;
            }

            for (const auto &[less_id, constraint] : lifetime->outlives) {
                if (!graph.is_indexed(less_id)) {
                    // std::cout << "Pushing outlives lifetime " << less_id.lt_id << " of lifetime "
                    //           << id.lt_id << " to stack" << std::endl;
                    outlives_stack.push(new QueuedNode{less_id, &constraint});
                    continue;
                }

                auto less_root = union_find(graph.get_node(less_id));
                // std::cout << "Union find of outlived lifetime " << less_id.lt_id << " is "
                //           << less_root.lifetime.lt_id << std::endl;
                if (!less_root.on_stack) {
                    // std::cout << "Not on stack, skipping\n";
                    continue;
                }

                node->lowlink = std::min(node->lowlink, less_root.tarjan_id);
                // std::cout << "Updated lowlink of lifetime " << id.lt_id << " to " <<
                // node->lowlink
                //           << std::endl;
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
            auto left = graph.get_node(constraint.left_id);
            auto right = graph.get_node(constraint.right_id);

            if (constraint.relation == LifetimeRelation::Greater && left.order <= right.order) {
                report_outlives_conflict(&left, &right, constraint, lifetimes, errors);
            } else if (constraint.relation == LifetimeRelation::GreaterEqual &&
                       left.order < right.order) {
                report_outlives_conflict(&left, &right, constraint, lifetimes, errors);
            } else if (constraint.relation == LifetimeRelation::Less && left.order >= right.order) {
                report_outlives_conflict(&right, &left, constraint, lifetimes, errors);
            } else if (constraint.relation == LifetimeRelation::LessEqual &&
                       left.order > right.order) {
                report_outlives_conflict(&right, &left, constraint, lifetimes, errors);
            }
        }

        void report_outlives_conflict(LifetimeNode *lifetime,
                                      LifetimeNode *outlives,
                                      const LifetimeConstraint &constraint,
                                      const LifetimeGroup *lifetimes,
                                      error::Reporter *errors) {
            auto name_left = lifetimes->get_lifetime_by_id(lifetime->lifetime)->get_debug_name();
            auto name_right = lifetimes->get_lifetime_by_id(outlives->lifetime)->get_debug_name();

            auto path_finder = DjikstraPathFinder(&graph, lifetimes, *lifetime, *outlives);
            std::vector<error::Cause> path = path_finder.find_path();

            if (path.empty()) {
                std::cout << "No path found between " << name_left << " and " << name_right
                          << ", cannot report error without location\n";
                return;
            }

            std::optional<error::Location> l = std::nullopt;

            if (path.front().location.has_value()) {
                l = path.front().location;
                path.erase(path.begin());
            } else {
                for (const auto &p : path) {
                    if (p.location.has_value()) {
                        l = p.location.value();
                        break;
                    }
                }
            }

            if (l.has_value()) {
                errors->E_L_OUTLV_VIOL(l.value(), name_left, name_right, path);
            } else {
                std::cout << "No location found for conflict between " << name_left << " and "
                          << name_right << ", reporting without location\n";
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
        // std::cout << "Lifetime " << node.lifetime.lt_id << ": tarjan_id " << node.tarjan_id
        //           << ", order " << node.order << ", parent tarjan_id: " << node.parent
        //           << ", parent relation: "
        //           << (node.parent_link != nullptr ? node.parent_link->description : "none")
        //           << std::endl;
        auto lifetime = group.get_lifetime_by_id(node.lifetime);
    }

    for (auto &constraint : group.get_constraints()) {
        solver.detect_conflicts(constraint, &group, errors);
    }
}