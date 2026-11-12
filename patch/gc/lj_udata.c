/*
** Userdata handling.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_udata_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_meta.h"
#include "lj_udata.h"

GCudata *lj_udata_new(lua_State *L, MSize sz, GCtab *env)
{
  GCudata *ud = lj_mem_newt(L, sizeof(GCudata) + sz, GCudata);
  global_State *g = G(L);
  newwhite(g, ud);  /* Not finalized. */
  ud->gct = ~LJ_TUDATA;
  ud->udtype = UDTYPE_USERDATA;
  ud->len = sz;
  /* NOBARRIER: The GCudata is new (marked white). */
  setgcrefnull(ud->metatable);
  setgcref(ud->env, obj2gco(env));
  /* Chain to userdata list (after main thread). */
  setgcrefr(ud->nextgc, mainthread(g)->nextgc);
  setgcref(mainthread(g)->nextgc, obj2gco(ud));
  return ud;
}

void LJ_FASTCALL lj_udata_free(global_State *g, GCudata *ud)
{
  /* Has an un-run __gc? Divert to the finalizer queue instead of freeing (cf. lj_cdata_free).
  ** Userdata are swept before tables (GCSsweepudata), so the metatable is not freed yet here.
  ** A live metatable survives the table sweep by itself.
  ** lj_gc_freeall sweeps tables first, so on shutdown the metatable may already be freed. */
  GCtab *mt = tabref(ud->metatable);
  /* A metatable that is itself dead (nothing but dying userdata held it) died with everything it refers to, its __gc
  ** function included, and those are freed later in this sweep (strings already were). Reviving the table alone would
  ** call a freed finalizer, so such a userdata is freed as if it had none. OpenMW's metatables with a __gc live in the registry and
  ** are never dead here; a script's newproxy(true) with its own metatable is. */
  if (mt && !(g->gc.currentwhite & LJ_GC_SFIXED) && !(ud->marked & LJ_GC_FINALIZED) &&
      !isdead(g, obj2gco(mt)) && lj_meta_fastg(g, mt, MM_gc)) {
    GCobj *o = obj2gco(ud);
    GCobj *root = gcref(g->gc.mmudata);
    makewhite(g, o);
    markfinalized(o);
    if (root != NULL) {
      setgcrefr(ud->nextgc, root->gch.nextgc);
      setgcref(root->gch.nextgc, o);
      setgcref(g->gc.mmudata, o);
    } else {
      setgcref(ud->nextgc, o);
      setgcref(g->gc.mmudata, o);
    }
    return;
  }
  lj_mem_free(g, ud, sizeudata(ud));
}

#if LJ_64
void *lj_lightud_intern(lua_State *L, void *p)
{
  global_State *g = G(L);
  uint64_t u = (uint64_t)p;
  uint32_t up = lightudup(u);
  uint32_t *segmap = mref(g->gc.lightudseg, uint32_t);
  MSize segnum = g->gc.lightudnum;
  if (segmap) {
    MSize seg;
    for (seg = 0; seg <= segnum; seg++)
      if (segmap[seg] == up)  /* Fast path. */
	return (void *)(((uint64_t)seg << LJ_LIGHTUD_BITS_LO) | lightudlo(u));
    segnum++;
    /* Leave last segment unused to avoid clash with ITERN key. */
    if (segnum >= (1 << LJ_LIGHTUD_BITS_SEG)-1) lj_err_msg(L, LJ_ERR_BADLU);
  }
  if (!((segnum-1) & segnum) && segnum != 1) {
    lj_mem_reallocvec(L, segmap, segnum, segnum ? 2*segnum : 2u, uint32_t);
    setmref(g->gc.lightudseg, segmap);
  }
  g->gc.lightudnum = segnum;
  segmap[segnum] = up;
  return (void *)(((uint64_t)segnum << LJ_LIGHTUD_BITS_LO) | lightudlo(u));
}
#endif

