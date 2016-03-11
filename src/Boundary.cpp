/*
  Copyright (c) 2015-2016 Lester Hedges <lester.hedges+lsm@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Boundary.h"

/*! \file Boundary.cpp
    \brief A class for the discretised boundary.
 */

namespace lsm
{
    Boundary::Boundary(Mesh& mesh_, LevelSet& levelSet_) : mesh(mesh_), levelSet(levelSet_)
    {
    }

    void Boundary::discretise(bool isTarget)
    {
        // Allocate memory for boundary points and segments.
        // 20% of node count is a reasonable estimate.
        // Will need to check that this limit isn't exceeded.

        // Make sure that memory is sufficient (for small test systems).
        int size = 0.2*mesh.nNodes;
        size = std::max(4, size);

        // Resize vectors.
        points.resize(size);
        segments.resize(size);

        // Reset the number of points and segments.
        nPoints = nSegments = 0;

        // Zero the boundary length.
        length = 0;

        // Initialise a pointer to the signed distance vector.
        std::vector<double>* signedDistance;

        // Point to the target signed distance.
        if (isTarget) signedDistance = &levelSet.target;

        // Point to the current signed distance function.
        else signedDistance = &levelSet.signedDistance;

        // Compute the status of nodes and elements in the finite element mesh.
        computeMeshStatus(signedDistance);

        // Loop over all elements.
        for (unsigned int i=0;i<mesh.nElements;i++)
        {
            // The element isn't outside of the structure.
            if (mesh.elements[i].status != ElementStatus::OUTSIDE)
            {
                // Number of cut edges.
                unsigned int nCut = 0;

                // Existing boundary points associated with a node.
                unsigned int boundaryPoints[4];

                // Look at each edge of the node to determine whether it is cut,
                // or if it's part of the boundary.
                for (unsigned int j=0;j<4;j++)
                {
                    // Index of first node on edge.
                    unsigned int n1 = mesh.elements[i].nodes[j];

                    // Index of second node (reconnecting to 0th node).
                    // Edge connectivity goes: 0 --> 1, 1 --> 2, 2 --> 3, 3 --> 0
                    unsigned int n2 = (j == 3) ? 0 : (j + 1);

                    // Convert to node index.
                    n2 = mesh.elements[i].nodes[n2];

                    // Check that both nodes are inside the narrow band region (only for non-target discretisation).
                    if (isTarget || (mesh.nodes[n1].isActive && mesh.nodes[n2].isActive))
                    {
                        // One node is inside, the other is outside. The edge is cut.
                        if ((mesh.nodes[n1].status|mesh.nodes[n2].status) == NodeStatus::CUT)
                        {
                            // Compute the distance from node 1 to the intersection point (by interpolation).
                            double d = (*signedDistance)[n1]
                                     / ((*signedDistance)[n1] - (*signedDistance)[n2]);

                            // Initialise boundary point coordinate.
                            Coord coord;

                            // Make sure that the boundary point hasn't already been added.
                            int index = isAdded(coord, n1, j, d);

                            // Boundary point is new.
                            if (index < 0)
                            {
                                mesh.nodes[n1].boundaryPoints[mesh.nodes[n1].nBoundaryPoints] = nPoints;
                                mesh.nodes[n2].boundaryPoints[mesh.nodes[n2].nBoundaryPoints] = nPoints;
                                mesh.nodes[n1].nBoundaryPoints++;
                                mesh.nodes[n2].nBoundaryPoints++;

                                // Store boundary point for cut edge.
                                boundaryPoints[nCut] = nPoints;

                                // Initialise boundary point.
                                initialisePoint(points[nPoints], coord);

                                // Increment number of boundary points.
                                nPoints++;
                            }
                            else
                            {
                                // Store existing boundary point.
                                boundaryPoints[nCut] = index;
                            }

                            // Increment number of cut edges.
                            nCut++;
                        }

                        // Both nodes lie on the boundary.
                        else if ((mesh.nodes[n1].status & NodeStatus::BOUNDARY) &&
                            (mesh.nodes[n2].status & NodeStatus::BOUNDARY))
                        {
                            // Initialise boundary point coordinate.
                            Coord coord;

                            // Create boundary segment.
                            BoundarySegment segment;

                            // Assign element index.
                            segment.element = i;

                            // Make sure that the start boundary point hasn't already been added.
                            int index = isAdded(coord, n1, 0, 0);

                            // Boundary point is new.
                            if (index < 0)
                            {
                                // Set index equal to current number of points.
                                index = nPoints;

                                mesh.nodes[n1].boundaryPoints[mesh.nodes[n1].nBoundaryPoints] = nPoints;
                                mesh.nodes[n1].nBoundaryPoints++;

                                // Initialise boundary point.
                                initialisePoint(points[nPoints], coord);

                                // Increment number of boundary points.
                                nPoints++;
                            }

                            // Assign start point index.
                            segment.start = index;

                            // Make sure that the end boundary point hasn't already been added.
                            index = isAdded(coord, n2, 0, 0);

                            // Boundary point is new.
                            if (index < 0)
                            {
                                // Set index equal to current number of points.
                                index = nPoints;

                                mesh.nodes[n2].boundaryPoints[mesh.nodes[n2].nBoundaryPoints] = nPoints;
                                mesh.nodes[n2].nBoundaryPoints++;

                                // Initialise boundary point.
                                initialisePoint(points[nPoints], coord);

                                // Increment number of boundary points.
                                nPoints++;
                            }

                            // Assign end point index.
                            segment.end = index;

                            // Compute the length of the boundary segment.
                            segment.length = segmentLength(segment);

                            // Update total boundary length.
                            length += segment.length;

                            // Create element to segment lookup.
                            mesh.elements[i].boundarySegments[mesh.elements[i].nBoundarySegments] = nSegments;
                            mesh.elements[i].nBoundarySegments++;

                            // Add segment to vector.
                            segments[nSegments] = segment;
                            nSegments++;
                        }
                    }
                }

                // If the element was cut, determine the boundary segment(s).

                // If two edges are cut, then a boundary segment must cross both.
                if (nCut == 2)
                {
                    // Create boundary segment.
                    BoundarySegment segment;
                    segment.start = boundaryPoints[0];
                    segment.end = boundaryPoints[1];
                    segment.element = i;

                    // Compute the length of the boundary segment.
                    segment.length = segmentLength(segment);

                    // Update total boundary length.
                    length += segment.length;

                    // Create element to segment lookup.
                    mesh.elements[i].boundarySegments[mesh.elements[i].nBoundarySegments] = nSegments;
                    mesh.elements[i].nBoundarySegments++;

                    // Add segment to vector.
                    segments[nSegments] = segment;
                    nSegments++;
                }

                // If there is only one cut edge, then the boundary must also cross an element node.
                else if (nCut == 1)
                {
                    // Find a node that is on the boundary and has a neighbour that is outside.
                    for (unsigned int j=0;j<4;j++)
                    {
                        // Node index.
                        unsigned int node = mesh.elements[i].nodes[j];

                        // Node is on the boundary, check its neighbours.
                        if (mesh.nodes[node].status & NodeStatus::BOUNDARY)
                        {
                            // Index of next node.
                            unsigned int nAfter = (j == 3) ? 0 : (j + 1);

                            // Index of previous node.
                            unsigned int nBefore = (j == 0) ? 3 : (j - 1);

                            // Convert to node indices.
                            nAfter = mesh.elements[i].nodes[nAfter];
                            nBefore = mesh.elements[i].nodes[nBefore];

                            // If a neighbour is outside the boundary, then add a boundary segment.
                            if ((mesh.nodes[nAfter].status & NodeStatus::OUTSIDE) ||
                                (mesh.nodes[nBefore].status & NodeStatus::OUTSIDE))
                            {
                                // Create boundary segment.
                                BoundarySegment segment;
                                segment.start = boundaryPoints[0];
                                segment.element = i;

                                // Initialise boundary point coordinate.
                                Coord coord;

                                // Make sure that the end boundary point hasn't already been added.
                                int index = isAdded(coord, node, 0, 0);

                                // Boundary point is new.
                                if (index < 0)
                                {
                                    // Set index equal to current number of points.
                                    index = nPoints;

                                    mesh.nodes[node].boundaryPoints[mesh.nodes[node].nBoundaryPoints] = nPoints;
                                    mesh.nodes[node].nBoundaryPoints++;

                                    // Initialise boundary point.
                                    initialisePoint(points[nPoints], coord);

                                    // Increment number of boundary points.
                                    nPoints++;
                                }

                                // Assign end point index.
                                segment.end = index;

                                // Compute the length of the boundary segment.
                                segment.length = segmentLength(segment);

                                // Update total boundary length.
                                length += segment.length;

                                // Create element to segment lookup.
                                mesh.elements[i].boundarySegments[mesh.elements[i].nBoundarySegments] = nSegments;
                                mesh.elements[i].nBoundarySegments++;

                                // Add segment to vector.
                                segments[nSegments] = segment;
                                nSegments++;
                            }
                        }
                    }
                }

                // If there are four cut edges, then determine which
                // boundary node pairs form the boundary.
                else if (nCut == 4)
                {
                    double lsfSum = 0;

                    // Evaluate level set value at element centre.
                    for (unsigned int j=0;j<4;j++)
                    {
                        // Node index.
                        unsigned int node = mesh.elements[i].nodes[j];

                        lsfSum += (*signedDistance)[node];
                    }

                    // Create boundary segment.
                    BoundarySegment segment;

                    // Store the status of the first node.
                    unsigned int node = mesh.elements[i].nodes[0];
                    NodeStatus::NodeStatus status = mesh.nodes[node].status;

                    if (((status & NodeStatus::INSIDE) && (lsfSum > 0)) ||
                        ((status & NodeStatus::OUTSIDE) && (lsfSum < 0)))
                    {
                        segment.start = boundaryPoints[0];
                        segment.end = boundaryPoints[1];
                        segment.element = i;

                        // Compute the length of the boundary segment.
                        segment.length = segmentLength(segment);

                        // Update total boundary length.
                        length += segment.length;

                        // Create element to segment lookup.
                        mesh.elements[i].boundarySegments[mesh.elements[i].nBoundarySegments] = nSegments;
                        mesh.elements[i].nBoundarySegments++;

                        // Add segment to vector.
                        segments[nSegments] = segment;
                        nSegments++;

                        segment.start = boundaryPoints[2];
                        segment.end = boundaryPoints[3];
                        segment.element = i;

                        // Compute the length of the boundary segment.
                        segment.length = segmentLength(segment);

                        // Update total boundary length.
                        length += segment.length;

                        // Create element to segment lookup.
                        mesh.elements[i].boundarySegments[mesh.elements[i].nBoundarySegments] = nSegments;
                        mesh.elements[i].nBoundarySegments++;

                        // Add segment to vector.
                        segments[nSegments] = segment;
                        nSegments++;
                    }

                    else
                    {
                        segment.start = boundaryPoints[0];
                        segment.end = boundaryPoints[3];
                        segment.element = i;

                        // Compute the length of the boundary segment.
                        segment.length = segmentLength(segment);

                        // Update total boundary length.
                        length += segment.length;

                        // Create element to segment lookup.
                        mesh.elements[i].boundarySegments[mesh.elements[i].nBoundarySegments] = nSegments;
                        mesh.elements[i].nBoundarySegments++;

                        // Add segment to vector.
                        segments[nSegments] = segment;
                        nSegments++;

                        segment.start = boundaryPoints[1];
                        segment.end = boundaryPoints[2];
                        segment.element = i;

                        // Compute the length of the boundary segment.
                        segment.length = segmentLength(segment);

                        // Update total boundary length.
                        length += segment.length;

                        // Create element to segment lookup.
                        mesh.elements[i].boundarySegments[mesh.elements[i].nBoundarySegments] = nSegments;
                        mesh.elements[i].nBoundarySegments++;

                        // Add segment to vector.
                        segments[nSegments] = segment;
                        nSegments++;
                    }

                    // Update element status to indicate whether centre is in or out.
                    mesh.elements[i].status = (lsfSum > 0) ? ElementStatus::CENTRE_INSIDE : ElementStatus::CENTRE_OUTSIDE;
                }

                // If no edges are cut and element is not inside structure
                // then the boundary segment must cross the diagonal.
                else if ((nCut == 0) && (mesh.elements[i].status != ElementStatus::INSIDE))
                {
                    // Node index.
                    unsigned int node;

                    // Find the two boundary nodes.
                    for (unsigned int j=0;j<4;j++)
                    {
                        // Node index.
                        node = mesh.elements[i].nodes[j];

                        if (mesh.nodes[node].status & NodeStatus::BOUNDARY)
                        {
                            boundaryPoints[nCut] = node;
                            nCut++;
                        }
                    }

                    // Create boundary segment.
                    BoundarySegment segment;
                    segment.element = i;

                    // Initialise boundary point coordinate.
                    Coord coord;

                    // Make sure that the start boundary point hasn't already been added.
                    node = boundaryPoints[0];
                    int index = isAdded(coord, node, 0, 0);

                    // Boundary point is new.
                    if (index < 0)
                    {
                        // Set index equal to current number of points.
                        index = nPoints;

                        mesh.nodes[node].boundaryPoints[mesh.nodes[node].nBoundaryPoints] = nPoints;
                        mesh.nodes[node].nBoundaryPoints++;

                        // Initialise boundary point.
                        initialisePoint(points[nPoints], coord);

                        // Increment number of boundary points.
                        nPoints++;
                    }

                    // Assign start point index.
                    segment.start = index;

                    // Make sure that the end boundary point hasn't already been added.
                    node = boundaryPoints[1];
                    index = isAdded(coord, node, 0, 0);

                    // Boundary point is new.
                    if (index < 0)
                    {
                        // Set index equal to current number of points.
                        index = nPoints;

                        mesh.nodes[node].boundaryPoints[mesh.nodes[node].nBoundaryPoints] = nPoints;
                        mesh.nodes[node].nBoundaryPoints++;

                        // Initialise boundary point.
                        initialisePoint(points[nPoints], coord);

                        // Increment number of boundary points.
                        nPoints++;
                    }

                    // Assign end point index.
                    segment.end = index;

                    // Compute the length of the boundary segment.
                    segment.length = segmentLength(segment);

                    // Update total boundary length.
                    length += segment.length;

                    // Create element to segment lookup.
                    mesh.elements[i].boundarySegments[mesh.elements[i].nBoundarySegments] = nSegments;
                    mesh.elements[i].nBoundarySegments++;

                    // Add segment to vector.
                    segments[nSegments] = segment;
                    nSegments++;
                }
            }
        }

        // Resize points and segments vectors.
        points.resize(nPoints);
        segments.resize(nSegments);

        // Work out boundary integral length associated with each boundary point.
        computePointLengths();
    }

    double Boundary::computeAreaFractions()
    {
        // Zero the total area fraction.
        area = 0;

        for (unsigned int i=0;i<mesh.nElements;i++)
        {
            // Element is inside structure.
            if (mesh.elements[i].status & ElementStatus::INSIDE)
                mesh.elements[i].area = 1.0;

            // Element is outside structure.
            else if (mesh.elements[i].status & ElementStatus::OUTSIDE)
                mesh.elements[i].area = 0.0;

            // Element is cut by the boundary.
            else mesh.elements[i].area = cutArea(mesh.elements[i]);

            // Add the area to the running total.
            area += mesh.elements[i].area;
        }

        return area;
    }

    void Boundary::computeNormalVectors()
    {
        // Whether the normal vector at a boundary point has been set.
        bool isSet[nPoints];

        // Weighting factor for each point.
        double weight[nPoints];

        // Initialise arrays.
        for (unsigned int i=0;i<nPoints;i++)
        {
            isSet[i] = false;
            weight[i] = 0;
            points[i].normal[0] = 0;
            points[i].normal[1] = 0;
        }

        // Loop over all narrow band nodes.
        for (unsigned int i=0;i<levelSet.nNarrowBand;i++)
        {
            // Node index.
            unsigned int node = levelSet.narrowBand[i];

            // Node has a neighbouring boundary point and isn't on the domain boundary.
            if ((mesh.nodes[node].nBoundaryPoints > 0) && !mesh.nodes[node].isDomain)
            {
                // Nodal coordinates.
                unsigned int x = mesh.nodes[node].coord.x;
                unsigned int y = mesh.nodes[node].coord.y;

                // Calculate the normal vector by central finite differences.

                // Gradient in the x direction.
                double gradX = 0.5*(levelSet.signedDistance[mesh.xyToIndex[x+1][y]]
                             - levelSet.signedDistance[mesh.xyToIndex[x-1][y]]);

                // Gradient in the y direction.
                double gradY = 0.5*(levelSet.signedDistance[mesh.xyToIndex[x][y+1]]
                             - levelSet.signedDistance[mesh.xyToIndex[x][y-1]]);

                // Absolute gradient.
                double grad = sqrt(gradX*gradX + gradY*gradY);

                // Compute the two normal vector components.
                double xNormal = gradX / grad;
                double yNormal = gradY / grad;

                // Loop over all boundary points.
                for (unsigned int j=0;j<mesh.nodes[node].nBoundaryPoints;j++)
                {
                    // Boundary point index.
                    unsigned int point = mesh.nodes[node].boundaryPoints[j];

                    // Distance from the boundary point to the node.
                    double dx = mesh.nodes[node].coord.x - points[point].coord.x;
                    double dy = mesh.nodes[node].coord.y - points[point].coord.y;

                    // Squared distance.
                    double rSqd = dx*dx + dy*dy;

                    // If boundary point lies exactly on the node, then set normal
                    // vector to that of the node.
                    if (rSqd < 1e-6)
                    {
                        points[point].normal[0] = xNormal;
                        points[point].normal[1] = yNormal;
                        weight[point] = 1.0;
                        isSet[point] = true;
                    }

                    else
                    {
                        // Update normal vector estimate if not already set.
                        if (!isSet[point])
                        {
                            points[point].normal[0] += xNormal / rSqd;
                            points[point].normal[1] += yNormal / rSqd;
                            weight[point] += 1.0 / rSqd;
                        }
                    }
                }
            }
        }

        // Compute interpolated normal vector.
        for (unsigned int i=0;i<nPoints;i++)
        {
            if (!points[i].isDomain)
            {
                points[i].normal[0] /= weight[i];
                points[i].normal[1] /= weight[i];

                // Compute the new vector norm.
                double norm = sqrt(points[i].normal[0]*points[i].normal[0]
                            + points[i].normal[1]*points[i].normal[1]);

                points[i].normal[0] /= norm;
                points[i].normal[1] /= norm;
            }
        }
    }

    double Boundary::computePerimeter(const BoundaryPoint& point)
    {
        double length = 0;

        // Sum the distance to each neighbour.
        for (unsigned int i=0;i<point.nNeighbours;i++)
        {
            double dx = point.coord.x - points[point.neighbours[i]].coord.x;
            double dy = point.coord.y - points[point.neighbours[i]].coord.y;

            length += sqrt(dx*dx + dy*dy);
        }

        return length;
    }

    void Boundary::computeMeshStatus(const std::vector<double>* signedDistance) const
    {
        // Calculate node status.
        for (unsigned int i=0;i<mesh.nNodes;i++)
        {
            // Reset the number of boundary points associated with the element.
            mesh.nodes[i].nBoundaryPoints = 0;

            // Flag node as being on the boundary if the signed distance is within
            // a small tolerance of the zero contour. This avoids problems with
            // rounding errors when generating the discretised boundary.
            if (std::abs((*signedDistance)[i]) < 1e-6)
            {
                mesh.nodes[i].status = NodeStatus::BOUNDARY;
            }
            else if ((*signedDistance)[i] < 0)
            {
                mesh.nodes[i].status = NodeStatus::OUTSIDE;
            }
            else mesh.nodes[i].status = NodeStatus::INSIDE;
        }

        // Calculate element status.
        for (unsigned int i=0;i<mesh.nElements;i++)
        {
            // Tally counters for the element's node statistics.
            unsigned int tallyInside = 0;
            unsigned int tallyOutside = 0;

            // Reset the number of boundary segments associated with the element.
            mesh.elements[i].nBoundarySegments = 0;

            // Loop over each node of the element.
            for (unsigned int j=0;j<4;j++)
            {
                unsigned int node = mesh.elements[i].nodes[j];

                if (mesh.nodes[node].status & NodeStatus::INSIDE) tallyInside++;
                else if (mesh.nodes[node].status & NodeStatus::OUTSIDE) tallyOutside++;
            }

            // No nodes are outside: element is inside the structure.
            if (tallyOutside == 0) mesh.elements[i].status = ElementStatus::INSIDE;

            // No nodes are inside: element is outside the structure.
            else if (tallyInside == 0) mesh.elements[i].status = ElementStatus::OUTSIDE;

            // Otherwise no status.
            else mesh.elements[i].status = ElementStatus::NONE;
        }
    }

    int Boundary::isAdded(Coord& point, const unsigned int& node, const unsigned int& edge, const double& distance)
    {
        // Work out the coordinates of the point (depends on which edge we are considering).
        // Set edge and distance equal to zero when considering boundary points lying exactly
        // on top of a mesh node.

        // Bottom edge.
        if (edge == 0)
        {
            point.x = mesh.nodes[node].coord.x + distance;
            point.y = mesh.nodes[node].coord.y;
        }
        // Right edge.
        else if (edge == 1)
        {
            point.x = mesh.nodes[node].coord.x;
            point.y = mesh.nodes[node].coord.y + distance;
        }
        // Top edge.
        else if (edge == 2)
        {
            point.x = mesh.nodes[node].coord.x - distance;
            point.y = mesh.nodes[node].coord.y;
        }
        // Left edge.
        else
        {
            point.x = mesh.nodes[node].coord.x;
            point.y = mesh.nodes[node].coord.y - distance;
        }

        // Check all points adjacent to the node.
        for (unsigned int i=0;i<mesh.nodes[node].nBoundaryPoints;i++)
        {
            // Index of the ith boundary point connected to the node.
            unsigned int index = mesh.nodes[node].boundaryPoints[i];

            // Point already exists.
            if ((std::abs(point.x - points[index].coord.x) < 1e-6) &&
                (std::abs(point.y - points[index].coord.y) < 1e-6))
            {
                // Boundary point is already added, return index.
                return index;
            }
        }

        // If we've made it this far, then point is new.
        return -1;
    }

    void Boundary::initialisePoint(BoundaryPoint& point, const Coord& coord)
    {
        // Initialise position, length, number of segments, and isDomain.
        point.coord = coord;
        point.length = 0;
        point.nSegments = 0;
        point.nNeighbours = 0;
        point.isDomain = false;

        // Assume two sensitivities to start with (objective and a single constraint).
        point.sensitivities.resize(2);

        // Initialise movement limit (CFL condition).
        point.negativeLimit = -levelSet.moveLimit;
        point.positiveLimit = levelSet.moveLimit;

        // Check whether point lies within half a grid spacing of the domain boundary.
        // If so, modify the lower movement limit so that point can't move outside of
        // the domain.

        // Closest distance to domain boundary in x.
        double minX = std::min(coord.x, mesh.width - coord.x);

        // Closest distance to domain boundary in y.
        double minY = std::min(coord.y, mesh.height - coord.y);

        // Closest distance to any domain boundary.
        double minBoundary = std::min(minX, minY);

        // Modify lower move limit.
        if (minBoundary < 0.5)
        {
            point.negativeLimit = -minBoundary;

            // Point is exactly on domain boundary.
            if (minBoundary < 1e-6)
                point.isDomain = true;
        }
    }

    double Boundary::cutArea(const Element& element)
    {
        // Number of polygon vertices.
        unsigned int nVertices = 0;

        // Polygon vertices (maximum of six).
        std::vector<Coord> vertices(6);

        // Whether we're searching for nodes that are inside or outside the boundary.
        NodeStatus::NodeStatus status;

        if (element.status & ElementStatus::CENTRE_OUTSIDE) status = NodeStatus::OUTSIDE;
        else status = NodeStatus::INSIDE;

        // Check all nodes of the element.
        for (unsigned int i=0;i<4;i++)
        {
            // Node index;
            unsigned int node = element.nodes[i];

            // Node matches status.
            if (mesh.nodes[node].status & status)
            {
                // Add coordinates to vertex array.
                vertices[nVertices].x = mesh.nodes[node].coord.x;
                vertices[nVertices].y = mesh.nodes[node].coord.y;

                // Increment number of vertices.
                nVertices++;
            }

            // Node is on the boundary.
            else if (mesh.nodes[node].status & NodeStatus::BOUNDARY)
            {
                // Next node.
                unsigned int n1 = (i == 3) ? 0 : (i + 1);
                n1 = element.nodes[n1];

                // Previous node.
                unsigned int n2 = (i == 0) ? 3 : (i - 1);
                n2 = element.nodes[n2];

                // Check that node isn't part of a boundary segment, i.e. both of its
                // neighbours are inside the structure.
                if ((mesh.nodes[n1].status & NodeStatus::INSIDE) &&
                    (mesh.nodes[n2].status & NodeStatus::INSIDE))
                {
                    // Add coordinates to vertex array.
                    vertices[nVertices].x = mesh.nodes[node].coord.x;
                    vertices[nVertices].y = mesh.nodes[node].coord.y;

                    // Increment number of vertices.
                    nVertices++;
                }
            }
        }

        // Add boundary segment start and end points.
        for (unsigned int i=0;i<element.nBoundarySegments;i++)
        {
            // Segment index.
            unsigned int segment = element.boundarySegments[i];

            // Add start point coordinates to vertices array.
            vertices[nVertices].x = points[segments[segment].start].coord.x;
            vertices[nVertices].y = points[segments[segment].start].coord.y;

            // Increment number of vertices.
            nVertices++;

            // Add end point coordinates to vertices array.
            vertices[nVertices].x = points[segments[segment].end].coord.x;
            vertices[nVertices].y = points[segments[segment].end].coord.y;

            // Increment number of vertices.
            nVertices++;
        }

        // Return area of the polygon.
        if (element.status & ElementStatus::CENTRE_OUTSIDE)
            return (1.0 - polygonArea(vertices, nVertices, element.coord));
        else
            return polygonArea(vertices, nVertices, element.coord);
    }

    bool Boundary::isClockwise(const Coord& point1, const Coord& point2, const Coord& centre) const
    {
        if ((point1.x - centre.x) >= 0 && (point2.x - centre.x) < 0)
            return false;

        if ((point1.x - centre.x) < 0 && (point2.x - centre.x) >= 0)
            return true;

        if ((point1.x - centre.x) == 0 && (point2.x - centre.x) == 0)
        {
            if ((point1.y - centre.y) >= 0 || (point2.y - centre.y) >= 0)
                return (point1.y > point2.y) ? false : true;

            return (point2.y > point1.y) ? false : true;
        }

        // Compute the cross product of the vectors (centre --> point1) x (centre --> point2).
        double det = (point1.x - centre.x) * (point2.y - centre.y)
                - (point2.x - centre.x) * (point1.y - centre.y);

        if (det < 0) return false;
        else return true;

        // Points are on the same line from the centre, check which point is
        // closer to the centre.

        double d1 = (point1.x - centre.x) * (point1.x - centre.x)
                  + (point1.y - centre.y) * (point1.y - centre.y);

        double d2 = (point2.x - centre.x) * (point2.x - centre.x)
                  + (point2.y - centre.y) * (point2.y - centre.y);

        return (d1 > d2) ? false : true;
    }

    double Boundary::polygonArea(std::vector<Coord>& vertices, const unsigned int& nVertices, const Coord& centre) const
    {
        double area = 0;

        // Sort vertices in anticlockwise order.
        std::sort(vertices.begin(), vertices.begin() + nVertices, std::bind(&Boundary::isClockwise,
            this, std::placeholders::_1, std::placeholders::_2, centre));

        // Loop over all vertices.
        for (unsigned int i=0;i<nVertices;i++)
        {
            // Next point around (looping back to beginning).
            unsigned int j = (i == (nVertices - 1)) ? 0 : (i + 1);

            area += vertices[i].x * vertices[j].y;
            area -= vertices[j].x * vertices[i].y;
        }

        area *= 0.5;

        return (std::abs(area));
    }

    double Boundary::segmentLength(const BoundarySegment& segment)
    {
        // Coordinates for start and end points.
        Coord p1, p2;

        p1.x = points[segment.start].coord.x;
        p1.y = points[segment.start].coord.y;

        p2.x = points[segment.end].coord.x;
        p2.y = points[segment.end].coord.y;

        // Compute separation in x and y directions.
        double dx = p1.x - p2.x;
        double dy = p1.y - p2.y;

        return (sqrt(dx*dx + dy*dy));
    }

    void Boundary::computePointLengths()
    {
        // Loop over all boundary segments.
        for (unsigned int i=0;i<nSegments;i++)
        {
            // Add half segment length to each boundary point.
            points[segments[i].start].length += 0.5 * segments[i].length;
            points[segments[i].end].length += 0.5 * segments[i].length;

            // Update point to segment lookup for start point.
            points[segments[i].start].segments[points[segments[i].start].nSegments] = i;
            points[segments[i].start].nSegments++;

            // Update point to segment lookup for end point.
            points[segments[i].end].segments[points[segments[i].end].nSegments] = i;
            points[segments[i].end].nSegments++;

            // Update nearest neigbours for the start point.
            points[segments[i].start].neighbours[points[segments[i].start].nNeighbours] = segments[i].end;
            points[segments[i].start].nNeighbours++;

            // Update nearest neigbours for the end point.
            points[segments[i].end].neighbours[points[segments[i].end].nNeighbours] = segments[i].start;
            points[segments[i].end].nNeighbours++;
        }
    }
}
