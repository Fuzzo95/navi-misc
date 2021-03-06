/*
 * ViewingFrustum.h - Culling and projection based on the current OpenGL context
 *
 * Copyright (C) 2002-2004 Micah Dowty and David Trowbridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#ifndef _H_VIEWINGFRUSTUM
#define _H_VIEWINGFRUSTUM

#include "Vector.h"
#include "Matrix.h"

class ViewingFrustum {
 public:
  static ViewingFrustum *getInstance(void);

  /* This class needs to recalculate the current matrix and frustum each time
   * the camera moves. This function -must- be called when the OpenGL matrix is
   * modified. Its speed penalty is negligible if you call it multiple times between drawings.
   */
  void invalidate();
  
  /* Return the current OpenGL matrices */
  const Matrix4x4 &getModelview(void);
  const Matrix4x4 &getProjection(void);
  const Matrix4x4 &getProjectionTimesModelview(void);

  const int* getViewport(void);
  
  /* Project a point into viewport coordinates using the current OpenGL context.
   * Does not use the modelview matrix, only the projection matrix.
   */
  Vector2 pixelProject(const Vector3 &v);

  /* Frustum testing functions - Return a bitfield indicating which planes the object is behind */

  enum frustumBitfield {
    behindNone   = 0x0000,
    behindRight  = 0x0001,
    behindLeft   = 0x0002,
    behindBottom = 0x0004,
    behindTop    = 0x0008,
    behindFar    = 0x0010,
    behindNear   = 0x0020,
  };

  frustumBitfield testPoint(const Vector3 &a);
  frustumBitfield testSphere(const Vector3 &a, const float radius);
  
  /* Do full culling, returning an object's relationship to the frustum: */
  
  enum intersectionCode {
    fullyOutside,
    fullyInside,
    overlappingBorder
  };
  
  intersectionCode cullPoint(const Vector3 &a);
  intersectionCode cullSphere(const Vector3 &a, const float radius);

  enum planes {
    rightPlane,
    leftPlane,
    bottomPlane,
    topPlane,
    farPlane,
    nearPlane,
  };

  float pointFrustumDistance(int plane, const Vector3 &v);

 private:
  ViewingFrustum();
  static ViewingFrustum *instance;

  void calculate();
  bool dirty;

  /* Data generated by calculate() */
  int viewport[4];
  Matrix4x4 modelview;
  Matrix4x4 projection;
  Matrix4x4 projectionTimesModelview;

  /* Frustum planes, in order: */
  Vector4 frustum[6];

};

#endif /* _H_VIEWINGFRUSTUM */

/* The End */
