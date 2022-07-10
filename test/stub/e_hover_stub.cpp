//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2022 Ioan Chera
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "e_hover.h"

void Hover::fastOpposite_begin()
{
}

void Hover::fastOpposite_finish()
{
}

Objid hover::getNearbyObject(ObjType type, const Document &doc, const ConfigData &config,
                      const Grid_State_c &grid, const v2double_t &pos)
{
   return Objid();
}

Objid hover::getNearestSector(const Document &doc, const v2double_t &pos)
{
   return Objid();
}

int Hover::getOppositeSector(int ld, Side ld_side) const
{
   return 0;
}
