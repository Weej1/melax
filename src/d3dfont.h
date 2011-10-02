//-----------------------------------------------------------------------------
// this file was stolen from the d3d demo common library,
// with some minor modificaitons here and there.
//
//  The design here is just wrong.  there shouldn't be a difference 
//  between the font material and any other in regards to how they are treated by the rendering systems.
// 
//
// File: D3DFont.h
//
// Desc: Texture-based font class
//
// Copyright (c) 1999-2001 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef SM_D3DFONT_H
#define SM_D3DFONT_H

struct IDirect3DDevice9;

void InitFont( struct IDirect3DDevice9 *device);
void DeleteFont();
void DrawText(float x,float y,const char *s);
void PostString(const char *_s,int _x,int _y,float _life);
void PrintString(char *s,int x,int y);
void RenderStrings();  // call this at the end of each frame

#endif


