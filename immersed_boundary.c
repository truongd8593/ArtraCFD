/****************************************************************************
 *                              ArtraCFD                                    *
 *                          <By Huangrui Mo>                                *
 * Copyright (C) Huangrui Mo <huangrui.mo@gmail.com>                        *
 * This file is part of ArtraCFD.                                           *
 * ArtraCFD is free software: you can redistribute it and/or modify it      *
 * under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation, either version 3 of the License, or        *
 * (at your option) any later version.                                      *
 ****************************************************************************/
/****************************************************************************
 * Required Header Files
 ****************************************************************************/
#include "immersed_boundary.h"
#include <stdio.h> /* standard library for input and output */
#include <math.h> /* common mathematical functions */
#include <float.h> /* size of floating point values */
#include "computational_geometry.h"
#include "cfd_commons.h"
#include "commons.h"
/****************************************************************************
 * Static Function Declarations
 ****************************************************************************/
static void InitializeGeometryDomain(Space *);
static void IdentifyGeometryNode(Space *);
static void IdentifyInterfacialNode(Space *);
/****************************************************************************
 * Function definitions
 ****************************************************************************/
/*
 * This function identify the type of each node: 
 *
 * Procedures are:
 * -- initialize node flag of global boundary and global ghost nodes to -1, 
 *    and interior computational nodes to 0;
 * -- identify nodes that are in or on the nth geometry as n;
 * -- identify interfacial nodes by the node type of neighbours;
 *
 * Interior ghost nodes are computational nodes that locate on numerical 
 * boundaries. They form a subset of interfacial nodes. It's necessary to
 * differentiate exterior nodes and interior nodes to avoid incorrect mark
 * of interfacial nodes near global domain boundaries.
 *
 * The identification process should proceed step by step to correctly handle
 * all these relationships and avoid interference between each step,
 * interference may happen if identification processes are crunched together.
 *
 * Moreover, whenever identifying a node, link the node to geometry, which 
 * can be used to access the geometric data for computing required data. 
 * The rational is that don't store every information for each node, but
 * only store necessary information. When need it, access and calculate it.
 */
void ComputeGeometryDomain(Space *space)
{
    ComputeGeometryParameters(space->part.collapsed, &(space->geo));
    InitializeGeometryDomain(space);
    IdentifyGeometryNode(space);
    IdentifyInterfacialNode(space);
    return;
}
static void InitializeGeometryDomain(Space *space)
{
    Partition *restrict part = &(space->part);
    Node *node = space->node;
    int idx = 0; /* linear array index math variable */
    for (int k = 0; k < part->n[Z]; ++k) {
        for (int j = 0; j < part->n[Y]; ++j) {
            for (int i = 0; i < part->n[X]; ++i) {
                idx = IndexNode(k, j, i, part->n[Y], part->n[X]);
                if ((part->ns[PIN][Z][MIN] <= k) && (part->ns[PIN][Z][MAX] > k) &&
                        (part->ns[PIN][Y][MIN] <= j) && (part->ns[PIN][Y][MAX] > j) &&
                        (part->ns[PIN][X][MIN] <= i) && (part->ns[PIN][X][MAX] > i)) {
                    node[idx].geoID = 0;
                } else {
                    node[idx].geoID = 0;
                }
                node[idx].faceID = 0;
                node[idx].layerID = 0;
            }
        }
    }
    return;
}
/*
 * When locate geometry nodes, there are two approaches available. One is search
 * over each node and verify each node regarding to all the geometries; another
 * is search each geometry and find all the nodes inside current geometry.
 * The second method is adopted here for performance reason.
 *
 * It is efficient to only test points that are inside the bounding box or
 * sphere of a large polyhedron. Be cautious with the validity of any calculated
 * index. It's extremely necessary to adjust the index into the valid region or 
 * check the validity of the index to avoid index exceed array bound limits.
 *
 * For a large data set, an extra data preprocessing with spatial subdivision
 * is required to ensure minimal test in addition to the bounding container
 * method. Spatial subdivision is to provide internal resolution for the
 * polyhedron for fast inclusion determination.
 */
static void IdentifyGeometryNode(Space *space)
{
    const Partition *restrict part = &(space->part);
    Geometry *geo = &(space->geo);
    Node *node = space->node;
    Polyhedron *poly = NULL;
    const IntVec nMin = {part->ns[PIN][X][MIN], part->ns[PIN][Y][MIN], part->ns[PIN][Z][MIN]};
    const IntVec nMax = {part->ns[PIN][X][MAX], part->ns[PIN][Y][MAX], part->ns[PIN][Z][MAX]};
    const RealVec sMin = {part->domain[X][MIN], part->domain[Y][MIN], part->domain[Z][MIN]};
    const RealVec d = {part->d[X], part->d[Y], part->d[Z]};
    const RealVec dd = {part->dd[X], part->dd[Y], part->dd[Z]};
    const int ng = part->ng;
    int idx = 0; /* linear array index math variable */
    int box[DIMS][LIMIT] = {{0}}; /* bounding box in node space */
    RealVec p = {0.0}; /* node point */
    for (int n = 0; n < geo->totalN; ++n) {
        poly = geo->poly + n;
        /* determine search range according to bounding box of polyhedron and valid node space */
        for (int s = 0; s < DIMS; ++s) {
            box[s][MIN] = ValidNodeSpace(NodeSpace(poly->box[s][MIN], sMin[s], dd[s], ng), nMin[s], nMax[s]);
            box[s][MAX] = ValidNodeSpace(NodeSpace(poly->box[s][MAX], sMin[s], dd[s], ng), nMin[s], nMax[s]) + 1;
        }
        /* find nodes in geometry, then flag and link to geometry. */
        for (int k = box[Z][MIN]; k < box[Z][MAX]; ++k) {
            for (int j = box[Y][MIN]; j < box[Y][MAX]; ++j) {
                for (int i = box[X][MIN]; i < box[X][MAX]; ++i) {
                    idx = IndexNode(k, j, i, part->n[Y], part->n[X]);
                    if (0 != node[idx].geoID) { /* already identified by others */
                        continue;
                    }
                    p[X] = PointSpace(i, sMin[X], d[X], ng);
                    p[Y] = PointSpace(j, sMin[Y], d[Y], ng);
                    p[Z] = PointSpace(k, sMin[Z], d[Z], ng);
                    if (geo->sphereN > n) { /* analytical sphere */
                        if (poly->r * poly->r >= Dist2(poly->O, p)) {
                            node[idx].geoID = n + 1;
                        }
                    } else { /* triangulated polyhedron */
                        if (PointInPolyhedron(p, poly, &(node[idx].faceID))) {
                            node[idx].geoID = n + 1;
                        }
                    }
                }
            }
        }
    }
    return;
}
static int IdentifyGhostNodes(Space *space, const Partition *part, const Geometry *geo)
{
    Index idx = 0; /* linear array index math variable */
    /* find solid nodes at numerical boundary, then flag as ghost while preserving link. */
    for (int k = part->n[PIN][Z][MIN]; k < part->n[PIN][Z][MAX]; ++k) {
        for (int j = part->n[PIN][Y][MIN]; j < part->n[PIN][Y][MAX]; ++j) {
            for (int i = part->n[PIN][X][MIN]; i < part->n[PIN][X][MAX]; ++i) {
                idx = IndexNode(k, j, i, space);
                if (SOLID != space->node[idx].type) {
                    continue;
                }
                /* search neighbours of node(k, j, i) to check fluid node */
                for (int r = 1; r < space->ng + 2; ++r) { /* max search range should be ng + 1 */
                    /* if rth neighbours has fluid, then it's rth type ghost node */
                    if (0 == SearchFluidNodes(k, j, i, r, space)) {
                        space->node[idx].type = GHOST;
                        space->node[idx].layerID = r; /* save ghost layer identifier */
                        break; /* exit search loop once identified successfully */
                    }
                }
            }
        }
    }
    return 0;
}
static int SearchFluidNodes(const int k, const int j, const int i, 
        const int h, const Space *space)
{
    /*
     * Search around the specified node and return the information of whether 
     * current node has fluid neighbours at the specified coordinate range.
     */
    /*
     * Convective terms only have first order derivatives, discretization
     * stencils of each computational nodes are cross types.
     * Diffusive tems have second order mixed derivatives, discretization
     * stencils of each computational nodes are plane squares with coner
     * nodes. Therefore, for interfacial computational nodes, neighbouring
     * nodes at corner directions require to be identified as ghost nodes.
     */
    const Index idxW = IndexNode(k, j, i - h, space);
    const Index idxE = IndexNode(k, j, i + h, space);
    const Index idxS = IndexNode(k, j - h, i, space);
    const Index idxN = IndexNode(k, j + h, i, space);
    const Index idxF = IndexNode(k - h, j, i, space);
    const Index idxB = IndexNode(k + h, j, i, space);
    return (space->node[idxW].type * space->node[idxE].type * 
            space->node[idxS].type * space->node[idxN].type * 
            space->node[idxF].type * space->node[idxB].type);
}
void ImmersedBoundaryTreatment(const int tn, Space *space, const Model *model)
{
    Index idx = 0; /* linear array index math variable */
    Ghost gs = {0.0}; /* data collection for flow reconstruction */
    Real weightSum = 0.0; /* store the sum of weights */
    for (int r = 1; r < space->ng + 2; ++r) {
        for (int k = part->n[PIN][Z][MIN]; k < part->n[PIN][Z][MAX]; ++k) {
            for (int j = part->n[PIN][Y][MIN]; j < part->n[PIN][Y][MAX]; ++j) {
                for (int i = part->n[PIN][X][MIN]; i < part->n[PIN][X][MAX]; ++i) {
                    idx = IndexNode(k, j, i, space);
                    if (GHOST != space->node[idx].type) {
                        continue;
                    }
                    if (r != space->node[idx].layerID) { /* not a ghost node in current layer */
                        continue;
                    }
                    if (model->layers >= r) { /* using the method of image */
                        CalculateGeometryInformation(&gs, k, j, i, geo, space);
                        /* obtain the spatial coordinates of the image point */
                        imageX = info[GSX] + 2 * info[GSDS] * info[GSNX];
                        imageY = info[GSY] + 2 * info[GSDS] * info[GSNY];
                        imageZ = info[GSZ] + 2 * info[GSDS] * info[GSNZ];
                        /*
                         * When extremely strong discontinuities exist in the
                         * domain of dependence of inverse distance weighting,
                         * the original weighting approach may produce spurious
                         * discontinuities among the weighted node and its
                         * neighboring nodes. The best way to solve this issue
                         * is to apply WENO's idea to avoid discontinuous
                         * stencils and to only use smooth stencils. However,
                         * this will make the algorithm much more complex.
                         */
                        FlowReconstruction(UoImage, imageZ, imageY, imageX, 
                                ComputeK(imageZ, space), ComputeJ(imageY, space), ComputeI(imageX, space),
                                type, UoBC, info, geo, U, space, model, geometry);
                        /* 
                         * Apply the method of image.
                         *  -- reflecting vectors over wall for both slip and noslip, stationary and
                         *     moving conditions is unified by linear interpolation.
                         *  -- scalars are symmetrically reflected between image and ghost.
                         */
                        UoGhost[1] = 2 * UoBC[1] - UoImage[1];
                        UoGhost[2] = 2 * UoBC[2] - UoImage[2];
                        UoGhost[3] = 2 * UoBC[3] - UoImage[3];
                        UoGhost[4] = UoImage[4];
                        UoGhost[5] = UoImage[5];
                    } else { /* using inverse distance weighting instead of method of image */
                        InverseDistanceWeighting(UoGhost, &weightSum, ComputeZ(k, space), ComputeY(j, space), ComputeX(i, space), 
                                k, j, i, 1, type - 1, U, space, model, geometry);
                        /* Normalize the weighted values */
                        NormalizeWeightedValues(UoGhost, weightSum);
                    }
                    UoGhost[0] = UoGhost[4] / (UoGhost[5] * model->gasR); /* compute density */
                    if (!((0 < UoGhost[0]) && (FLT_MAX > UoGhost[0]))) {
                        fprintf(stderr, "rho=%.6g\n", UoGhost[0]);
                        FatalError("illegal density reconstructed, solution diverges...");
                    }
                    ConservativeByPrimitive(U, idx * DIMU, UoGhost, model);
                }
            }
        }
    }
    return;
}
int FlowReconstruction(Real Uo[], const Real z, const Real y, const Real x,
        const int k, const int j, const int i, const int h, 
        Real UoBC[], const Real info[], const Real *geo, const Real *U,
        const Space *space, const Model *model, const Geometry *geometry)
{
    Real weightSum = 0.0; /* store the sum of weights */
    /* pre-estimate step */
    InverseDistanceWeighting(Uo, &weightSum, z, y, x, k, j, i, h, FLUID, U, space, model, geometry);
    /* physical boundary condition enforcement step */
    if (0 < geo[GROUGH]) { /* noslip wall */
        UoBC[1] = geo[GU];
        UoBC[2] = geo[GV];
        UoBC[3] = geo[GW];
    } else { /* slip wall */
        Real nVec[DIMS] = {0.0}; /* normal vector */
        Real taVec[DIMS] = {0.0}; /* tangential vector */
        Real tbVec[DIMS] = {0.0}; /* tangential vector */
        Real rhs[DIMS] = {0.0}; /* right hand side vector */
        OrthogonalSpace(nVec, taVec, tbVec, info);
        rhs[X] = geo[GU] * nVec[X] + geo[GV] * nVec[Y] + geo[GW] * nVec[Z];
        rhs[Y] = (Uo[1] * taVec[X] + Uo[2] * taVec[Y] + Uo[3] * taVec[Z]) / weightSum;
        rhs[Z] = (Uo[1] * tbVec[X] + Uo[2] * tbVec[Y] + Uo[3] * tbVec[Z]) / weightSum;
        UoBC[1] = nVec[X] * rhs[X] + taVec[X] * rhs[Y] + tbVec[X] * rhs[Z];
        UoBC[2] = nVec[Y] * rhs[X] + taVec[Y] * rhs[Y] + tbVec[Y] * rhs[Z];
        UoBC[3] = nVec[Z] * rhs[X] + taVec[Z] * rhs[Y] + tbVec[Z] * rhs[Z];
    }
    UoBC[4] = Uo[4] / weightSum; /* zero gradient pressure */
    if (0 > geo[GT]) { /* adiabatic, dT/dn = 0 */
        UoBC[5] = Uo[5] / weightSum;
    } else { /* otherwise, use specified constant wall temperature, T = Tw */
        UoBC[5] = geo[GT];
    }
    UoBC[0] = UoBC[4] / (UoBC[5] * model->gasR); /* compute density */
    /* correction step by adding the boundary point as a stencil */
    ApplyWeighting(Uo, &weightSum, info[GSDS] * info[GSDS], UoBC, space->tinyL);
    /* Normalize the weighted values */
    NormalizeWeightedValues(Uo, weightSum);
    return 0;
}
static int InverseDistanceWeighting(Real Uo[], Real *weightSum, const Real z, const Real y, const Real x,
        const int k, const int j, const int i, const int h, const int nodeType, const Real *U,
        const Space *space, const Model *model, const Geometry *geometry)
{
    int idxh = 0; /* linear array index math variable */
    int tally = 0; /* stencil count and zero stencil detector */
    int geoID = 0;
    Real Uoh[DIMUo] = {0.0}; /* primitive at target node */
    Real distZ = 0.0;
    Real distY = 0.0;
    Real distX = 0.0;
    Real distance = 0.0;
    for (int dim = 0; dim < DIMUo; ++dim) {
        Uo[dim] = 0.0; /* reset */
    }
    *weightSum = 0.0; /* reset */
    /* 
     * Search around the specified node to find required target nodes as interpolation stencil.
     */
    for (int kh = -h; kh <= h; ++kh) {
        for (int jh = -h; jh <= h; ++jh) {
            for (int ih = -h; ih <= h; ++ih) {
                idxh = IndexMath(k + kh, j + jh, i + ih, space);
                if ((0 > idxh) || (space->nMax <= idxh)) {
                    continue; /* illegal index */
                }
                if (FLUID == nodeType) { /* require fluid nodes */
                    if (FLUID != space->nodeFlag[idxh]) {
                        continue;
                    }
                } else { /* require specified ghost node type */
                    geoID = space->nodeFlag[idxh] - OFFSET - (nodeType - 1) * geometry->totalN;
                    if ((0 > geoID) || (geometry->totalN <= geoID)) { /* not a ghost node with current type */
                        continue;
                    }
                }
                ++tally;
                distZ = ComputeZ(k + kh, space) - z;
                distY = ComputeY(j + jh, space) - y;
                distX = ComputeX(i + ih, space) - x;
                /* use distance square to avoid expensive sqrt */
                distance = distX * distX + distY * distY + distZ * distZ;
                PrimitiveByConservative(Uoh, idxh * DIMU, U, model);
                ApplyWeighting(Uo, weightSum, distance, Uoh, space->tinyL);
            }
        }
    }
    if (0 == tally) {
        fprintf(stderr, "k=%d, j=%d, i=%d\n", k, j, i);
        FatalError("zero stencil encountered...");
    }
    return 0;
}
static int ApplyWeighting(Real Uo[], Real *weightSum, Real distance, const Real Uoh[], const Real tiny)
{
    if (tiny > distance) { /* avoid overflow of too small distance */
        distance = tiny;
    }
    distance = 1 / distance; /* compute weight */
    for (int n = 0; n < DIMUo; ++n) {
        Uo[n] = Uo[n] + Uoh[n] * distance;
    }
    *weightSum = *weightSum + distance; /* accumulate normalizer */
    return 0;
}
/* a good practice: end file with a newline */

