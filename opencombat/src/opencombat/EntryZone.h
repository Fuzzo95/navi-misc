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

#ifndef __ENTRYZONE_H__
#define __ENTRYZONE_H__

#include <vector>
#include "Flag.h"
#include "Team.h"

class EntryZone {
  public:
    EntryZone() {
      pos[0] = pos[1] = pos[2] = 0.0f;
      size[0] = size[1] = size[2] = 1.0f;
      rot = 0.0f;
      flags.clear();
      teams.clear();
      safety.clear();
    }
    float pos[3];
    float size[3];
    float rot;
    std::vector<FlagType*> flags;
    std::vector<TeamColor> teams;
    std::vector<TeamColor> safety;
};


#endif /* __ENTRYZONE_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
