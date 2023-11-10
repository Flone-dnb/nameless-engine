#pragma once

// Standard.
#include <string>
#include <queue>
#include <mutex>
#include <memory>

namespace ne {
    class ShaderBindlessArrayIndexManager;

    /**
     * RAII-style class that holds an index into a bindless array and marks it as unused in destructor
     * so that other shader resources can use it later.
     */
    class BindlessArrayIndex {
        /** Only index manager can created indices. */
        friend class ShaderBindlessArrayIndexManager;

    public:
        BindlessArrayIndex() = delete;

        BindlessArrayIndex(const BindlessArrayIndex&) = delete;
        BindlessArrayIndex& operator=(const BindlessArrayIndex&) = delete;

        BindlessArrayIndex(BindlessArrayIndex&&) = delete;
        BindlessArrayIndex& operator=(BindlessArrayIndex&&) = delete;

        /**
         * Returns an actual index into the bindless array.
         *
         * @return Index into the bindless array.
         */
        unsigned int getActualIndex() const;

        /** Notifies the manager (that created this index) about index no longer being used. */
        ~BindlessArrayIndex();

    private:
        /**
         * Constructs a new array index.
         *
         * @param pManager                Manager that created this index.
         * @param iIndexIntoBindlessArray Actual index into a bindless array.
         */
        BindlessArrayIndex(ShaderBindlessArrayIndexManager* pManager, unsigned int iIndexIntoBindlessArray);

        /** Manager that created this index. */
        ShaderBindlessArrayIndexManager* pManager = nullptr;

        /** Actual index into a bindless array. */
        unsigned int iIndexIntoBindlessArray;
    };

    /**
     * Controls and provides indices into bindless arrays (defined in shaders).
     *
     * @remark If you need to bind something to a specific descriptor in a bindless array
     * this manager can give you an index to a descriptor (in the array) that you can use.
     */
    class ShaderBindlessArrayIndexManager {
        // Indices notify the manager in their destructor about no longer being used.
        friend class BindlessArrayIndex;

    public:
        ShaderBindlessArrayIndexManager() = delete;

        /**
         * Constructs a new index manager.
         *
         * @param sName Name of the manager (used for logging). It's recommended to not only specify
         * the shader resource name (that this manager is handling) but also some additional information
         * if possible.
         * @param iArraySize Optional parameter that could be specified to mark the
         * maximum possible number of elements in the array, if reached a warning will be logged.
         * Specify zero to disable logging and these checks.
         */
        ShaderBindlessArrayIndexManager(const std::string& sName, unsigned int iArraySize);

        /** Makes sure there are no active indices in use. */
        ~ShaderBindlessArrayIndexManager();

        /**
         * Returns a new (unused) index into the bindless array that this manager is handling.
         *
         * @return New index.
         */
        std::unique_ptr<BindlessArrayIndex> getNewIndex();

    private:
        /**
         * Called by index objects in their destructor to notify that an index is no longer used.
         *
         * @param iIndex Index that's no longer being used.
         */
        void onIndexNoLongerUsed(unsigned int iIndex);

        /** Groups mutex guarded data. */
        struct InternalData {
            /** Stores indices that were used some time and no longer being used. */
            std::queue<unsigned int> noLongerUsedIndices;

            /** Index that can be used. */
            unsigned int iNextFreeIndex = 0;

            /**
             * The number of currently existing (active, not destroyed) index objects that still reference
             * this manager.
             */
            size_t iActiveIndexCount = 0;
        };

        /** Internal data. */
        std::pair<std::recursive_mutex, InternalData> mtxData;

        /**
         * Optional parameter that could be specified during creation to mark the maximum possible number of
         * elements in the array.
         */
        const unsigned int iArraySize = 0;

        /** Name of the manager (used for logging). */
        const std::string sName;
    };
}