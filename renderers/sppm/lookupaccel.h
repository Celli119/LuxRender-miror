/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#ifndef LUX_LOOKUPACCEL_H
#define	LUX_LOOKUPACCEL_H

#include <vector>

#include "osfunc.h"
#include "scheduler.h"

namespace lux
{

//------------------------------------------------------------------------------
// Hit points look up accelerators
//------------------------------------------------------------------------------

class HitPoint;
class HitPoints;
class HashCell;
class PhotonData;

enum LookUpAccelType {
	HASH_GRID, KD_TREE, HYBRID_HASH_GRID, PARALLEL_HASH_GRID
};

class HitPointsLookUpAccel {
public:
	HitPointsLookUpAccel(HitPoints *hps): hitPoints(hps) { }
	virtual ~HitPointsLookUpAccel() { }

	virtual void Refresh(scheduling::Scheduler *scheduler) = 0;

	virtual void AddFlux(Sample &sample, const PhotonData &photon) = 0;

	friend class HashCell;

protected:
	void AddFluxToHitPoint(Sample &sample, HitPoint *hp, const PhotonData &photon);

	HitPoints *hitPoints;
};


inline void SpectrumAtomicAdd(SWCSpectrum &s, const SWCSpectrum &a) {
	for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
		osAtomicAdd(&s.c[i], a.c[i]);
}

inline void XYZColorAtomicAdd(XYZColor &s, const XYZColor &a) {
	for (int i = 0; i < COLOR_SAMPLES; ++i)
		osAtomicAdd(&s.c[i], a.c[i]);
}

//------------------------------------------------------------------------------
// HashGrid accelerator
//------------------------------------------------------------------------------

class HashGrid : public HitPointsLookUpAccel {
public:
	HashGrid(HitPoints *hps);

	~HashGrid();

	void Refresh(scheduling::Scheduler *scheduler);

	virtual void AddFlux(Sample &sample, const PhotonData &photon);

private:
	void RefreshMutex();

	u_int Hash(const int ix, const int iy, const int iz) {
		return (u_int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % gridSize;
	}
	/*u_int Hash(const int ix, const int iy, const int iz) {
		return (u_int)((ix * 997 + iy) * 443 + iz) % gridSize;
	}*/

	u_int gridSize;
	float invCellSize;
	std::list<HitPoint *> **grid;
};

//------------------------------------------------------------------------------
// Parallel Hash Grid accelerator
//------------------------------------------------------------------------------

class ParallelHashGrid : public HitPointsLookUpAccel {
public:
	ParallelHashGrid(HitPoints *hps, float const gridCoef);

	~ParallelHashGrid();

	void Refresh(scheduling::Scheduler *scheduler);

	virtual void AddFlux(Sample &sample, const PhotonData &photon);

private:
	void ResetGrid(scheduling::Range *range, unsigned *data);
	void Fill(scheduling::Range *range);

	u_int Hash(const int ix, const int iy, const int iz) {
		return (u_int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % gridSize;
	}
	void JumpInsert(unsigned int hv, unsigned int i);

	float invCellSize;
	unsigned int *grid;
	unsigned int *jump_list;
	unsigned int gridSize, jumpSize;
};

//------------------------------------------------------------------------------
// KdTree accelerator
//------------------------------------------------------------------------------

class KdTree : public HitPointsLookUpAccel {
public:
	KdTree(HitPoints *hps);

	~KdTree();

	void Refresh(scheduling::Scheduler *scheduler);

	virtual void AddFlux(Sample &sample, const PhotonData &photon);

private:
	void RefreshMutex();

	struct KdNode {
		void init(const float p, const u_int a) {
			splitPos = p;
			splitAxis = a;
			// Dade - in order to avoid a gcc warning
			rightChild = 0;
			rightChild = ~rightChild;
			hasLeftChild = 0;
		}

		void initLeaf() {
			splitAxis = 3;
			// Dade - in order to avoid a gcc warning
			rightChild = 0;
			rightChild = ~rightChild;
			hasLeftChild = 0;
		}

		// KdNode Data
		float splitPos;
		u_int splitAxis : 2;
		u_int hasLeftChild : 1;
		u_int rightChild : 29;
	};

	struct CompareNode {
		CompareNode(int a) { axis = a;}

		int axis;

		bool operator()(const HitPoint *d1, const HitPoint *d2) const;
	};

	void RecursiveBuild(
		const u_int nodeNum, const u_int start,
		const u_int end, std::vector<HitPoint *> &buildNodes);

	KdNode *nodes;
	HitPoint **nodeData;
	u_int nNodes, nextFreeNode, maxNNodes;
	float maxDistSquared;
};

//------------------------------------------------------------------------------
// HashCell definition
//------------------------------------------------------------------------------

enum HashCellType {
	HH_LIST, HH_KD_TREE
};

class HashCell {
public:
	HashCell(const HashCellType t) {
		type = HH_LIST;
		size = 0;
		list = new std::list<HitPoint *>();
	}
	~HashCell() {
		switch (type) {
			case HH_LIST:
				delete list;
				break;
			case HH_KD_TREE:
				delete kdtree;
				break;
			default:
				assert (false);
		}
	}

	void AddList(HitPoint *hp) {
		assert (type == HH_LIST);

		/* Too slow:
		// Check if the hit point has been already inserted
		std::list<HitPoint *>::iterator iter = list->begin();
		while (iter != list->end()) {
			HitPoint *lhp = *iter++;

			if (lhp == hp)
				return;
		}*/

		list->push_front(hp);
		++size;
	}

	void TransformToKdTree();

	void AddFlux(Sample &sample, HitPointsLookUpAccel *accel, const PhotonData &photon);

	u_int GetSize() const { return size; }

private:
	class HCKdTree {
	public:
		HCKdTree( std::list<HitPoint *> *hps, const u_int count);
		~HCKdTree();

	void AddFlux(Sample &sample, HitPointsLookUpAccel *accel, const PhotonData &photon);

	private:
		struct KdNode {
			void init(const float p, const u_int a) {
				splitPos = p;
				splitAxis = a;
				// Dade - in order to avoid a gcc warning
				rightChild = 0;
				rightChild = ~rightChild;
				hasLeftChild = 0;
			}

			void initLeaf() {
				splitAxis = 3;
				// Dade - in order to avoid a gcc warning
				rightChild = 0;
				rightChild = ~rightChild;
				hasLeftChild = 0;
			}

			// KdNode Data
			float splitPos;
			u_int splitAxis : 2;
			u_int hasLeftChild : 1;
			u_int rightChild : 29;
		};

		struct CompareNode {
			CompareNode(int a) { axis = a;}

			int axis;

			bool operator()(const HitPoint *d1, const HitPoint *d2) const;
		};

		void RecursiveBuild(
				const u_int nodeNum, const u_int start,
				const u_int end, std::vector<HitPoint *> &buildNodes);

		KdNode *nodes;
		HitPoint **nodeData;
		u_int nNodes, nextFreeNode;
		float maxDistSquared;
	};

	HashCellType type;
	u_int size;
	union {
		std::list<HitPoint *> *list;
		HCKdTree *kdtree;
	};
};

//------------------------------------------------------------------------------
// HybridHashGrid accelerator
//------------------------------------------------------------------------------

class HybridHashGrid : public HitPointsLookUpAccel {
public:
	HybridHashGrid(HitPoints *hps);

	~HybridHashGrid();

	void Refresh(scheduling::Scheduler *scheduler);

	virtual void AddFlux(Sample &sample, const PhotonData &photon);

private:
	void ConvertKdTree(scheduling::Range *range);
	void RefreshMutex();

	u_int Hash(const int ix, const int iy, const int iz) {
		return (u_int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % gridSize;
	}

	u_int kdtreeThreshold;
	u_int gridSize;
	float invCellSize;
	int maxHashIndexX, maxHashIndexY, maxHashIndexZ;
	HashCell **grid;
};

}//namespace lux

#endif	/* LUX_LOOKUPACCEL_H */
