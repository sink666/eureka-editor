//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2020 Ioan Chera
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

#ifndef INSTANCE_H_
#define INSTANCE_H_

#include "Document.h"

//
// An instance with a document, holding all other associated data, such as the window reference, the
// wad list.
//
class Instance
{
public:
	// M_KEYS
	void Beep(EUR_FORMAT_STRING(const char *fmt), ...) const EUR_PRINTF(2, 3);

	// UI_INFOBAR
	void Status_Set(EUR_FORMAT_STRING(const char *fmt), ...) const EUR_PRINTF(2, 3);
	void Status_Clear() const;

public:	// will be private when we encapsulate everything
	Document level{*this};	// level data proper

	UI_MainWindow *main_win = nullptr;
};

extern Instance gInstance;	// for now we run with one instance, will have more for the MDI.

#endif
