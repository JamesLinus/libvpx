/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/common/vp9_entropymv.h"

#define MV_COUNT_SAT 20
#define MV_MAX_UPDATE_FACTOR 128

// Integer pel reference mv threshold for use of high-precision 1/8 mv
#define COMPANDED_MVREF_THRESH 8

const vp9_tree_index vp9_mv_joint_tree[TREE_SIZE(MV_JOINTS)] = {
  -MV_JOINT_ZERO, 2,
  -MV_JOINT_HNZVZ, 4,
  -MV_JOINT_HZVNZ, -MV_JOINT_HNZVNZ
};

const vp9_tree_index vp9_mv_class_tree[TREE_SIZE(MV_CLASSES)] = {
  -MV_CLASS_0, 2,
  -MV_CLASS_1, 4,
  6, 8,
  -MV_CLASS_2, -MV_CLASS_3,
  10, 12,
  -MV_CLASS_4, -MV_CLASS_5,
  -MV_CLASS_6, 14,
  16, 18,
  -MV_CLASS_7, -MV_CLASS_8,
  -MV_CLASS_9, -MV_CLASS_10,
};

const vp9_tree_index vp9_mv_class0_tree[TREE_SIZE(CLASS0_SIZE)] = {
  -0, -1,
};

const vp9_tree_index vp9_mv_fp_tree[TREE_SIZE(MV_FP_SIZE)] = {
  -0, 2,
  -1, 4,
  -2, -3
};

static const nmv_context default_nmv_context = {
  {32, 64, 96},
  {
    { // Vertical component
      128,                                                  // sign
      {224, 144, 192, 168, 192, 176, 192, 198, 198, 245},   // class
      {216},                                                // class0
      {136, 140, 148, 160, 176, 192, 224, 234, 234, 240},   // bits
      {{128, 128, 64}, {96, 112, 64}},                      // class0_fp
      {64, 96, 64},                                         // fp
      160,                                                  // class0_hp bit
      128,                                                  // hp
    },
    { // Horizontal component
      128,                                                  // sign
      {216, 128, 176, 160, 176, 176, 192, 198, 198, 208},   // class
      {208},                                                // class0
      {136, 140, 148, 160, 176, 192, 224, 234, 234, 240},   // bits
      {{128, 128, 64}, {96, 112, 64}},                      // class0_fp
      {64, 96, 64},                                         // fp
      160,                                                  // class0_hp bit
      128,                                                  // hp
    }
  },
};

static const uint8_t log_in_base_2[] = {
  0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10
};

#if CONFIG_GLOBAL_MOTION
const vp9_tree_index vp9_global_motion_types_tree
          [TREE_SIZE(GLOBAL_MOTION_TYPES)] = {
  -GLOBAL_ZERO, 2,
  -GLOBAL_TRANSLATION, -GLOBAL_ROTZOOM
};

static const vp9_prob default_global_motion_types_prob
                 [GLOBAL_MOTION_TYPES - 1] = {
  // Currently only translation is used, so make the second prob very high.
  240, 255
};

static void convert_params_to_rotzoom(double *H, Global_Motion_Params *model) {
  double z = 1.0 + (double) model->zoom / (1 << ZOOM_PRECISION_BITS);
  double r = (double) model->rotation / (1 << ROTATION_PRECISION_BITS);
  H[0] =  (1 + z) * cos(r * M_PI / 180.0);
  H[1] = -(1 + z) * sin(r * M_PI / 180.0);
  H[2] = (double) model->mv.as_mv.col / 8.0;
  H[3] = (double) model->mv.as_mv.row / 8.0;
}

static int_mv get_global_mv(int col, int row, Global_Motion_Params *model) {
  int_mv mv;
  double H[4];
  double x, y;
  convert_params_to_rotzoom(H, model);
  x =  H[0] * col + H[1] * row + H[2];
  y = -H[1] * col + H[0] * row + H[3];
  mv.as_mv.col = (int)floor(x * 8 + 0.5) - col;
  mv.as_mv.row = (int)floor(y * 8 + 0.5) - row;
  return mv;
}

int_mv vp9_get_global_sb_center_mv(int col, int row, BLOCK_SIZE bsize,
                                   Global_Motion_Params *model) {
  col += num_4x4_blocks_wide_lookup[bsize] * 2;
  row += num_4x4_blocks_high_lookup[bsize] * 2;
  return get_global_mv(col, row, model);
}

int_mv vp9_get_global_sub8x8_center_mv(int col, int row, int block,
                                       Global_Motion_Params *model) {
  if (block == 0 || block == 2)
    col += 2;
  else
    col += 6;
  if (block == 0 || block == 1)
    row += 2;
  else
    row += 6;
  return get_global_mv(col, row, model);
}
#endif  // CONFIG_GLOBAL_MOTION

static INLINE int mv_class_base(MV_CLASS_TYPE c) {
  return c ? CLASS0_SIZE << (c + 2) : 0;
}

MV_CLASS_TYPE vp9_get_mv_class(int z, int *offset) {
  const MV_CLASS_TYPE c = (z >= CLASS0_SIZE * 4096) ?
      MV_CLASS_10 : (MV_CLASS_TYPE)log_in_base_2[z >> 3];
  if (offset)
    *offset = z - mv_class_base(c);
  return c;
}

int vp9_use_mv_hp(const MV *ref) {
  return (abs(ref->row) >> 3) < COMPANDED_MVREF_THRESH &&
         (abs(ref->col) >> 3) < COMPANDED_MVREF_THRESH;
}

int vp9_get_mv_mag(MV_CLASS_TYPE c, int offset) {
  return mv_class_base(c) + offset;
}

static void inc_mv_component(int v, nmv_component_counts *comp_counts,
                             int incr, int usehp) {
  int s, z, c, o, d, e, f;
  assert(v != 0);             /* should not be zero */
  s = v < 0;
  comp_counts->sign[s] += incr;
  z = (s ? -v : v) - 1;       /* magnitude - 1 */

  c = vp9_get_mv_class(z, &o);
  comp_counts->classes[c] += incr;

  d = (o >> 3);               /* int mv data */
  f = (o >> 1) & 3;           /* fractional pel mv data */
  e = (o & 1);                /* high precision mv data */

  if (c == MV_CLASS_0) {
    comp_counts->class0[d] += incr;
    comp_counts->class0_fp[d][f] += incr;
    comp_counts->class0_hp[e] += usehp * incr;
  } else {
    int i;
    int b = c + CLASS0_BITS - 1;  // number of bits
    for (i = 0; i < b; ++i)
      comp_counts->bits[i][((d >> i) & 1)] += incr;
    comp_counts->fp[f] += incr;
    comp_counts->hp[e] += usehp * incr;
  }
}

void vp9_inc_mv(const MV *mv, nmv_context_counts *counts) {
  if (counts != NULL) {
    const MV_JOINT_TYPE j = vp9_get_mv_joint(mv);
    ++counts->joints[j];

    if (mv_joint_vertical(j)) {
      inc_mv_component(mv->row, &counts->comps[0], 1, 1);
    }

    if (mv_joint_horizontal(j)) {
      inc_mv_component(mv->col, &counts->comps[1], 1, 1);
    }
  }
}

static vp9_prob adapt_prob(vp9_prob prep, const unsigned int ct[2]) {
  return merge_probs(prep, ct, MV_COUNT_SAT, MV_MAX_UPDATE_FACTOR);
}

static void adapt_probs(const vp9_tree_index *tree, const vp9_prob *pre_probs,
                        const unsigned int *counts, vp9_prob *probs) {
  vp9_tree_merge_probs(tree, pre_probs, counts, MV_COUNT_SAT,
                       MV_MAX_UPDATE_FACTOR, probs);
}

void vp9_adapt_mv_probs(VP9_COMMON *cm, int allow_hp) {
  int i, j;

  nmv_context *fc = &cm->fc.nmvc;
  const nmv_context *pre_fc = &cm->frame_contexts[cm->frame_context_idx].nmvc;
  const nmv_context_counts *counts = &cm->counts.mv;

  adapt_probs(vp9_mv_joint_tree, pre_fc->joints, counts->joints, fc->joints);

  for (i = 0; i < 2; ++i) {
    nmv_component *comp = &fc->comps[i];
    const nmv_component *pre_comp = &pre_fc->comps[i];
    const nmv_component_counts *c = &counts->comps[i];

    comp->sign = adapt_prob(pre_comp->sign, c->sign);
    adapt_probs(vp9_mv_class_tree, pre_comp->classes, c->classes,
                comp->classes);
    adapt_probs(vp9_mv_class0_tree, pre_comp->class0, c->class0, comp->class0);

    for (j = 0; j < MV_OFFSET_BITS; ++j)
      comp->bits[j] = adapt_prob(pre_comp->bits[j], c->bits[j]);

    for (j = 0; j < CLASS0_SIZE; ++j)
      adapt_probs(vp9_mv_fp_tree, pre_comp->class0_fp[j], c->class0_fp[j],
                  comp->class0_fp[j]);

    adapt_probs(vp9_mv_fp_tree, pre_comp->fp, c->fp, comp->fp);

    if (allow_hp) {
      comp->class0_hp = adapt_prob(pre_comp->class0_hp, c->class0_hp);
      comp->hp = adapt_prob(pre_comp->hp, c->hp);
    }
  }
}

void vp9_init_mv_probs(VP9_COMMON *cm) {
  cm->fc.nmvc = default_nmv_context;
#if CONFIG_INTRABC
  cm->fc.ndvc = default_nmv_context;
#endif  // CONFIG_INTRABC
#if CONFIG_GLOBAL_MOTION
  vp9_copy(cm->fc.global_motion_types_prob, default_global_motion_types_prob);
#endif  // CONFIG_GLOBAL_MOTION
}
