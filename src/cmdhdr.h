/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2022 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: December 2022 */

/* This header file contains a list of functions for obeying E commands. They
have to be global so that they can be inserted into a vector. */

extern int e_abandon(cmdstr *);
extern int e_abe(cmdstr *);
extern int e_actongroup(cmdstr *);
extern int e_attn(cmdstr *);
extern int e_autoalign(cmdstr *);
extern int e_back(cmdstr *);
extern int e_backregion(cmdstr *);
extern int e_backup(cmdstr *);
extern int e_beginpar(cmdstr *);
extern int e_break(cmdstr *);
extern int e_buffer(cmdstr *);
extern int e_c(cmdstr *);
extern int e_casematch(cmdstr *);
extern int e_cdbuffer(cmdstr *);
extern int e_centre(cmdstr *);
extern int e_cl(cmdstr *);
extern int e_comment(cmdstr *);
extern int e_copy(cmdstr *);
extern int e_cproc(cmdstr *);
extern int e_csd(cmdstr *);
extern int e_csu(cmdstr *);
extern int e_cut(cmdstr *);
extern int e_cutstyle(cmdstr *);
extern int e_dbuffer(cmdstr *);
extern int e_dcut(cmdstr *);
extern int e_debug(cmdstr *);
extern int e_detrail(cmdstr *);
extern int e_df(cmdstr *);
extern int e_dmarked(cmdstr *);
extern int e_drest(cmdstr *);
extern int e_dtab(cmdstr *);
extern int e_dtwl(cmdstr *);
extern int e_dtwr(cmdstr *);
extern int e_eightbit(cmdstr *);
extern int e_endpar(cmdstr *);
extern int e_f(cmdstr *);
extern int e_fks(cmdstr *);
extern int e_format(cmdstr *);
extern int e_front(cmdstr *);
extern int e_g(cmdstr *);
extern int e_i(cmdstr *);
extern int e_icurrent(cmdstr *);
extern int e_if(cmdstr *);
extern int e_iline(cmdstr *);
extern int e_ispace(cmdstr *);
extern int e_key(cmdstr *);
extern int e_lcl(cmdstr *);
extern int e_load(cmdstr *);
extern int e_loop(cmdstr *);
extern int e_m(cmdstr *);
extern int e_makebuffer(cmdstr *);
extern int e_mark(cmdstr *);
extern int e_mouse(cmdstr *);
extern int e_n(cmdstr *);
extern int e_name(cmdstr *);
extern int e_newbuffer(cmdstr *);
extern int e_overstrike(cmdstr *);
extern int e_p(cmdstr *);
extern int e_pab(cmdstr *);
extern int e_paste(cmdstr *);
extern int e_plllr(cmdstr *);
extern int e_proc(cmdstr *);
extern int e_prompt(cmdstr *);
extern int e_readonly(cmdstr *);
extern int e_refresh(cmdstr *);
extern int e_renumber(cmdstr *);
extern int e_repeat(cmdstr *);
extern int e_rmargin(cmdstr *);
extern int e_sab(cmdstr *);
extern int e_save(cmdstr *);
extern int e_set(cmdstr *);
extern int e_show(cmdstr *);
extern int e_stop(cmdstr *);
extern int e_subchar(cmdstr *);
extern int e_ttl(cmdstr *);
extern int e_title(cmdstr *);
extern int e_topline(cmdstr *);
extern int e_ucl(cmdstr *);
extern int e_undelete(cmdstr *);
extern int e_unformat(cmdstr *);
extern int e_verify(cmdstr *);
extern int e_w(cmdstr *);
extern int e_warn(cmdstr *);
extern int e_wide(cmdstr *);
extern int e_word(cmdstr *);
extern int e_while(cmdstr *);
extern int e_write(cmdstr *);

extern int e_singlechar(cmdstr *);
extern int e_star(cmdstr *);
extern int e_sequence(cmdstr *);
extern int e_obeyproc(cmdstr *);

/* End of cmdhdr.h */
