#pragma once

#include <frm/core/geom.h>
#include <frm/core/math.h>

#include <apt/Pool.h>

#include <EASTL/vector.h>

////////////////////////////////////////////////////////////////////////////////
// Octree
// Octree space (suffix O) is in [-1,1] with 0 being the origin of the root node 
// (like NDC).
// https://geidav.wordpress.com/2014/07/18/advanced-octrees-1-preliminaries-insertion-strategies-and-max-tree-depth/
// https://geidav.wordpress.com/2014/08/18/advanced-octrees-2-node-representations/
//
// \todo
// - Need to prune dead branches - can't just check if all children are empty, 
//   need to know if the whole branch is empty.
// - Integer-only arithmetic (faster AABB checks?)
////////////////////////////////////////////////////////////////////////////////
class Octree
{
public:
	Octree(int _levelCount);
	~Octree();

	void update();

	void insert(const frm::vec4& _data, const frm::AlignedBox& _boxO);

	void debugDraw(const frm::vec3& _origin, const frm::vec3& _scale);
	frm::uint32 debugFindLeaf(const frm::vec3& _p) const;
	frm::vec4 debugGetCenterWidth(frm::uint32 _nodeIndex) const;

	frm::Buffer* getHiearchyBufferGPU()    { return m_bfHierarchyGPU; }
	frm::Buffer* getCenterWidthBufferGPU() { return m_bfCenterWidthGPU; }
	frm::Buffer* getDataBufferGPU()        { return m_bfDataGPU; }
	frm::Buffer* getDataCountBufferGPU()   { return m_bfDataCountGPU; }

private:
	// \todo this could be much more compact/efficient:
	// - Use pool indices instead of ptrs (requires special pool implementation).
	// - Store a ptr/index to a block of 8 children instead of 8 ptrs.
	// - Split data required for traversal from the payload.
	// - 1 byte mask to determine which children are valid?
	struct Node
	{
		bool                 m_isLeaf      = true;           // has no children
		bool                 m_isActive    = false;          // is the root of a non-empty branch
		Node*                m_parent      = nullptr;
		Node*                m_children[8] = { nullptr };
		int                  m_level       = 0;
		frm::vec3            m_centerO     = frm::vec3(0.0f);
		float                m_widthO      = 2.0f;
		eastl::vector<frm::vec4>  m_data;
	};

	apt::Pool<Node>      m_nodePool;
	Node*                m_rootNode           = nullptr;
	int                  m_levelCount         = 1;
	int                  m_maxLevel           = 0;
	int                  m_totalNodeCount     = 1;
	int                  m_maxNodeDataCount   = 0;

	union NodeGPU
	{
		struct Fields
		{
			frm::uint32 m_level      : 4;
			frm::uint32 m_childCount : 4;
			frm::uint32 m_childIndex : 12;
			frm::uint32 m_dataIndex  : 12;
		};
		Fields      m_fields;
		frm::uint32 m_value;

		NodeGPU(): m_value(0)
		{
		}
	};
	eastl::vector<NodeGPU>     m_hierarchyGPU;
	eastl::vector<frm::vec4>   m_centerWidthGPU;
	eastl::vector<frm::vec4>   m_dataGPU;
	eastl::vector<frm::uint32> m_dataCountGPU;
	frm::Buffer*               m_bfHierarchyGPU     = nullptr;
	frm::Buffer*               m_bfCenterWidthGPU   = nullptr;
	frm::Buffer*               m_bfDataGPU          = nullptr;
	frm::Buffer*               m_bfDataCountGPU     = nullptr;
	bool                       m_updateGPU          = true;

	// Return the octree space bounding box of _node.
	frm::AlignedBox getNodeBoundingBox(Node* _node) const;

	// Return whether box _outer contains _box _inner.
	bool contains(const frm::AlignedBox& _outer, const frm::AlignedBox& _inner) const;

	// Return whether box _outer contains _point.
	bool contains(const frm::AlignedBox& _outer, const frm::vec3& _point) const;

	// Split _node, allocate children. Note that all 8 possible children are created (no partial subdivison).
	void split(Node* _node);

	// Merge _node, free children.
	void merge(Node* _node);

	// Depth-first traversal of the quadtree starting at _root, call _onVisit for each node. Traversal proceeds to a node's children only if _onVisit returns true.
	template<typename OnVisit>
	void traverse(OnVisit&& _onVisit, Node* _rootNode);
};
