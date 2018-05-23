/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *  Yannick Koehler <koehler@mythrium.com>
 *  Paul Ashford <arougthopher@lizardland.net>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#ifndef nsBeOSCursors_h__
#define nsBeOSCursors_h__

#include <app/Cursor.h>


static unsigned char cursorHyperlink[] = {16,1,2,2,
0,0,0,0,56,0,36,0,36,0,19,224,18,92,9,42,
8,1,60,33,76,113,66,113,48,249,12,249,2,0,1,0,
0,0,0,0,56,0,60,0,60,0,31,224,31,252,15,254,
15,255,63,255,127,255,127,255,63,255,15,255,3,254,1,248
};

static unsigned char cursorHelp[] = {16,1,2,2,
0,0,0,0,56,0,36,0,36,0,19,224,18,92,9,42,
8,1,60,121,76,205,66,25,48,49,12,49,2,0,1,48,
0,0,0,0,56,0,60,0,60,0,31,224,31,252,15,254,
15,255,63,255,127,255,127,255,63,255,15,255,3,254,1,248
};

static unsigned char cursorCrosshair[] = {16,1,8,8,
0,0,0,128,0,128,0,128,0,128,0,128,0,128,0,128,
127,255,0,128,0,128,0,128,0,128,0,128,0,128,0,128,
0,0,0,128,0,128,0,128,0,128,0,128,0,128,0,128,
127,255,0,128,0,128,0,128,0,128,0,128,0,128,0,128
};

static unsigned char cursorCopy[] = {16,1,2,2,
0,0,0,0,56,0,36,0,36,0,19,224,18,92,9,42,
8,1,60,1,76,1,66,17,48,17,12,125,2,17,1,16,
0,0,0,0,56,0,60,0,60,0,31,224,31,252,15,254,
15,255,63,255,127,255,127,255,63,255,15,255,3,255,1,248
};

static unsigned char cursorAlias[] = {16,1,2,2,
0,0,0,0,56,0,36,0,36,0,19,224,18,92,9,42,
8,1,60,1,76,113,66,137,48,157,12,137,2,65,1,0,
0,0,0,0,56,0,60,0,60,0,31,224,31,252,15,254,
15,255,63,255,127,255,127,255,63,255,15,255,3,255,1,248
};

static unsigned char cursorGrabbing[] = {16,1,8,2,
0,0,0,0,0,0,0,0,15,224,18,92,33,42,40,1,
60,1,20,1,18,1,8,1,4,1,2,1,1,0,0,128,
0,0,0,0,0,0,0,0,15,224,31,252,63,254,63,255,
63,255,31,255,31,255,15,255,7,255,3,255,1,254,0,248
};

static unsigned char cursorGrab[] = {16,1,2,2,
0,0,0,0,59,64,36,168,36,148,18,74,18,74,9,37,
8,1,60,1,76,1,68,1,48,1,12,1,2,0,1,0,
0,0,0,0,59,64,63,232,63,252,31,254,31,254,15,255,
15,255,63,255,127,255,127,255,63,255,15,255,3,254,1,248
};
  
static unsigned char cursorLowerRight[] = {16,1,13,13,
0,0,0,0,0,0,0,0,0,0,4,6,2,38,1,38,
0,166,0,102,3,230,0,6,0,6,7,254,7,254,0,0,
0,0,0,0,0,0,0,0,4,15,14,127,7,127,3,255,
1,255,7,255,7,255,7,255,15,255,15,255,15,255,15,255
};

static unsigned char cursorLowerLeft[] = {16,1,13,2,
0,0,0,0,0,0,0,0,0,0,96,32,100,64,100,128,
101,0,102,0,103,192,96,0,96,0,127,224,127,224,0,0,
0,0,0,0,0,0,0,0,240,32,254,112,254,224,255,192,
255,128,255,224,255,224,255,224,255,240,255,240,255,240,255,240
};

static unsigned char cursorUpperLeft[] = {16,1,2,2,
0,0,127,224,127,224,96,0,96,0,103,192,102,0,101,0,
100,128,100,64,96,32,0,0,0,0,0,0,0,0,0,0,
255,240,255,240,255,240,255,240,255,224,255,224,255,224,255,128,
255,192,254,224,254,112,240,32,0,0,0,0,0,0,0,0
};

static unsigned char cursorUpperRight[] = {16,1,2,13,
0,0,7,254,7,254,0,6,0,6,3,230,0,102,0,166,
1,38,2,38,4,6,0,0,0,0,0,0,0,0,0,0,
15,255,15,255,15,255,15,255,7,255,7,255,7,255,1,255,
3,255,7,127,14,127,4,15,0,0,0,0,0,0,0,0
};

static unsigned char cursorRight[] = {16,1,6,13,
0,0,0,6,0,6,1,6,0,134,0,70,15,230,0,70,
0,134,1,6,0,6,0,6,0,0,0,0,0,0,0,0,
0,15,0,15,1,15,3,143,1,207,31,239,31,255,31,239,
1,207,3,143,1,15,0,15,0,15,0,0,0,0,0,0
};

static unsigned char cursorLeft[] = {16,1,6,2,
0,0,96,0,96,0,96,128,97,0,98,0,103,224,98,0,
97,0,96,128,96,0,96,0,0,0,0,0,0,0,0,0,
240,0,240,0,240,128,241,192,243,128,247,240,255,240,247,240,
243,128,241,192,240,128,240,0,240,0,0,0,0,0,0,0
};

static unsigned char cursorTop[] = {16,1,2,7,
0,0,63,248,63,248,0,0,0,0,1,0,3,128,5,64,
9,32,1,0,1,0,0,0,0,0,0,0,0,0,0,0,
127,252,127,252,127,252,127,252,1,0,3,128,7,192,15,224,
31,240,11,160,3,128,3,128,0,0,0,0,0,0,0,0
};

static unsigned char cursorBottom[] = {16,1,13,7,
0,0,0,0,0,0,0,0,0,0,1,0,1,0,9,32,
5,64,3,128,1,0,0,0,0,0,63,248,63,248,0,0,
0,0,0,0,0,0,0,0,3,128,3,128,11,160,31,240,
15,224,7,192,3,128,1,0,127,252,127,252,127,252,127,252
};

static unsigned char cursorHorizontalDrag[] = {16,1,7,7,
1,128,1,128,1,128,1,128,1,128,17,136,49,140,125,190,
125,190,49,140,17,136,1,128,1,128,1,128,1,128,1,128,
3,192,3,192,3,192,3,192,27,216,59,220,127,254,255,255,
255,255,127,254,59,220,27,216,3,192,3,192,3,192,3,192
};

static unsigned char cursorVerticalDrag[] = {16,1,7,7,
0,0,1,128,3,192,7,224,1,128,1,128,0,0,255,255,
255,255,0,0,1,128,1,128,7,224,3,192,1,128,0,0,
1,128,3,192,7,224,15,240,15,240,3,192,255,255,255,255,
255,255,255,255,3,192,15,240,15,240,7,224,3,192,1,128
};


static unsigned char cursorWatch2[] = {16,1,0,1,
112,0,72,0,72,0,39,192,36,184,18,84,16,2,120,2,
152,2,132,2,96,58,24,70,4,138,2,146,1,130,0,197,
112,0,120,0,120,0,63,192,63,248,31,252,31,254,127,254,
255,254,255,254,127,254,31,254,7,254,3,254,1,254,0,255
};

#if 0
// keeping these in case we want to animate the spinner at some point
static unsigned char cursorWatch1[] = {16,1,0,1,
112,0,72,0,72,0,39,192,36,184,18,84,16,2,120,2,
152,2,132,2,96,58,24,86,4,146,2,146,1,130,0,197,
112,0,120,0,120,0,63,192,63,248,31,252,31,254,127,254,
255,254,255,254,127,254,31,254,7,254,3,254,1,254,0,255
};

static unsigned char cursorWatch3[] = {16,1,0,1,
112,0,72,0,72,0,39,192,36,184,18,84,16,2,120,2,
152,2,132,2,96,58,24,70,4,130,2,158,1,130,0,197,
112,0,120,0,120,0,63,192,63,248,31,252,31,254,127,254,
255,254,255,254,127,254,31,254,7,254,3,254,1,254,0,255
};

static unsigned char cursorWatch4[] = {16,1,0,1,
112,0,72,0,72,0,39,192,36,184,18,84,16,2,120,2,
152,2,132,2,96,58,24,70,4,130,2,146,1,138,0,197,
112,0,120,0,120,0,63,192,63,248,31,252,31,254,127,254,
255,254,255,254,127,254,31,254,7,254,3,254,1,254,0,255
};

static unsigned char cursorWatch5[] = {16,1,0,1,
112,0,72,0,72,0,39,192,36,184,18,84,16,2,120,2,
152,2,132,2,96,58,24,70,4,130,2,146,1,146,0,213,
112,0,120,0,120,0,63,192,63,248,31,252,31,254,127,254,
255,254,255,254,127,254,31,254,7,254,3,254,1,254,0,255
};

static unsigned char cursorWatch6[] = {16,1,0,1,
112,0,72,0,72,0,39,192,36,184,18,84,16,2,120,2,
152,2,132,2,96,58,24,70,4,130,2,146,1,162,0,197,
112,0,120,0,120,0,63,192,63,248,31,252,31,254,127,254,
255,254,255,254,127,254,31,254,7,254,3,254,1,254,0,255
};

static unsigned char cursorWatch7[] = {16,1,0,1,
112,0,72,0,72,0,39,192,36,184,18,84,16,2,120,2,
152,2,132,2,96,58,24,70,4,130,2,242,1,130,0,197,
112,0,120,0,120,0,63,192,63,248,31,252,31,254,127,254,
255,254,255,254,127,254,31,254,7,254,3,254,1,254,0,255
};

static unsigned char cursorWatch8[] = {16,1,0,1,
112,0,72,0,72,0,39,192,36,184,18,84,16,2,120,2,
152,2,132,2,96,58,24,70,4,162,2,146,1,130,0,197,
112,0,120,0,120,0,63,192,63,248,31,252,31,254,127,254,
255,254,255,254,127,254,31,254,7,254,3,254,1,254,0,255
};
#endif
        
#endif // nsBeOSCursors_h__

