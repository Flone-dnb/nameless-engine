#pragma once

// Standard.
#include <functional>
#include <optional>

// Custom.
#include "game/GameManager.h"

namespace ne {
    template <typename FunctionReturnType, typename... FunctionArgs> class NodeFunction;

    /**
     * `std::function` wrapper used for Node functions/lambdas with an additional
     * check (compared to the usual `std::function`): once the callback is called this class
     * will first check if the node, the callback function points to, is still spawned or not,
     * and if not spawned then the callback function will not be called to avoid running functions
     * on despawned nodes or hitting deleted memory.
     */
    template <typename FunctionReturnType, typename... FunctionArgs>
    class NodeFunction<FunctionReturnType(FunctionArgs...)> {
    public:
        NodeFunction() = default;

        /**
         * Constructor.
         *
         * @warning Don't capture `gc` pointers in `std::function` (as it's used under the hood).
         *
         * @param iNodeId  ID of the spawned node that "contains" the callback.
         * @param callback Points to the function/lambda of the spawned node with ID that was specified
         * as the previous argument.
         */
        NodeFunction(size_t iNodeId, const std::function<FunctionReturnType(FunctionArgs...)>& callback) {
            static_assert(
                std::is_same_v<FunctionReturnType, void>,
                "return type must be `void` - this is a current limitation");

            this->iNodeId = iNodeId;
            this->callback = callback;
        }

        /**
         * Copy constructor.
         *
         * @param other Other object.
         */
        NodeFunction(const NodeFunction& other) = default;

        /**
         * Copy assignment.
         *
         * @param other Other object.
         *
         * @return Result of copy assignment.
         */
        NodeFunction& operator=(const NodeFunction& other) = default;

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        NodeFunction(NodeFunction&& other) noexcept = default;

        /**
         * Move assignment.
         *
         * @param other Other object.
         *
         * @return Result of move assignment.
         */
        NodeFunction& operator=(NodeFunction&& other) noexcept = default;

        /**
         * Calls the stores callback function with the specified arguments.
         *
         * @remark If the node that the callback function points to is no longer spawned
         * the callback function will not be called.
         *
         * @remark If you are calling the callback function in a multi-threaded environment
         * (for ex. from a non-main thread) then once the callback function has started executing
         * it's up to you to guarantee that the node, the callback function points to, will not be
         * despawned while the callback function is executing (if it matters for you).
         *
         * @param args Arguments to pass to callback function call.
         *
         * @return `true` if the node, the callback function points to, was despawned and the callback
         * was not called to avoid running logic on despawned/deleted node, otherwise `false`.
         */
        bool operator()(FunctionArgs... args) {
            if (isNodeSpawned()) {
                callback(args...);
                return false;
            }

            return true;
        }

        /**
         * Checks if the node, the callback function points to, is still spawned or not.
         *
         * @remark You don't need to call this function before calling the callback
         * to check if the callback is still valid or not - it will be done automatically under the hood.
         *
         * @return `true` if the node is still spawned, `false` otherwise.
         */
        bool isNodeSpawned() {
            // Get game manager.
            const auto pGameManager = GameManager::get();
            if (pGameManager == nullptr) [[unlikely]] {
                return false;
            }

            // Make sure the game manager is not being destroyed.
            if (pGameManager->isBeingDestroyed()) [[unlikely]] {
                // Exit now because it might be dangerous to continue if we are on a non-main thread.
                return false;
            }

            return pGameManager->isNodeSpawned(iNodeId);
        }

    private:
        /** Points to the function of the spawned node with ID @ref iNodeId. */
        std::function<FunctionReturnType(FunctionArgs...)> callback;

        /** ID of the spawned node that contains @ref callback function. */
        size_t iNodeId = 0;
    };
} // namespace ne
