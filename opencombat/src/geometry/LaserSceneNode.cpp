/* bzflag
 * Copyright (c) 1993 - 2004 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <math.h>
#include "common.h"
#include "LaserSceneNode.h"
#include "SceneRenderer.h"
#include "OpenGLTexture.h"
#include "StateDatabase.h"
#include "BZDBCache.h"

const GLfloat		LaserRadius = 0.1f;

LaserSceneNode::LaserSceneNode(const GLfloat pos[3], const GLfloat forward[3]) :
				texturing(false),
				renderNode(this)
{
  // prepare rendering info
  azimuth = 180.0f / M_PI*atan2f(forward[1], forward[0]);
  elevation = -180.0f / M_PI*atan2f(forward[2], hypotf(forward[0],forward[1]));
  length = hypotf(forward[0], hypotf(forward[1], forward[2]));

  // setup sphere
  setCenter(pos);
  setRadius(length * length);

  OpenGLGStateBuilder builder(gstate);
  builder.setCulling(GL_NONE);
  gstate = builder.getState();
}

LaserSceneNode::~LaserSceneNode()
{
  // do nothing
}

void			LaserSceneNode::setTexture(const int texture)
{
  OpenGLGStateBuilder builder(gstate);
  builder.setTexture(texture);
  builder.enableTexture(texture>=0);
  gstate = builder.getState();
  forceNotifyStyleChange();
}

bool			LaserSceneNode::cull(const ViewFrustum&) const
{
  // no culling
  return false;
}

void			LaserSceneNode::notifyStyleChange(
				const SceneRenderer&)
{
  texturing = BZDBCache::texture;
  OpenGLGStateBuilder builder(gstate);
  builder.enableTexture(texturing);
  gstate = builder.getState();
}

void			LaserSceneNode::addRenderNodes(
				SceneRenderer& renderer)
{
  renderer.addRenderNode(&renderNode, &gstate);
}

//
// LaserSceneNode::LaserRenderNode
//

GLfloat			LaserSceneNode::LaserRenderNode::geom[6][2];

LaserSceneNode::LaserRenderNode::LaserRenderNode(
				const LaserSceneNode* _sceneNode) :
				sceneNode(_sceneNode)
{
  // initialize geometry if first instance
  static bool init = false;
  if (!init) {
    init = true;
    for (int i = 0; i < 6; i++) {
      geom[i][0] = -LaserRadius * cosf(2.0f * M_PI * float(i) / 6.0f);
      geom[i][1] =  LaserRadius * sinf(2.0f * M_PI * float(i) / 6.0f);
    }
  }
}

LaserSceneNode::LaserRenderNode::~LaserRenderNode()
{
  // do nothing
}

void			LaserSceneNode::LaserRenderNode::render()
{
  const GLfloat length = sceneNode->length;
  const GLfloat* sphere = sceneNode->getSphere();
  glPushMatrix();
    glTranslatef(sphere[0], sphere[1], sphere[2]);
    glRotatef(sceneNode->azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(sceneNode->elevation, 0.0f, 1.0f, 0.0f);

    if (sceneNode->texturing) {
      myColor3f(1.0f, 1.0f, 1.0f);
      glBegin(GL_TRIANGLE_FAN);
	glTexCoord2f(0.5f,  0.5f);
	glVertex3f(  0.0f,  0.0f,  0.0f);
	glTexCoord2f(0.0f,  0.0f);
	glVertex3f(  0.0f,  0.0f,  1.0f);
	glVertex3f(  0.0f,  1.0f,  0.0f);
	glVertex3f(  0.0f,  0.0f, -1.0f);
	glVertex3f(  0.0f, -1.0f,  0.0f);
	glVertex3f(  0.0f,  0.0f,  1.0f);
      glEnd();

      glBegin(GL_QUADS);
	glTexCoord2f(0.0f,  0.0f);
	glVertex3f(  0.0f,  0.0f,  1.0f);
	glTexCoord2f(0.0f,  1.0f);
	glVertex3f(length,  0.0f,  1.0f);
	glTexCoord2f(1.0f,  1.0f);
	glVertex3f(length,  0.0f, -1.0f);
	glTexCoord2f(1.0f,  0.0f);
	glVertex3f(  0.0f,  0.0f, -1.0f);

	glTexCoord2f(0.0f,  0.0f);
	glVertex3f(  0.0f,  1.0f,  0.0f);
	glTexCoord2f(0.0f,  1.0f);
	glVertex3f(length,  1.0f,  0.0f);
	glTexCoord2f(1.0f,  1.0f);
	glVertex3f(length, -1.0f,  0.0f);
	glTexCoord2f(1.0f,  0.0f);
	glVertex3f(  0.0f, -1.0f,  0.0f);
      glEnd();
    }

    else {
      // draw beam
      myColor4f(1.0f, 0.25f, 0.0f, 0.85f);
      glBegin(GL_QUAD_STRIP);
	glVertex3f(  0.0f, geom[0][0], geom[0][1]);
	glVertex3f(length, geom[0][0], geom[0][1]);
	glVertex3f(  0.0f, geom[1][0], geom[1][1]);
	glVertex3f(length, geom[1][0], geom[1][1]);
	glVertex3f(  0.0f, geom[2][0], geom[2][1]);
	glVertex3f(length, geom[2][0], geom[2][1]);
	glVertex3f(  0.0f, geom[3][0], geom[3][1]);
	glVertex3f(length, geom[3][0], geom[3][1]);
	glVertex3f(  0.0f, geom[4][0], geom[4][1]);
	glVertex3f(length, geom[4][0], geom[4][1]);
	glVertex3f(  0.0f, geom[5][0], geom[5][1]);
	glVertex3f(length, geom[5][0], geom[5][1]);
	glVertex3f(  0.0f, geom[0][0], geom[0][1]);
	glVertex3f(length, geom[0][0], geom[0][1]);
      glEnd();

      // also draw a line down the middle (so the beam is visible even
      // if very far away).  this will also give the beam an extra bright
      // center.
      glBegin(GL_LINES);
	glVertex3f(  0.0f, 0.0f, 0.0f);
	glVertex3f(length, 0.0f, 0.0f);
      glEnd();
    }

  glPopMatrix();
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

