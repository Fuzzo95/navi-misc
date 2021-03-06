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

#include "common.h"
#include "math.h"
#include "PlayerState.h"
#include "Pack.h"
#include "Protocol.h"
#include "StateDatabase.h"

// the full scale of a int16_t  (less 1.0 for safety)
const float smallScale     = 32766.0f;

// 2 cm resolution  (range: +/- 655.32 meters)
const float smallMaxDist   = 0.02f * smallScale;

// 1 cm/sec resolution  (range: +/- 327.66 meters/sec)
const float smallMaxVel    = 0.01f * smallScale;

// 0.001 radians/sec resolution  (range: +/- 32.766 rads/sec)
const float smallMaxAngVel = 0.001f * smallScale;


PlayerState::PlayerState()
: order(0), status(DeadStatus), azimuth(0.0f), angVel(0.0f)
{
  pos[0] = pos[1] = pos[2] = 0.0f;
  velocity[0] = velocity[0] = velocity[2] = 0.0f;
}

void*	PlayerState::pack(void* buf, uint16_t& code)
{
  order++;
  buf = nboPackInt(buf, int32_t(order));
  buf = nboPackShort(buf, int16_t(status));
  
  if ((BZDB.eval(StateDatabase::BZDB_NOSMALLPACKETS) > 0.0f) ||
      (fabsf (pos[0]) >= smallMaxDist)      ||
      (fabsf (pos[1]) >= smallMaxDist)      ||
      (fabsf (pos[2]) >= smallMaxDist)      ||
      (fabsf (velocity[0]) >= smallMaxVel)  ||
      (fabsf (velocity[1]) >= smallMaxVel)  ||
      (fabsf (velocity[2]) >= smallMaxVel)  ||
      (fabsf (angVel) >= smallMaxAngVel)) {

    code = MsgPlayerUpdate;
    
    buf = nboPackVector(buf, pos);
    buf = nboPackVector(buf, velocity);
    buf = nboPackFloat(buf, azimuth);
    buf = nboPackFloat(buf, angVel);
  }
  else {

    code = MsgPlayerUpdateSmall;
    
    int16_t posShort[3], velShort[3], aziShort, angVelShort;
    
    for (int i=0; i<3; i++) {
      posShort[i] = (int16_t) ((pos[i] * smallScale) / smallMaxDist);
      velShort[i] = (int16_t) ((velocity[i] * smallScale) / smallMaxVel);
    }

    // put the angle between -M_PI and +M_PI
    float angle = fmodf (azimuth, M_PI * 2.0f);
    if (angle > M_PI) {
      angle -= (M_PI * 2.0f);
    }    
    else if (angle < -M_PI) {
      angle += (M_PI * 2.0f);
    }    
    aziShort = (int16_t) ((angle * smallScale) / M_PI);
    angVelShort = (int16_t) ((angVel * smallScale) / smallMaxAngVel);

    buf = nboPackShort(buf, posShort[0]);
    buf = nboPackShort(buf, posShort[1]);
    buf = nboPackShort(buf, posShort[2]);
    buf = nboPackShort(buf, velShort[0]);
    buf = nboPackShort(buf, velShort[1]);
    buf = nboPackShort(buf, velShort[2]);
    buf = nboPackShort(buf, aziShort);
    buf = nboPackShort(buf, angVelShort);
  }
  return buf;
}

void*	PlayerState::unpack(void* buf, uint16_t code)
{
  int32_t inOrder;
  int16_t inStatus;
  buf = nboUnpackInt(buf, inOrder);
  buf = nboUnpackShort(buf, inStatus);
  order = int(inOrder);
  status = short(inStatus);

  if (code == MsgPlayerUpdate) {
    buf = nboUnpackVector(buf, pos);
    buf = nboUnpackVector(buf, velocity);
    buf = nboUnpackFloat(buf, azimuth);
    buf = nboUnpackFloat(buf, angVel);
  }
  else {
    int16_t posShort[3], velShort[3], aziShort, angVelShort;

    buf = nboUnpackShort(buf, posShort[0]);
    buf = nboUnpackShort(buf, posShort[1]);
    buf = nboUnpackShort(buf, posShort[2]);
    buf = nboUnpackShort(buf, velShort[0]);
    buf = nboUnpackShort(buf, velShort[1]);
    buf = nboUnpackShort(buf, velShort[2]);
    buf = nboUnpackShort(buf, aziShort);
    buf = nboUnpackShort(buf, angVelShort);
    
    for (int i=0; i<3; i++) {
      pos[i] = ((float)posShort[i] * smallMaxDist) / smallScale;
      velocity[i] = ((float)velShort[i] * smallMaxVel) / smallScale;
    }
    azimuth = ((float)aziShort * M_PI) / smallScale;
    angVel = ((float)angVelShort * smallMaxAngVel) / smallScale;
  }
  return buf;
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
