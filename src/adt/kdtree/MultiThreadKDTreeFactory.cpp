#include <MultiThreadKDTreeFactory.h>
#include <KDTreeBuildType.h>
#include <surfaceinspector/maths/Scalar.hpp>

using SurfaceInspector::maths::Scalar;

// ***  CONSTRUCTION / DESTRUCTION  *** //
// ************************************ //
MultiThreadKDTreeFactory::MultiThreadKDTreeFactory(
    shared_ptr<SimpleKDTreeFactory> const kdtf,
    size_t const numJobs
) :
    SimpleKDTreeFactory(),
    kdtf(kdtf),
    tpNode(numJobs-1),
    minTaskPrimitives(32),
    numJobs(numJobs)
{
    /*
     * Handle node thread pool start pending tasks so when geometry-level
     * parallelization is used no node-level parallelization occurs before
     * it must (at adequate depth)
     */
    tpNode.setPendingTasks(numJobs-1);

    // Determine max geometry depth
    maxGeometryDepth = (int) std::floor(std::log2(numJobs));

    // Initialize shared task sequencer of master threads
    masters = std::make_shared<SharedTaskSequencer>(
        Scalar<int>::pow2(maxGeometryDepth)-1
    );

    /*
     * See SimpleKDTreeFactory constructor implementation to understand why
     *  it is safe to call virtual function here.
     */
    _buildRecursive =
        [&](
            KDTreeNode *parent,
            bool const left,
            vector<Primitive *> &primitives,
            int const depth,
            int const index
        ) -> KDTreeNode * {
            return this->buildRecursive(
                parent, left, primitives, depth, index
            );
        }
    ;
    kdtf->_buildRecursive = _buildRecursive;
}

// ***  KDTREE FACTORY METHODS  *** //
// ******************************** //
KDTreeNodeRoot * MultiThreadKDTreeFactory::makeFromPrimitivesUnsafe(
    vector<Primitive *> &primitives
){
    // Build the KDTree using a modifiable copy of primitives pointers vector
    KDTreeNodeRoot *root = (KDTreeNodeRoot *) kdtf->_buildRecursive(
        nullptr,        // Parent node
        false,          // Node is not left child, because it is root not child
        primitives,     // Primitives to be contained inside the KDTree
        0,              // Starting depth level (must be 0 for root node)
        0               // Starting index at depth 0 (must be 0 for root node)
    );
    tpNode.join();
    if(root == nullptr){
        /*
         * NOTICE building a null KDTree is not necessarily a bug.
         * It is a natural process that might arise for instance when upgrading
         *  from static scene to dynamic scene in the XmlSceneLoader.
         * This message is not reported at INFO level because it is only
         *  relevant for debugging purposes.
         */
        std::stringstream ss;
        ss  << "Null KDTree with no primitives was built";
        logging::DEBUG(ss.str());
    }
    else{
        computeKDTreeStats(root);
        reportKDTreeStats(root, primitives);
        if(buildLightNodes) lighten(root);
    }
    return root;
}

// ***  BUILDING METHODS  *** //
// ************************** //
KDTreeNode * MultiThreadKDTreeFactory::buildRecursive(
    KDTreeNode *parent,
    bool const left,
    vector<Primitive*> &primitives,
    int const depth,
    int const index
){
    if(depth <= maxGeometryDepth)
        return buildRecursiveGeometryLevel(
            parent, left, primitives, depth, index
        );
    return buildRecursiveNodeLevel(parent, left, primitives, depth, index);
}

KDTreeNode * MultiThreadKDTreeFactory::buildRecursiveGeometryLevel(
    KDTreeNode *parent,
    bool const left,
    vector<Primitive*> &primitives,
    int const depth,
    int const index
){
    // Compute work distribution strategy definition
    int const maxSplits = Scalar<int>::pow2(depth);
    int const alpha = (int) std::floor(numJobs/maxSplits);
    int const beta = numJobs % maxSplits;
    bool const excess = index < (maxSplits - beta); // True when 1 extra thread
    int const a = (excess) ? index*alpha : index*(alpha+1) - maxSplits + beta;
    int const b = (excess) ?
        alpha*(index+1) - 1 :
        index*(alpha+1) - maxSplits + beta + alpha;
    int const auxiliarThreads = b-a; // Subordinated threads
    int const assignedThreads = 1+auxiliarThreads; // Total threads
    // TODO Remove section ---
    std::stringstream ss;
    ss  << "Depth=" << depth << ", k=" << numJobs << ", 2^d=" << maxSplits
        << ", index=" << index << ", alpha=" << alpha << ", beta=" << beta
        << ", a=" << a << ", b=" << b
        << ", assignedThreads=" << assignedThreads
        << ", auxiliarThreads=" << auxiliarThreads
        << "\n--------------------\n" << std::endl
    ;
    std::cout << ss.str();
    // --- TODO Remove section

    // Geometry-level parallel processing
    // TODO Rethink : Prevent duplicated code (below is copy of SKDT::buildRec)
    if(primitives.empty()) return nullptr;
    KDTreeNode *node;
    if(depth > 0) node = new KDTreeNode();
    else node = new KDTreeNodeRoot();
    kdtf->computeNodeBoundaries(node, parent, left, primitives);
    kdtf->defineSplit(node, parent, primitives, depth);
    vector<Primitive*> leftPrimitives, rightPrimitives;
    kdtf->GEOM_populateSplits(
        primitives,
        node->splitAxis,
        node->splitPos,
        leftPrimitives,
        rightPrimitives,
        assignedThreads
    );

    // TODO Rethink : Implement geometry level building
    //return this->kdtf->buildRecursive(parent, left, primitives, depth, index);
    if(depth == maxGeometryDepth){
        // Move auxiliar threads from geometry thread pool to node thread pool
        if(auxiliarThreads > 0)
            tpNode.safeSubtractPendingTasks(auxiliarThreads);
        // Recursively build children nodes
        kdtf->buildChildrenNodes(
            node,
            parent,
            primitives,
            depth,
            index,
            leftPrimitives,
            rightPrimitives
        );
        // Allow one more thread to node-level pool when master thread finishes
        tpNode.safeSubtractPendingTasks(1);
    }
    else{
        kdtf->GEOM_buildChildrenNodes(
            node,
            parent,
            primitives,
            depth,
            index,
            leftPrimitives,
            rightPrimitives
        );
    }
    return node;
}

KDTreeNode * MultiThreadKDTreeFactory::buildRecursiveNodeLevel(
    KDTreeNode *parent,
    bool const left,
    vector<Primitive*> &primitives,
    int const depth,
    int const index
){
    bool posted = false;
    KDTreeBuildType *data = nullptr;
    if(primitives.size() >= minTaskPrimitives){
        data = new KDTreeBuildType(
            parent,
            left,
            primitives,
            depth,
            index
        );
        posted = tpNode.try_run_md_task(
            [&] (
                KDTreeNode *parent,
                bool const left,
                vector<Primitive*> &primitives,
                int const depth,
                int const index
            ) ->  void {
                if(left){
                    parent->left = this->kdtf->buildRecursive(
                        parent, left, primitives, depth, 2*index
                    );
                }
                else{
                    parent->right = this->kdtf->buildRecursive(
                        parent, left, primitives, depth, 2*index+1
                    );
                }
            },
            data
        );
    }
    if(posted){ // Null placeholder
        primitives.clear(); // Discard primitives, copy passed through data
        return nullptr;
    }
    else{ // Continue execution on current thread
        delete data; // Release data, it will not be used at all
        return this->kdtf->buildRecursive(
            parent, left, primitives, depth, 2*index+(left ? 0:1)
        );
    }
}
