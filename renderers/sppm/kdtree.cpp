/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include "hitpoints.h"
#include "lookupaccel.h"
#include "bxdf.h"

using namespace lux;

KdTree::KdTree(HitPoints *hps) {
	hitPoints = hps;
	maxNNodes = hitPoints->GetSize();

	nextFreeNode = 1;
	nodes = NULL;
	nodeData = NULL;
	
	nodes = new KdNode[maxNNodes];
	nodeData = new HitPoint*[maxNNodes];

	RefreshMutex(0);
}

KdTree::~KdTree() {
	delete[] nodes;
	delete[] nodeData;
}

bool KdTree::CompareNode::operator ()(const HitPoint *d1, const HitPoint *d2) const {
	return (d1->eyePass[passIndex].position[axis] == d2->eyePass[passIndex].position[axis]) ? (d1 < d2) :
			(d1->eyePass[passIndex].position[axis] < d2->eyePass[passIndex].position[axis]);
}

void KdTree::RecursiveBuild(const u_int passIndex,
		const unsigned int nodeNum, const unsigned int start,
		const unsigned int end, std::vector<HitPoint *> &buildNodes) {
	assert (nodeNum >= 0);
	assert (start >= 0);
	assert (end >= 0);
	assert (nodeNum < nNodes);
	assert (start < nNodes);
	assert (end <= nNodes);

	// Create leaf node of kd-tree if we've reached the bottom
	if (start + 1 == end) {
		nodes[nodeNum].initLeaf();
		nodeData[nodeNum] = buildNodes[start];
		return;
	}

	// Choose split direction and partition data
	// Compute bounds of data from start to end
	BBox bound;
	for (unsigned int i = start; i < end; ++i)
		bound = Union(bound, buildNodes[i]->eyePass[passIndex].position);
	unsigned int splitAxis = bound.MaximumExtent();
	unsigned int splitPos = (start + end) / 2;

	std::nth_element(buildNodes.begin() + start, buildNodes.begin() + splitPos,
		buildNodes.begin() + end, CompareNode(splitAxis, passIndex));

	// Allocate kd-tree node and continue recursively
	nodes[nodeNum].init(buildNodes[splitPos]->eyePass[passIndex].position[splitAxis], splitAxis);
	nodeData[nodeNum] = buildNodes[splitPos];

	if (start < splitPos) {
		nodes[nodeNum].hasLeftChild = 1;
		const unsigned int childNum = nextFreeNode++;
		RecursiveBuild(passIndex, childNum, start, splitPos, buildNodes);
	}

	if (splitPos + 1 < end) {
		nodes[nodeNum].rightChild = nextFreeNode++;
		RecursiveBuild(passIndex, nodes[nodeNum].rightChild, splitPos + 1, end, buildNodes);
	}
}

void KdTree::RefreshMutex(const u_int passIndex) {

	nextFreeNode = 1;

	// Begin the KdTree building process
	std::vector<HitPoint *> buildNodes;
	buildNodes.reserve(maxNNodes);
	maxDistSquared = 0.f;
	for (unsigned int i = 0; i < maxNNodes; ++i)  {
		HitPoint * const hp = hitPoints->GetHitPoint(i);
		if(hp->eyePass[passIndex].type == SURFACE)
		{
			buildNodes.push_back(hp);
			maxDistSquared = max<float>(maxDistSquared, hp->accumPhotonRadius2);
		}
	}
	nNodes = buildNodes.size();

	LOG(LUX_DEBUG, LUX_NOERROR) << "Building kD-Tree with " << nNodes << " nodes";
	LOG(LUX_DEBUG, LUX_NOERROR) << "kD-Tree search radius: " << sqrtf(maxDistSquared);

	RecursiveBuild(passIndex, 0, 0, nNodes, buildNodes);
	assert (nNodes == nextFreeNode);
}

void KdTree::AddFlux(const Point &p, const u_int passIndex, const Vector &wi,
		const SpectrumWavelengths &sw, const SWCSpectrum &photonFlux, const u_int lightGroup) {
	unsigned int nodeNumStack[64];
	// Start from the first node
	nodeNumStack[0] = 0;
	int stackIndex = 0;

	while (stackIndex >= 0) {
		const unsigned int nodeNum = nodeNumStack[stackIndex--];
		KdNode *node = &nodes[nodeNum];

		const int axis = node->splitAxis;
		if (axis != 3) {
			const float dist = p[axis] - node->splitPos;
			const float dist2 = dist * dist;
			if (p[axis] <= node->splitPos) {
				if ((dist2 < maxDistSquared) && (node->rightChild < nNodes))
					nodeNumStack[++stackIndex] = node->rightChild;
				if (node->hasLeftChild)
					nodeNumStack[++stackIndex] = nodeNum + 1;
			} else {
				if (node->rightChild < nNodes)
					nodeNumStack[++stackIndex] = node->rightChild;
				if ((dist2 < maxDistSquared) && (node->hasLeftChild))
					nodeNumStack[++stackIndex] = nodeNum + 1;
			}
		}

		// Process the leaf
		HitPoint *hp = nodeData[nodeNum];
		AddFluxToHitPoint(hp, passIndex, p, wi, sw, photonFlux, lightGroup);
	}
}

void KdTree::AddFlux(SplatList *splatList, const Point &p, const u_int passIndex, const Vector &wi,
		const SpectrumWavelengths &sw, const SWCSpectrum &photonFlux, const u_int lightGroup) {
	unsigned int nodeNumStack[64];
	// Start from the first node
	nodeNumStack[0] = 0;
	int stackIndex = 0;

	while (stackIndex >= 0) {
		const unsigned int nodeNum = nodeNumStack[stackIndex--];
		KdNode *node = &nodes[nodeNum];

		const int axis = node->splitAxis;
		if (axis != 3) {
			const float dist = p[axis] - node->splitPos;
			const float dist2 = dist * dist;
			if (p[axis] <= node->splitPos) {
				if ((dist2 < maxDistSquared) && (node->rightChild < nNodes))
					nodeNumStack[++stackIndex] = node->rightChild;
				if (node->hasLeftChild)
					nodeNumStack[++stackIndex] = nodeNum + 1;
			} else {
				if (node->rightChild < nNodes)
					nodeNumStack[++stackIndex] = node->rightChild;
				if ((dist2 < maxDistSquared) && (node->hasLeftChild))
					nodeNumStack[++stackIndex] = nodeNum + 1;
			}
		}

		// Process the leaf
		HitPoint *hp = nodeData[nodeNum];
		AddFluxToSplatList(splatList, hp, passIndex, p, wi, sw, photonFlux, lightGroup);
	}
}