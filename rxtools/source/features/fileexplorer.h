/*
 * Copyright (C) 2015 The PASTA Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef FILEEXPLORER_H
#define FILEEXPLORER_H
 
#include <wchar.h>
#include "console.h"

void FileExplorerShow();
void FileExplorerNextSelection();
void FileExplorerPrevSelection();
int FileExplorerSelect(wchar_t *p, size_t n);
void FileExplorerBack();
int FileExplorerMain(wchar_t *p, size_t n);

#endif
