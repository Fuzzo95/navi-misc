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

#include <stdlib.h>
#include <math.h>
#include "common.h"
#include "BoltSceneNode.h"
#include "ViewFrustum.h"
#include "SceneRenderer.h"
#include "OpenGLTexture.h"
#include "StateDatabase.h"
#include "BZDBCache.h"

BoltSceneNode::BoltSceneNode(const GLfloat pos[3]) :
				drawFlares(false),
				texturing(false),
				colorblind(false),
				size(1.0f),
				renderNode(this)
{
  OpenGLGStateBuilder builder(gstate);
  builder.enableTextureReplace();
  gstate = builder.getState();

  // prepare light
  light.setAttenuation(0, 0.05f);
  light.setAttenuation(1, 0.0f);
  light.setAttenuation(2, 0.03f);

  // prepare geometry
  move(pos, NULL);
  setSize(size);
  setColor(1.0f, 1.0f, 1.0f);
}

BoltSceneNode::~BoltSceneNode()
{
  // do nothing
}

void			BoltSceneNode::setFlares(bool on)
{
  drawFlares = on;
}

void			BoltSceneNode::setSize(float radius)
{
  size = radius;
  setRadius(size * size);
}
void			BoltSceneNode::setTextureColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
  color[0] = r;
  color[1] = g;
  color[2] = b;
  color[3] = a;
  light.setColor(1.5f * r, 1.5f * g, 1.5f * b);
  renderNode.setTextureColor(color);
}

void			BoltSceneNode::setColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
  color[0] = r;
  color[1] = g;
  color[2] = b;
  color[3] = a;
  light.setColor(1.5f * r, 1.5f * g, 1.5f * b);
  renderNode.setColor(color);
}

void			BoltSceneNode::setColor(const GLfloat* rgb)
{
  setColor(rgb[0], rgb[1], rgb[2]);
}

bool			BoltSceneNode::getColorblind() const
{
  return colorblind;
}

void			BoltSceneNode::setColorblind(bool _colorblind)
{
  colorblind = _colorblind;
}

void			BoltSceneNode::setTexture(const int texture)
{
  OpenGLGStateBuilder builder(gstate);
  builder.setTexture(texture);
  builder.enableTexture(texture>=0);
  gstate = builder.getState();
  forceNotifyStyleChange();
}

void			BoltSceneNode::setTextureAnimation(int cu, int cv)
{
  renderNode.setAnimation(cu, cv);
}

void			BoltSceneNode::move(const GLfloat pos[3],
							const GLfloat[3])
{
  setCenter(pos);
  light.setPosition(pos);
}

void			BoltSceneNode::addLight(
				SceneRenderer& renderer)
{
  renderer.addLight(light);
}

void			BoltSceneNode::notifyStyleChange(
				const SceneRenderer&)
{
}

void			BoltSceneNode::addRenderNodes(
				SceneRenderer& renderer)
{
  renderer.addRenderNode(&renderNode, &gstate);
}

//
// BoltSceneNode::BoltRenderNode
//

const GLfloat		BoltSceneNode::BoltRenderNode::CoreFraction = 0.4f;
const GLfloat		BoltSceneNode::BoltRenderNode::FlareSize = 1.0f;
const GLfloat		BoltSceneNode::BoltRenderNode::FlareSpread = 0.08f;
GLfloat			BoltSceneNode::BoltRenderNode::core[9][2];
GLfloat			BoltSceneNode::BoltRenderNode::corona[8][2];
const GLfloat		BoltSceneNode::BoltRenderNode::ring[8][2] = {
				{ 1.0f, 0.0f },
				{ M_SQRT1_2, M_SQRT1_2 },
				{ 0.0f, 1.0f },
				{ -M_SQRT1_2, M_SQRT1_2 },
				{ -1.0f, 0.0f },
				{ -M_SQRT1_2, -M_SQRT1_2 },
				{ 0.0f, -1.0f },
				{ M_SQRT1_2, -M_SQRT1_2 }
			};

BoltSceneNode::BoltRenderNode::BoltRenderNode(
				const BoltSceneNode* _sceneNode) :
				sceneNode(_sceneNode),
				numFlares(0)
{
  // initialize core and corona if not already done
  static bool init = false;
  if (!init) {
    init = true;
    core[0][0] = 0.0f;
    core[0][1] = 0.0f;
    for (int i = 0; i < 8; i++) {
      core[i+1][0] = CoreFraction * ring[i][0];
      core[i+1][1] = CoreFraction * ring[i][1];
      corona[i][0] = ring[i][0];
      corona[i][1] = ring[i][1];
    }
  }
  
  textureColor[0] = 1.0f;
  textureColor[1] = 1.0f;
  textureColor[2] = 1.0f;
  textureColor[3] = 1.0f;

  setAnimation(1, 1);
}

BoltSceneNode::BoltRenderNode::~BoltRenderNode()
{
  // do nothing
}

void			BoltSceneNode::BoltRenderNode::setAnimation(
				int _cu, int _cv)
{
  cu = _cu;
  cv = _cv;
  du = 1.0f / (float)cu;
  dv = 1.0f / (float)cv;

  // pick a random start frame
  const int index = (int)((float)cu * (float)cv * bzfrand());
  u = index % cu;
  v = index / cu;
  if (v >= cv) v = 0;
}
void			BoltSceneNode::BoltRenderNode::setTextureColor(const GLfloat* rgba)
{
  textureColor[0] = rgba[0];
  textureColor[1] = rgba[1];
  textureColor[2] = rgba[2];
  textureColor[3] = rgba[3];
}


void			BoltSceneNode::BoltRenderNode::setColor(
				const GLfloat* rgba)
{
  mainColor[0] = rgba[0];
  mainColor[1] = rgba[1];
  mainColor[2] = rgba[2];
  mainColor[3] = rgba[3];

  innerColor[0] = mainColor[0] + 0.5f * (1.0f - mainColor[0]);
  innerColor[1] = mainColor[1] + 0.5f * (1.0f - mainColor[1]);
  innerColor[2] = mainColor[2] + 0.5f * (1.0f - mainColor[2]);
  innerColor[3] = rgba[3];

  outerColor[0] = mainColor[0];
  outerColor[1] = mainColor[1];
  outerColor[2] = mainColor[2];
  outerColor[3] = (rgba[3] == 1.0f )? 0.1f: rgba[3];

  coronaColor[0] = mainColor[0];
  coronaColor[1] = mainColor[1];
  coronaColor[2] = mainColor[2];
  coronaColor[3] = (rgba[3] == 1.0f )? 0.5f : rgba[3];

  flareColor[0] = mainColor[0];
  flareColor[1] = mainColor[1];
  flareColor[2] = mainColor[2];
  flareColor[3] = (rgba[3] == 1.0f )? 0.667f : rgba[3];
}

void			BoltSceneNode::BoltRenderNode::render()
{
  const float u0 = (float)u * du;
  const float v0 = (float)v * dv;

  const GLfloat* sphere = sceneNode->getSphere();
  glPushMatrix();
    glTranslatef(sphere[0], sphere[1], sphere[2]);
    SceneRenderer::getInstance()->getViewFrustum().executeBillboard();
    glScalef(sceneNode->size, sceneNode->size, sceneNode->size);

    // draw some flares
    if (sceneNode->drawFlares) {
      if (!SceneRenderer::getInstance()->isSameFrame()) {
	numFlares = 3 + int(3.0f * (float)bzfrand());
	for (int i = 0; i < numFlares; i++) {
	  theta[i] = 2.0f * M_PI * (float)bzfrand();
	  phi[i] = (float)bzfrand() - 0.5f;
	  phi[i] *= 2.0f * M_PI * fabsf(phi[i]);
	}
      }

      if (sceneNode->texturing)
				glDisable(GL_TEXTURE_2D);
      myColor4fv(flareColor);
      glBegin(GL_QUADS);
      for (int i = 0; i < numFlares; i++) {
	// pick random direction in 3-space.  picking a random theta with
	// a uniform distribution is fine, but doing so with phi biases
	// the directions toward the poles.  my correction doesn't remove
	// the bias completely, but moves it towards the equator, which is
	// really where i want it anyway cos the flares are more noticeable
	// there.
	const GLfloat c = FlareSize * GLfloat(cosf(phi[i]));
	const GLfloat s = FlareSize * GLfloat(sinf(phi[i]));
	glVertex3fv(core[0]);
	glVertex3f(c * cosf(theta[i]-FlareSpread), c * sinf(theta[i]-FlareSpread), s);
	glVertex3f(2.0f * c * cosf(theta[i]), 2.0f * c * sinf(theta[i]), 2.0f * s);
	glVertex3f(c * cosf(theta[i]+FlareSpread), c * sinf(theta[i]+FlareSpread), s);
      }
      glEnd();
      if (sceneNode->texturing) glEnable(GL_TEXTURE_2D);
    }

		// draw billboard square
		myColor4fv(textureColor); // 1.0f all
		glBegin(GL_QUADS);
			glTexCoord2f(   u0,    v0);
			glVertex2f  (-1.0f, -1.0f);
			glTexCoord2f(du+u0,    v0);
			glVertex2f  ( 1.0f, -1.0f);
			glTexCoord2f(du+u0, dv+v0);
			glVertex2f  ( 1.0f,  1.0f);
			glTexCoord2f(   u0, dv+v0);
			glVertex2f  (-1.0f,  1.0f);
		glEnd();

  glPopMatrix();

  if (SceneRenderer::getInstance()->isLastFrame()) {
    if (++u == cu) {
      u = 0;
      if (++v == cv) v = 0;
    }
  }
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

