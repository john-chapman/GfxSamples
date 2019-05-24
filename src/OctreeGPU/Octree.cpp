#include "Octree.h"

#include <frm/core/geom.h>
#include <frm/core/Buffer.h>
#include <frm/core/Profiler.h>

#include <apt/log.h>
#include <apt/memory.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

#include <EASTL/fixed_vector.h>

#if 0
	#define LOG_DBG(...) APT_LOG(__VA_ARGS__)
#else
	#define LOG_DBG(...)
#endif

using namespace apt;
using namespace frm;

static Im3d::Color GetLevelColor(int _lod)
{
	static const ImU32 kLevelColors[] =
	{
		Im3d::Color_Red,
		Im3d::Color_Green,
		Im3d::Color_Blue,
		Im3d::Color_Magenta,
		Im3d::Color_Yellow,
		Im3d::Color_Cyan
	};
	return kLevelColors[_lod % APT_ARRAY_COUNT(kLevelColors)];
}

// PUBLIC

Octree::Octree(int _levelCount)
	: m_nodePool(512)
{
	m_levelCount = _levelCount;
	m_maxLevel   = _levelCount - 1;
	m_rootNode   = m_nodePool.alloc(); // Node defaults are root
}

Octree::~Octree()
{
	merge(m_rootNode);
	m_nodePool.free(m_rootNode);
}

void Octree::update()
{
	PROFILER_MARKER_CPU("Octree::update");

	APT_STATIC_ASSERT(sizeof(NodeGPU) == sizeof(uint32));
	if (m_updateGPU)
	{	PROFILER_MARKER_CPU("Build GPU Nodes");
		m_hierarchyGPU.clear();
		m_centerWidthGPU.clear();
		m_dataGPU.clear();
		m_dataCountGPU.clear();

		m_hierarchyGPU.push_back(NodeGPU());
		m_centerWidthGPU.push_back(vec4(m_rootNode->m_centerO, m_rootNode->m_widthO));
		m_dataCountGPU.push_back(m_rootNode->m_data.size());
	
		eastl::fixed_vector<eastl::pair<Node*, size_t>, 32> tstack; // track the node *and* the flat index
		tstack.push_back(eastl::make_pair(m_rootNode, 0));
		while (!tstack.empty())
		{
			auto nodeAndIndex = tstack.back();
			Node* node = nodeAndIndex.first;
			size_t index = nodeAndIndex.second;
			APT_ASSERT(node);
			tstack.pop_back();
	
			if (!node->m_isActive)
			{
				continue;
			}

			m_hierarchyGPU[index].m_fields.m_dataIndex = m_dataGPU.size();			
			for (auto& data : node->m_data)
			{
				m_dataGPU.push_back(data);
			}

			if (node->m_isLeaf)
			{
				continue;
			}
	
			for (Node* child : node->m_children)
			{
				if (child->m_isActive)
				{
					if (m_hierarchyGPU[index].m_fields.m_childCount == 0)
					{
						m_hierarchyGPU[index].m_fields.m_childIndex = m_hierarchyGPU.size();
					}
					++m_hierarchyGPU[index].m_fields.m_childCount;
					tstack.push_back(eastl::make_pair(child, m_hierarchyGPU.size()));

					NodeGPU& nodeGPU = m_hierarchyGPU.push_back();
					nodeGPU.m_fields.m_level = child->m_level;
					m_centerWidthGPU.push_back(vec4(child->m_centerO, child->m_widthO));
					m_dataCountGPU.push_back(child->m_data.size());
				}
			}
		}

		if (m_bfHierarchyGPU)
		{
			Buffer::Destroy(m_bfHierarchyGPU);
		}
		m_bfHierarchyGPU = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(NodeGPU) * m_hierarchyGPU.size(), GL_NONE, m_hierarchyGPU.data());
		m_bfHierarchyGPU->setName("bfNodeHierarchy");

		if (m_bfCenterWidthGPU)
		{
			Buffer::Destroy(m_bfCenterWidthGPU);
		}
		m_bfCenterWidthGPU = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * m_centerWidthGPU.size(), GL_NONE, m_centerWidthGPU.data());
		m_bfCenterWidthGPU->setName("bfNodeCenterWidth");

		if (m_bfDataGPU)
		{
			Buffer::Destroy(m_bfDataGPU);
		}
		m_bfDataGPU = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Max((size_t)1, m_dataGPU.size()), GL_NONE, m_dataGPU.data());
		m_bfDataGPU->setName("bfNodeData");

		if (m_bfDataCountGPU)
		{
			Buffer::Destroy(m_bfDataCountGPU);
		}
		m_bfDataCountGPU = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(uint32) * Max((size_t)1, m_dataCountGPU.size()), GL_NONE, m_dataCountGPU.data());
		m_bfDataCountGPU->setName("bfNodeDataCount");
	}
}

void Octree::insert(const vec4& _data, const AlignedBox& _boxO)
{
	const float dataSize = _boxO.m_max.x - _boxO.m_min.x; // octree space bounds are always cubes

	eastl::fixed_vector<Node*, 32> tstack;
	tstack.push_back(m_rootNode);
	while (!tstack.empty()) 
	{
		auto node = tstack.back();
		APT_ASSERT(node);
		tstack.pop_back();

		m_maxNodeDataCount = Max(m_maxNodeDataCount, (int)node->m_data.size());
	
	 // if we traverse to this node either it contains data or is on a branch containing data
		node->m_isActive = true;

	 // if _node is an empty leaf or is at the max level, insert here
		if (node->m_isLeaf && (node->m_data.size() == 0 || node->m_level == m_maxLevel))
		{
			node->m_data.push_back(_data);
			break;
		}

	 // if _data can't fit into a child, insert here
		if (dataSize > node->m_widthO / 2.0f)
		{
			node->m_data.push_back(_data);
			break;
		}

	 // see if the data will fit into a child
	 // \todo avoid splitting if the data won't really fit in any child node, i.e. compute the BBs first
		bool splitHere = false;
		if (node->m_isLeaf)
		{
			splitHere = true;
			split(node);
		}
		bool recurseToChild = false;
		for (Node* child : node->m_children)
		{
			AlignedBox childBox = getNodeBoundingBox(child);
			if (contains(childBox, _boxO))
			{
				tstack.push_back(child);
				recurseToChild = true;
				break;
			}
		}
		if (recurseToChild)
		{
			continue;
		}
		
	 // else insert here
		if (splitHere)
		{
			merge(node); // undo the split
		}	 
		node->m_data.push_back(_data);
		break;
	}
}

void Octree::debugDraw(const vec3& _origin, const vec3& _scale)
{
	ImGui::Text("%d nodes", m_totalNodeCount);
	ImGui::Text("%d max node data count", m_maxNodeDataCount);

	Im3d::PushDrawState();
	#if 0
	{
		traverse(
			[&](Node* _node)
			{
				vec3 nodeCenter    = _origin + _node->m_centerO * _scale;
				vec3 nodeHalfWidth = vec3(_node->m_widthO / 2.0f) * _scale;
				vec3 nodeMin       = nodeCenter - nodeHalfWidth;
				vec3 nodeMax       = nodeCenter + nodeHalfWidth;
				
				if (_node->m_level == 0)
				{
					Im3d::SetColor(Im3d::Color_White);
					Im3d::SetSize(3.0f);
					Im3d::DrawAlignedBox(nodeMin, nodeMax);
				}
				if (_node->m_isLeaf)
				{
					Im3d::SetColor(GetLevelColor(_node->m_level));
					Im3d::PushEnableSorting();
					Im3d::PushAlpha(Im3d::GetAlpha() * 0.1f);
					Im3d::DrawAlignedBoxFilled(nodeMin, nodeMax);
					Im3d::PopAlpha();
					Im3d::PopEnableSorting();

					Im3d::SetColor(Im3d::Color_White);
					Im3d::SetSize(2.0f);
					Im3d::DrawAlignedBox(nodeMin, nodeMax);
					return false;
				}

				return true;
			},
			m_rootNode
			);
	}
	#else
	{
		eastl::fixed_vector<size_t, 32> tstack;
		tstack.push_back(0);
		while (!tstack.empty())
		{
			size_t nodeIndex = tstack.back();
			tstack.pop_back();
			const NodeGPU& node = m_hierarchyGPU[nodeIndex];
			const vec4& nodeCenterWidth = m_centerWidthGPU[nodeIndex];

			if (node.m_fields.m_childCount > 0)
			{
				for (size_t i = 0; i < (size_t)node.m_fields.m_childCount; ++i)
				{
					tstack.push_back((size_t)node.m_fields.m_childIndex + i);
				}
			}

			if (node.m_fields.m_childCount == 0)
			{
				vec3 nodeCenter    = _origin + nodeCenterWidth.xyz() * _scale;
				vec3 nodeHalfWidth = vec3(nodeCenterWidth.w / 2.0f) * _scale;
				vec3 nodeMin       = nodeCenter - nodeHalfWidth;
				vec3 nodeMax       = nodeCenter + nodeHalfWidth;

				Im3d::SetColor(Im3d::Color_White);
				Im3d::SetSize(2.0f);
				Im3d::DrawAlignedBox(nodeMin, nodeMax);

				Im3d::PushEnableSorting();
				Im3d::SetColor(GetLevelColor((int)node.m_fields.m_level));
				Im3d::PushAlpha(Im3d::GetAlpha() * 0.1f);
				Im3d::DrawAlignedBoxFilled(nodeMin, nodeMax);
				Im3d::PopAlpha();
				Im3d::PopEnableSorting();
			}
		}
	}
	#endif
	Im3d::PopDrawState();
}

uint32 Octree::debugFindLeaf(const frm::vec3& _p) const
{
	uint32 tstack[32];
	int tstackTop = 0;
	tstack[0] = 0;
	while (tstackTop != -1)
	{
		uint32 nodeIndex = tstack[tstackTop];
		--tstackTop;

		vec4 nodeCenterWidth = m_centerWidthGPU[nodeIndex];
		vec3 nodeMin = nodeCenterWidth.xyz() - vec3(nodeCenterWidth.w) * 0.5f;
		vec3 nodeMax = nodeCenterWidth.xyz() + vec3(nodeCenterWidth.w) * 0.5f;
		
		if (contains(AlignedBox(nodeMin, nodeMax), _p))
		{
		 // if this is a leaf node, stop
			uint32 childCount = m_hierarchyGPU[nodeIndex].m_fields.m_childCount;
			if (childCount == 0)
			{
				return nodeIndex;
			}

		 // else push children onto stack
			uint32 childIndex = m_hierarchyGPU[nodeIndex].m_fields.m_childIndex;
			for (uint32 i = 0; i < childCount; ++i)
			{
				tstack[++tstackTop] = childIndex + i;
				APT_ASSERT(tstackTop < APT_ARRAY_COUNT(tstack));
			}
		}
	}
	return 0;
}

vec4 Octree::debugGetCenterWidth(frm::uint32 _nodeIndex) const
{
	return m_centerWidthGPU[_nodeIndex];
}

// PRIVATE

AlignedBox Octree::getNodeBoundingBox(Node* _node) const
{
	vec3 nodeHalfWidth = vec3(_node->m_widthO / 2.0f);
	vec3 nodeMin       = _node->m_centerO - nodeHalfWidth;
	vec3 nodeMax       = _node->m_centerO + nodeHalfWidth;
	return AlignedBox(nodeMin, nodeMax);
}

bool Octree::contains(const AlignedBox& _outer, const AlignedBox& _inner) const
{
	return true
		&& _inner.m_min.x >= _outer.m_min.x
		&& _inner.m_min.y >= _outer.m_min.y
		&& _inner.m_min.z >= _outer.m_min.z
		&& _inner.m_max.x <= _outer.m_max.x
		&& _inner.m_max.y <= _outer.m_max.y
		&& _inner.m_max.z <= _outer.m_max.z
		;
}

bool Octree::contains(const frm::AlignedBox& _outer, const frm::vec3& _point) const
{	
	return true
		&& _point.x >= _outer.m_min.x
		&& _point.y >= _outer.m_min.y
		&& _point.z >= _outer.m_min.z
		&& _point.x <= _outer.m_max.x
		&& _point.y <= _outer.m_max.y
		&& _point.z <= _outer.m_max.z
		;
}

void Octree::split(Node* _node)
{
	PROFILER_MARKER_CPU("Octree::split");
	
	APT_ASSERT(_node);
	if (!_node->m_isLeaf) // node already split 
	{
		return;
	}

	LOG_DBG("split(%d -- %f,%f,%f)", _node->m_level, _node->m_centerO.x, _node->m_centerO.y, _node->m_centerO.z);

	APT_ASSERT(_node->m_level < m_maxLevel);

	for (Node*& child : _node->m_children)
	{
		child           = m_nodePool.alloc(Node());
		child->m_parent = _node;
		child->m_level  = _node->m_level + 1;
		child->m_widthO = _node->m_widthO / 2.0f;
	}

	float childOffset = _node->m_widthO / 4.0f;
	_node->m_children[0]->m_centerO = _node->m_centerO + vec3(-childOffset, -childOffset, -childOffset);
	_node->m_children[1]->m_centerO = _node->m_centerO + vec3(-childOffset,  childOffset, -childOffset);
	_node->m_children[2]->m_centerO = _node->m_centerO + vec3( childOffset,  childOffset, -childOffset);
	_node->m_children[3]->m_centerO = _node->m_centerO + vec3( childOffset, -childOffset, -childOffset);
	_node->m_children[4]->m_centerO = _node->m_centerO + vec3(-childOffset, -childOffset,  childOffset);
	_node->m_children[5]->m_centerO = _node->m_centerO + vec3(-childOffset,  childOffset,  childOffset);
	_node->m_children[6]->m_centerO = _node->m_centerO + vec3( childOffset,  childOffset,  childOffset);
	_node->m_children[7]->m_centerO = _node->m_centerO + vec3( childOffset, -childOffset,  childOffset);

	_node->m_isLeaf = false;
	m_totalNodeCount += 8;
	m_updateGPU = true;
}

void Octree::merge(Node* _node)
{
	PROFILER_MARKER_CPU("Octree::merge");

	if (!_node || _node->m_isLeaf)
	{
		return;
	}

	for (Node*& child : _node->m_children)
	{
		merge(child);
		if (child->m_parent)
		{
		 // move data up into the parent
			child->m_parent->m_data.insert(child->m_parent->m_data.end(), child->m_data.begin(), child->m_data.end());
		}
		m_nodePool.free(child);
		child = nullptr;
	}

	_node->m_isLeaf = true;
	m_totalNodeCount -= 8;
	m_updateGPU = true;
}

template<typename OnVisit>
void Octree::traverse(OnVisit&& _onVisit, Node* _rootNode)
{
	eastl::fixed_vector<Node*, 32> tstack;
	tstack.push_back(_rootNode);
	while (!tstack.empty()) 
	{
		auto node = tstack.back();
		APT_ASSERT(node);
		tstack.pop_back();
		if (eastl::forward<OnVisit>(_onVisit)(node) && !node->m_isLeaf) 
		{
			for (Node* child : node->m_children)
			{
				tstack.push_back(child);
			}
		}
	}
}
