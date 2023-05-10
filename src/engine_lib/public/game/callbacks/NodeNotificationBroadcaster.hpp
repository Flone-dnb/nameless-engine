#pragma once

// Standard.
#include <mutex>
#include <unordered_map>
#include <vector>

// Custom.
#include "game/callbacks/NodeFunction.hpp"
#include "io/Logger.h"

namespace ne {
    class Node;

    /** Base class for notification broadcasters. */
    class NodeNotificationBroadcasterBase {
        // Only Node can create broadcasters because it has some additional protection code
        // to avoid shooting yourself in the foot. Additionally, the broadcaster is only valid
        // when the node that owns the broadcaster is spawned, otherwise the broadcaster does nothing.
        friend class Node;

    public:
        NodeNotificationBroadcasterBase() = default;
        virtual ~NodeNotificationBroadcasterBase() = default;

        NodeNotificationBroadcasterBase(const NodeNotificationBroadcasterBase&) = delete;
        NodeNotificationBroadcasterBase& operator=(const NodeNotificationBroadcasterBase&) = delete;

    protected:
        /**
         * Called by the node, that owns this broadcaster, when it's spawning.
         *
         * @param pOwnerNode Spawned node that owns this broadcaster.
         */
        virtual void onOwnerNodeSpawning(Node* pOwnerNode) = 0;

        /**
         * Called by the node, that owns this broadcaster, when it's despawning.
         *
         * @param pOwnerNode Node that owns this broadcaster.
         */
        virtual void onOwnerNodeDespawning(Node* pOwnerNode) = 0;
    };

    template <typename FunctionReturnType, typename... FunctionArgs> class NodeNotificationBroadcaster;

    /**
     * Implements publisher-subscriber pattern. Allows nodes to subscribe by specifying their
     * callback functions via `NodeFunction` and triggers them once the broadcast method is called.
     */
    template <typename FunctionReturnType, typename... FunctionArgs>
    class NodeNotificationBroadcaster<FunctionReturnType(FunctionArgs...)>
        : public NodeNotificationBroadcasterBase {
        // Only Node can create broadcasters because it has some additional protection code
        // to avoid shooting yourself in the foot. Additionally, the broadcaster is only valid
        // when the node that owns the broadcaster is spawned, otherwise the broadcaster does nothing.
        friend class Node;

    public:
        virtual ~NodeNotificationBroadcaster() override = default;

        NodeNotificationBroadcaster(const NodeNotificationBroadcaster&) = delete;
        NodeNotificationBroadcaster& operator=(const NodeNotificationBroadcaster&) = delete;

        /**
         * Executes all registered (subscribed) callbacks.
         *
         * @remark It's safe to call this function while your node is despawned, in this case the call
         * will be ignored and nothing will be broadcasted.
         *
         * @remark Additionally, before running registered callbacks, removes callbacks of despawned nodes.
         *
         * @param args Arguments to pass to subscribed callbacks.
         */
        void broadcast(FunctionArgs... args) {
            // Make sure we have a spawned owner node.
            std::scoped_lock ownerGuard(mtxSpawnedOwnerNode.first, mtxCallbacks.first);
            if (mtxSpawnedOwnerNode.second == nullptr) {
                // Being in a cleared state - do nothing.
                return;
            }

            // don't unlock mutexes yet

            // Make sure we are in the top level `broadcast` call (not called from some
            // callback that was triggered in other `broadcast` call)
            // because we will modify callbacks array.
            bool bIsTopLevelBroadcast = !bIsBroadcasting.test();
            if (bIsTopLevelBroadcast) {
                // Mark the start of broadcasting (start of working with callbacks).
                bIsBroadcasting.test_and_set();

                {
                    // Add new pending callbacks.
                    std::scoped_lock guard(mtxCallbacksToAdd.first);
                    for (auto& [iBindingId, callback] : mtxCallbacksToAdd.second) {
                        mtxCallbacks.second[iBindingId] = std::move(callback);
                    }
                    mtxCallbacksToAdd.second.clear();
                }

                {
                    // Remove callbacks marked as "to be removed".
                    std::scoped_lock guard(mtxCallbacksToRemove.first);
                    for (const auto& iBindingId : mtxCallbacksToRemove.second) {
                        // Make sure a binding with this ID exists.
                        auto it = mtxCallbacks.second.find(iBindingId);
                        if (it == mtxCallbacks.second.end()) [[unlikely]] {
                            Logger::get().error(
                                fmt::format(
                                    "a callback with binding ID {} was marked to be removed from a "
                                    "broadcaster but broadcaster does not have a callback with this ID",
                                    iBindingId),
                                sNodeNotificationBroadcasterLogCategory);
                            continue;
                        }
                        mtxCallbacks.second.erase(it);
                    }
                    mtxCallbacksToRemove.second.clear();
                }

                // Erase no longer valid callbacks.
                std::erase_if(mtxCallbacks.second, [](auto& item) { return !item.second.isNodeSpawned(); });
            }

            // Call registered callbacks.
            for (auto& [iBindingId, callback] : mtxCallbacks.second) {
                callback(args...);

                // Make sure our owner node is still spawned because the callback we just
                // called could have despawned the owner node.
                if (mtxSpawnedOwnerNode.second == nullptr) {
                    // Owner node was despawned and all callbacks were removed, exit.
                    break;
                }
            }

            if (bIsTopLevelBroadcast) {
                // Finished broadcasting.
                bIsBroadcasting.clear();
            }
        }

        /**
         * Adds the specified callback to be registered in the broadcaster so that the callback
         * will be triggered on the next @ref broadcast call.
         *
         * @param callback Callback to register.
         *
         * @return Unique ID (only unique relative to this broadcaster) of the registered callback.
         * You can save this ID if you would need to unsubscribe later (see @ref unsubscribe),
         * otherwise just ignore this returned value.
         */
        size_t subscribe(const NodeFunction<FunctionReturnType(FunctionArgs...)>& callback) {
            std::scoped_lock callbacksGuard(mtxCallbacks.first);

            // Generate new binding ID.
            const auto iNewBindingId = iAvailableBindingId.fetch_add(1);
            if (iNewBindingId + 1 == ULLONG_MAX) [[unlikely]] {
                Logger::get().warn(
                    fmt::format(
                        "\"next available broadcaster binding ID\" is at its maximum value: {}, another "
                        "subscribed callback will cause an overflow",
                        iNewBindingId + 1),
                    sNodeNotificationBroadcasterLogCategory);
            }

            // Check if we are inside of a `broadcast` call.
            if (bIsBroadcasting.test()) {
                // We are inside of a `broadcast` call (this function is probably called from some
                // callback that we called inside of our `broadcast` call), don't modify callbacks array
                // as we are iterating over it. Instead, add this callback as "pending to be added".
                std::scoped_lock callbacksToAddGuard(mtxCallbacksToAdd.first);
                mtxCallbacksToAdd.second[iNewBindingId] = callback;
            } else {
                // We are not inside of a `broadcast` call, it's safe to modify callbacks array.
                mtxCallbacks.second[iNewBindingId] = callback;
            }

            return iNewBindingId;
        }

        /**
         * Removes a previously added callback (see @ref subscribe) by its binding ID.
         *
         * @remark You don't need to unsubscribe when your subscribed node is being despawned/destroyed
         * as this is done automatically. Each @ref broadcast call removes callbacks of despawned nodes.
         *
         * @param iBindingId ID of the binding to remove.
         */
        void unsubscribe(size_t iBindingId) {
            {
                // First, look if this binding is still pending to be added.
                std::scoped_lock callbacksToAddGuard(mtxCallbacksToAdd.first);
                auto it = mtxCallbacksToAdd.second.find(iBindingId);
                if (it != mtxCallbacksToAdd.second.end()) {
                    // Just remove it from the pending array.
                    mtxCallbacksToAdd.second.erase(it);
                    return;
                }
            }

            // Make sure the binding exists in the main array.
            std::scoped_lock callbacksGuard(mtxCallbacks.first);
            auto it = mtxCallbacks.second.find(iBindingId);
            if (it == mtxCallbacks.second.end()) [[unlikely]] {
                Logger::get().error(
                    fmt::format("callback with binding ID {} was not found in the broadcaster", iBindingId),
                    sNodeNotificationBroadcasterLogCategory);
                return;
            }

            // Check if we are inside of a `broadcast` call.
            if (bIsBroadcasting.test()) {
                // We are inside of a `broadcast` call (this function is probably called from some
                // callback that we called inside of our `broadcast` call), don't modify callbacks array
                // as we are iterating over it. Instead, add this binding ID as "pending to be removed".
                std::scoped_lock callbacksToRemoveGuard(mtxCallbacksToRemove.first);
                mtxCallbacksToRemove.second.push_back(iBindingId);
            } else {
                // We are not inside of a `broadcast` call, it's safe to modify callbacks array.
                mtxCallbacks.second.erase(it);
            }
        }

        /**
         * Returns the current estimated number of subscribers.
         *
         * The returned number is called "estimated" because right now we don't know if some nodes
         * that subscribed to this broadcaster have despawned or not, callbacks of despawned nodes
         * are only removed in @ref broadcast calls. The only thing that we can say for sure is that
         * the actual number of spawned subscribers is either equal to the returned value or smaller than it.
         *
         * @return Estimated number of spawned subscribers.
         */
        size_t getSubscriberCount() {
            std::scoped_lock guard(mtxCallbacks.first, mtxCallbacksToAdd.first, mtxCallbacksToRemove.first);

            // About returning "estimated" number:
            // We might check `bIsBroadcasting` and remove callbacks of despawned nodes
            // but if we are being inside of a `broadcast` call we still would not know
            // if the returned number is correct or not so we generally can't guarantee
            // that the returned number is 100% correct.

            const auto iCurrentPlusPending = mtxCallbacks.second.size() + mtxCallbacksToAdd.second.size();
            const auto iPendingToBeRemoved = mtxCallbacksToRemove.second.size();

            // Make extra sure everything is correct.
            if (iCurrentPlusPending < iPendingToBeRemoved) [[unlikely]] {
                Logger::get().error(
                    fmt::format(
                        "there are more callbacks to be removed than all existing callbacks plus "
                        "pending to be added: currently registered: {}, pending to be added: {}, pending to "
                        "be removed: {}",
                        mtxCallbacks.second.size(),
                        mtxCallbacksToAdd.second.size(),
                        mtxCallbacksToRemove.second.size()),
                    sNodeNotificationBroadcasterLogCategory);
                return 0;
            }

            return iCurrentPlusPending - iPendingToBeRemoved;
        }

    protected:
        NodeNotificationBroadcaster() { mtxSpawnedOwnerNode.second = nullptr; };

        /**
         * Called by the node, that owns this broadcaster, when it's spawning.
         *
         * @param pOwnerNode Spawned node that owns this broadcaster.
         */
        virtual void onOwnerNodeSpawning(Node* pOwnerNode) override {
            std::scoped_lock ownerGuard(mtxSpawnedOwnerNode.first);

            // Make sure we don't have an owner.
            if (mtxSpawnedOwnerNode.second != nullptr) {
                Error error(
                    "some node has notified a broadcaster about being spawned but this broadcaster already "
                    "has an owner node");
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Save new owner.
            mtxSpawnedOwnerNode.second = pOwnerNode;
        }

        /**
         * Called by the node, that owns this broadcaster, when it's despawning.
         *
         * @param pOwnerNode Node that owns this broadcaster.
         */
        virtual void onOwnerNodeDespawning(Node* pOwnerNode) override {
            std::scoped_lock ownerGuard(mtxSpawnedOwnerNode.first);

            // Make sure the specified owner is indeed our owner node.
            if (mtxSpawnedOwnerNode.second != pOwnerNode) [[unlikely]] {
                Logger::get().error(
                    "some node notified a broadcaster about it being despawned but this "
                    "broadcaster's owner is not this node",
                    sNodeNotificationBroadcasterLogCategory);
                return;
            }

            // Clear pointer to the owner node (to mark "cleared" state and avoid broadcasting).
            mtxSpawnedOwnerNode.second = nullptr;

            removeAllCallbacks();
        }

    private:
        /** Removes all registered callbacks (including callbacks that are pending to be added/removed). */
        void removeAllCallbacks() {
            {
                std::scoped_lock guard(mtxCallbacks.first);
                mtxCallbacks.second.clear();
            }

            {
                std::scoped_lock guard(mtxCallbacksToAdd.first);
                mtxCallbacksToAdd.second.clear();
            }

            {
                std::scoped_lock guard(mtxCallbacksToRemove.first);
                mtxCallbacksToRemove.second.clear();
            }
        }

        /**
         * Stores map that should be used with the mutex.
         * Map contains pairs of "binding ID" - "callback".
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<size_t, NodeFunction<FunctionReturnType(FunctionArgs...)>>>
            mtxCallbacks;

        /**
         * Stores map that should be used with the mutex.
         * Map contains pairs of "binding ID" - "callback" to add to @ref mtxCallbacks.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<size_t, NodeFunction<FunctionReturnType(FunctionArgs...)>>>
            mtxCallbacksToAdd;

        /**
         * Stores array that should be used with the mutex.
         * Array contains binding IDs to remove from @ref mtxCallbacks.
         */
        std::pair<std::recursive_mutex, std::vector<size_t>> mtxCallbacksToRemove;

        /** Information about the owner node. */
        std::pair<std::recursive_mutex, Node*> mtxSpawnedOwnerNode;

        /** Stores the next free (available for use) binding ID. */
        std::atomic<size_t> iAvailableBindingId{0};

        /** Determines whether we are currently broadcasting or not. */
        std::atomic_flag bIsBroadcasting{};

        /** Name of the category used for logging. */
        static inline const auto sNodeNotificationBroadcasterLogCategory = "Node Notification Broadcaster";
    };
} // namespace ne
