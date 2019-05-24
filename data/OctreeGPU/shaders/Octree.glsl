layout(std430) restrict readonly buffer bfNodeHierarchy
{
	uint uNodeHierarchy[];
};

layout(std430) restrict readonly buffer bfNodeCenterWidth
{
	vec4 uNodeCenterWidth[];
};

layout(std430) restrict readonly buffer bfNodeData
{
	vec4 uNodeData[];
};

layout(std430) restrict readonly buffer bfNodeDataCount
{
	uint uNodeDataCount[];
};

#define Octree_INVALID_NODE_INDEX          0xffffffff

#define Octree_NODE_LEVEL_BITS             4
#define Octree_NODE_CHILD_COUNT_BITS       4
#define Octree_NODE_CHILD_INDEX_BITS       12
#define Octree_NODE_DATA_INDEX_BITS        12

#define Octree_NODE_LEVEL_START_BIT        (0)
#define Octree_NODE_CHILD_COUNT_START_BIT  (Octree_NODE_LEVEL_BITS)
#define Octree_NODE_CHILD_INDEX_START_BIT  (Octree_NODE_LEVEL_BITS + Octree_NODE_CHILD_COUNT_BITS)
#define Octree_NODE_DATA_INDEX_START_BIT   (Octree_NODE_LEVEL_BITS + Octree_NODE_CHILD_COUNT_BITS + Octree_NODE_CHILD_INDEX_BITS)

uint Octree_GetNodeLevel(in uint _nodeIndex)
{
	return bitfieldExtract(uNodeHierarchy[_nodeIndex], Octree_NODE_LEVEL_START_BIT, Octree_NODE_LEVEL_BITS);
}

uint Octree_GetNodeChildCount(in uint _nodeIndex)
{
	return bitfieldExtract(uNodeHierarchy[_nodeIndex], Octree_NODE_CHILD_COUNT_START_BIT, Octree_NODE_CHILD_COUNT_BITS);
}

uint Octree_GetNodeChildIndex(in uint _nodeIndex)
{
	return bitfieldExtract(uNodeHierarchy[_nodeIndex], Octree_NODE_CHILD_INDEX_START_BIT, Octree_NODE_CHILD_INDEX_BITS);
}

uint Octree_GetNodeDataIndex(in uint _nodeIndex)
{
	return bitfieldExtract(uNodeHierarchy[_nodeIndex], Octree_NODE_DATA_INDEX_START_BIT, Octree_NODE_DATA_INDEX_BITS);
}

vec4 Octree_GetNodeCenterWidth(in uint _nodeIndex)
{
	return uNodeCenterWidth[_nodeIndex];
}

uint Octree_GetNodeDataCount(in uint _nodeIndex)
{
	return uNodeDataCount[_nodeIndex];
}

vec4 Octree_GetNodeData(in uint _dataIndex, in uint _offset)
{
	return uNodeData[_dataIndex + _offset];
}

bool Octree_Contains(in uint _nodeIndex, in vec3 _p)
{
	vec4 nodeCenterWidth = Octree_GetNodeCenterWidth(_nodeIndex);
	vec3 nodeMin = nodeCenterWidth.xyz - vec3(nodeCenterWidth.w) * 0.5;
	vec3 nodeMax = nodeCenterWidth.xyz + vec3(nodeCenterWidth.w) * 0.5;
	return all(greaterThanEqual(_p, nodeMin)) && all(lessThanEqual(_p, nodeMax));
	//return true
	//	&& _p.x >= nodeMin.x
	//	&& _p.y >= nodeMin.y
	//	&& _p.z >= nodeMin.z
	//	&& _p.x <= nodeMax.x
	//	&& _p.y <= nodeMax.y
	//	&& _p.z <= nodeMax.z
	//	;
}

float Octree_EdgeDistance(in uint _nodeIndex, in vec3 _p)
{
	vec4 nodeCenterWidth = Octree_GetNodeCenterWidth(_nodeIndex);
	vec3 edgeDistance = abs(_p - nodeCenterWidth.xyz) / (nodeCenterWidth.w * 0.5);
	return 1.0 - max3(edgeDistance.x, edgeDistance.y, edgeDistance.z);
}

uint Octree_FindLeaf(in vec3 _p)
{
	uint tstack[32];
	int tstackTop = 0;
	tstack[0] = 0;
	while (tstackTop != -1)
	{
		uint nodeIndex = tstack[tstackTop];
		--tstackTop;

		if (Octree_Contains(nodeIndex, _p))
		{
		 // if this is a leaf node, stop
			uint childCount = Octree_GetNodeChildCount(nodeIndex);
			if (childCount == 0)
			{
				return nodeIndex;
			}

		 // else push children onto stack
			uint childIndex = Octree_GetNodeChildIndex(nodeIndex);
			for (uint i = 0; i < childCount; ++i)
			{
				tstack[++tstackTop] = childIndex + i;
			}
		}
	}
	return Octree_INVALID_NODE_INDEX;
}
