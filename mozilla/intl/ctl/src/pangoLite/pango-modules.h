/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * pango-modules.h:
 *
 * The contents of this file are subject to the Mozilla Public	
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Pango Library (www.pango.org) 
 * 
 * The Initial Developer of the Original Code is Red Hat Software
 * Portions created by Red Hat are Copyright (C) 1999 Red Hat Software.
 * 
 * Contributor(s): 
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Lessor General Public License Version 2 (the 
 * "LGPL"), in which case the provisions of the LGPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the LGPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the LGPL. If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * LGPL.
*/

#ifndef __PANGO_MODULES_H__
#define __PANGO_MODULES_H__

#include "pango-engine.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _PangoMap PangoMap;
typedef struct _PangoMapEntry PangoMapEntry;

struct _PangoMapEntry 
{
  PangoEngineInfo *info;
  gboolean        is_exact;
};

typedef struct _PangoIncludedModule PangoIncludedModule;

struct _PangoIncludedModule
{
  void        (*list) (PangoEngineInfo **engines, int *n_engines);
  PangoEngine *(*load) (const char *id);
  void        (*unload) (PangoEngine *engine);
};

PangoMap      *pango_find_map(const char *lang, guint engine_type_id, 
                              guint render_type_id);
PangoMapEntry *pango_map_get_entry(PangoMap *map, guint32 wc);
PangoEngine   *pango_map_get_engine(PangoMap *map, guint32 wc);
void          pango_module_register(PangoIncludedModule *module);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANGO_MODULES_H__ */
