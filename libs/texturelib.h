/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#if !defined (INCLUDED_TEXTURELIB_H)
#define INCLUDED_TEXTURELIB_H

#include "debugging/debugging.h"
#include "math/Vector3.h"
#include "math/Vector2.h"
#include "math/Matrix4.h"
#include "math/Plane3.h"
#include "igl.h"
#include <vector>
#include <limits.h>

#include "iimage.h"
#include "ishaders.h"

typedef unsigned int GLuint;

enum ProjectionAxis {
	eProjectionAxisX = 0,
	eProjectionAxisY = 1,
	eProjectionAxisZ = 2,
};

inline Matrix4 matrix4_rotation_for_vector3(const Vector3& x, const Vector3& y, const Vector3& z) {
	return Matrix4::byColumns(
		x.x(), x.y(), x.z(), 0,
		y.x(), y.y(), y.z(), 0,
		z.x(), z.y(), z.z(), 0,
		0, 0, 0, 1
	);
}

inline Matrix4 matrix4_swap_axes(const Vector3& from, const Vector3& to) {
	if(from.x() != 0 && to.y() != 0) {
		return matrix4_rotation_for_vector3(to, from, g_vector3_axis_z);
	}

	if(from.x() != 0 && to.z() != 0) {
		return matrix4_rotation_for_vector3(to, g_vector3_axis_y, from);
	}

	if(from.y() != 0 && to.z() != 0) {
		return matrix4_rotation_for_vector3(g_vector3_axis_x, to, from);
	}

	if(from.y() != 0 && to.x() != 0) {
		return matrix4_rotation_for_vector3(from, to, g_vector3_axis_z);
	}

	if(from.z() != 0 && to.x() != 0) {
		return matrix4_rotation_for_vector3(from, g_vector3_axis_y, to);
	}

	if(from.z() != 0 && to.y() != 0) {
		return matrix4_rotation_for_vector3(g_vector3_axis_x, from, to);
	}

	ERROR_MESSAGE("unhandled axis swap case");

	return Matrix4::getIdentity();
}

inline Matrix4 matrix4_reflection_for_plane(const Plane3& plane) {
	return Matrix4::byColumns(
		1 - (2 * plane.normal().x() * plane.normal().x()),
		-2 * plane.normal().x() * plane.normal().y(),
		-2 * plane.normal().x() * plane.normal().z(),
		0,
		-2 * plane.normal().y() * plane.normal().x(),
		1 - (2 * plane.normal().y() * plane.normal().y()),
		-2 * plane.normal().y() * plane.normal().z(),
		0,
		-2 * plane.normal().z() * plane.normal().x(),
		-2 * plane.normal().z() * plane.normal().y(),
		1 - (2 * plane.normal().z() * plane.normal().z()),
		0,
		-2 * plane.dist() * plane.normal().x(),
		-2 * plane.dist() * plane.normal().y(),
		-2 * plane.dist() * plane.normal().z(),
		1
	);
}

inline Matrix4 matrix4_reflection_for_plane45(const Plane3& plane, const Vector3& from, const Vector3& to) {
	Vector3 first = from;
	Vector3 second = to;

	if ((from.dot(plane.normal()) > 0) == (to.dot(plane.normal()) > 0))
    {
		first = -first;
		second = -second;
	}

#if 0
  rMessage() << "normal: ";
  print_vector3(plane.normal());

  rMessage() << "from: ";
  print_vector3(first);

  rMessage() << "to: ";
  print_vector3(second);
#endif

	Matrix4 swap = matrix4_swap_axes(first, second);

	//Matrix4 tmp = matrix4_reflection_for_plane(plane);

	swap.tx() = -(-2 * plane.normal().x() * plane.dist());
	swap.ty() = -(-2 * plane.normal().y() * plane.dist());
	swap.tz() = -(-2 * plane.normal().z() * plane.dist());

	return swap;
}

const double ProjectionAxisEpsilon = 0.0001;

inline bool projectionaxis_better(double axis, double other) {
	return fabs(axis) > fabs(other) + ProjectionAxisEpsilon;
}

/// \brief Texture axis precedence: Z > X > Y
inline ProjectionAxis projectionaxis_for_normal(const Vector3& normal) {
	return (projectionaxis_better(normal[eProjectionAxisY], normal[eProjectionAxisX]))
		? (projectionaxis_better(normal[eProjectionAxisY], normal[eProjectionAxisZ]))
			? eProjectionAxisY
			: eProjectionAxisZ
		: (projectionaxis_better(normal[eProjectionAxisX], normal[eProjectionAxisZ]))
			? eProjectionAxisX
			: eProjectionAxisZ;
}

/*!
\brief Construct a transform from XYZ space to ST space (3d to 2d).
This will be one of three axis-aligned spaces, depending on the surface normal.
NOTE: could also be done by swapping values.
*/
inline void Normal_GetTransform(const Vector3& normal, Matrix4& transform) {
	switch (projectionaxis_for_normal(normal)) {
		case eProjectionAxisZ:
			transform[0]  =  1;
			transform[1]  =  0;
			transform[2]  =  0;

			transform[4]  =  0;
			transform[5]  =  1;
			transform[6]  =  0;

			transform[8]  =  0;
			transform[9]  =  0;
			transform[10] =  1;
		break;
		case eProjectionAxisY:
			transform[0]  =  1;
			transform[1]  =  0;
			transform[2]  =  0;

			transform[4]  =  0;
			transform[5]  =  0;
			transform[6]  = -1;

			transform[8]  =  0;
			transform[9]  =  1;
			transform[10] =  0;
		break;
		case eProjectionAxisX:
			transform[0]  =  0;
			transform[1]  =  0;
			transform[2]  =  1;

			transform[4]  =  1;
			transform[5]  =  0;
			transform[6]  =  0;

			transform[8]  =  0;
			transform[9]  =  1;
			transform[10] =  0;
		break;
	}
	transform[3] = transform[7] = transform[11] = transform[12] = transform[13] = transform[14] = 0;
	transform[15] = 1;
}

/* greebo: This method calculates the normalised texture basis vectors of the texture plane 
 * as defined by <normal>.
 * 
 * Why is this needed? To calculate the texture coords of a brush winding, 
 * the texture projection matrix is used to transform the 3D vertex coords into 2D UV space. 
 * But *before* this happens the world system has to be rotated to align with 
 * the face's UV space. We only need the face normal to calculate the UV base vectors.
 * 
 * There are two special cases coded into this method dealing with normals pointing 
 * straight up or straight down - in theses cases the world XY plane is
 * rotated by 90 degrees.
 * 
 * The idTech4 game engine is doing the same, even though the algorithm for the
 * method "ComputeAxisBase" is implemented differently, based on atan2 instead
 * of if-else and cross-products. Note that this behaviour seems to be different
 * from Quake3, which appears to use one of six possibly axis-aligned projection bases.
 * 
 * The output basis vectors will be normalised.
 * 
 * If the normal vector points to the z-direction, the basis vectors are part
 * of the xy-plane: texS = <0,1,0> and texT = <1,0,0>
 *
 * If normal vector points to the negative z-direction, the above case applies, but with
 * the x-direction inversed: texS = <0,1,0> and texT = <-1,0,0> (note the minus)
 *
 * (I assume that the rotation of the vectors is to make images appear upright instead
 * of vertically flipped for faces that are pointing towards -z or +z.)
 * 
 * If none of the two above cases apply, the basis is calculated via cross products
 * that result in vectors perpendicular to <normal>. These lie within the plane
 * that is defined by the normal vector itself.
 *
 * Note: the vector <normal> MUST be normalised for this to function correctly.
 */
inline void ComputeAxisBase(const Vector3& normal, Vector3& texS, Vector3& texT)
{
	static const Vector3 up(0, 0, 1);
	static const Vector3 down(0, 0, -1);

	if (math::isNear(normal, up, 1e-6)) // straight up?
	{
		texS = Vector3(0, 1, 0);
		texT = Vector3(1, 0, 0);
	}
	else if (math::isNear(normal, down, 1e-6)) // straight down?
	{
		texS = Vector3(0, 1, 0);
		texT = Vector3(-1, 0, 0);
	}
	else
	{
		texS = normal.cross(up).getNormalised();
		texT = normal.cross(texS).getNormalised();
		texS = -texS;
	}
}

/* greebo: This returns the basis vectors of the texture (plane) space needed for projection.
 * The vectors are normalised and stored within the basis matrix <basis>
 * as column vectors.
 *
 * Note: the normal vector MUST be normalised already when this function is called,
 * but this should be fulfilled as it represents a FacePlane vector (which is usually normalised)
 */
inline Matrix4 getBasisTransformForNormal(const Vector3& normal)
{
    auto basis = Matrix4::getIdentity();

    ComputeAxisBase(normal, basis.xCol().getVector3(), basis.yCol().getVector3());
    basis.zCol().getVector3() = normal;

    // At this point the basis matrix contains three column vectors that are
    // perpendicular to each other.

    // The x-line of <basis> contains the <texS> basis vector (within the face plane)
    // The y-line of <basis> contains the <texT> basis vector (within the face plane)
    // The z-line of <basis> contains the <normal> basis vector (perpendicular to the face plane)

    // invert this matrix and return
    return basis.getTransposed(); 
}

/* greebo: this is used to calculate the directions the patch is "flattened" in.
 * If one of the patch bases is parallel or anti-parallel to the <faceNormal> it cannot
 * be projected onto the facePlane, so a new orthogonal vector is taken as direction instead.
 *
 * This prevents the patch from disappearing and the texture from being infinetly stretched in such cases.
 *
 * @returns: This returns two normalised vectors that are orthogonal to the face plane normal and point
 * into the direction of the patch orientation. */
inline void getVirtualPatchBase(const Vector3& widthVector, const Vector3& heightVector,
								const Vector3& faceNormal, Vector3& widthBase, Vector3& heightBase)
{
	bool widthVectorIsParallel = math::isParallel(widthVector, faceNormal);
	bool heightVectorIsParallel = math::isParallel(heightVector, faceNormal);

	if (widthVectorIsParallel) {
		// Calculate a orthogonal width vector
		widthBase = faceNormal.cross(heightVector).getNormalised();
	}
	else {
		// Project the vector onto the faceplane (this is the width direction)
		widthBase = (widthVector - faceNormal*(faceNormal*widthVector)).getNormalised();
	}

	if (heightVectorIsParallel) {
		// Calculate a orthogonal height vector
		heightBase = faceNormal.cross(widthVector).getNormalised();
	}
	else {
		// Project the vector onto the faceplane (this is the height direction)
		heightBase = (heightVector - faceNormal*(faceNormal*heightVector)).getNormalised();
	}
}

// handles degenerate cases, just in case library atan2 doesn't
inline double arctangent_yx(double y, double x) {
	if (fabs(x) > 1.0E-6) {
		return atan2(y, x);
	}
	else if (y > 0) {
		return c_half_pi;
	}
	else {
		return -c_half_pi;
	}
}

// Returns the index of the one edge which points "most" into the given direction, <direction> should be normalised
inline std::size_t findBestEdgeForDirection(const Vector2& direction, const std::vector<Vector2>& edges)
{
	double best = -LONG_MAX;
	std::size_t bestIndex = 0;

	for (std::size_t i = 0; i < edges.size(); ++i)
	{
		double dot = direction.dot(edges[i]);

		if (dot <= best) continue;

		// Found a new best edge
		bestIndex = i;
		best = dot;
	}

	return bestIndex;
}

#endif
