#ifndef _AK_AUDIO_CONFIG_H_
#include "ak_common_audio.h"
/* set ai param */
struct ak_audio_nr_attr default_ai_nr_attr ={-25, 0, 1};
struct ak_audio_agc_attr default_ai_agc_attr = {16384, 4, 0, 40, 0, 1};
struct ak_audio_aec_attr default_ai_aec_attr = {0, 1024, 1024, 0, 512, 1, 6553};
struct ak_audio_aslc_attr default_ai_aslc_attr = {3276, 10, 0};
struct ak_audio_eq_attr default_ai_eq_attr = {
0,
0,
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1},
0,
0,
0,
0,
0,
0,
0,
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

/* set ao param */
struct ak_audio_nr_attr default_ao_nr_attr = {-25, 0, 1};
struct ak_audio_aslc_attr default_ao_aslc_attr = {16384, 8, 0};
/* gain */
 
int default_ai_gain = 3;
 
/*			### how to set ai gain ###
ak_ai_set_gain(ai_handle_id,default_ai_gain);*/
 
int default_ao_gain = 2;
 
/*			### how to set ao gain ###
ak_ao_set_gain(ao_handle_id,default_ao_gain);*/
 
struct ak_audio_eq_attr default_ao_eq_attr = {
1024,
2,
{4800, 4500, 0, 0, 0, 0, 0, 0, 0, 0},
{-12288, -2048, 0, 0, 0, 0, 0, 0, 0, 0},
{614, 716, 0, 0, 0, 0, 0, 0, 0, 0},
{TYPE_HSF, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1},
0,
0,
0,
0,
0,
0,
1,
{1, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
#endif
