/*
 * xrdp_login_wnd.h
 *
 *  Created on: Aug 9, 2015
 *      Author: maillard
 */

#ifndef XRDP_LOGIN_WND_H_
#define XRDP_LOGIN_WND_H_

int APP_CC xrdp_wm_show_edits(struct xrdp_wm *self, struct xrdp_bitmap *combo);
int APP_CC xrdp_wm_cancel_clicked(struct xrdp_bitmap *wnd);
int APP_CC xrdp_wm_ok_clicked(struct xrdp_bitmap *wnd);
int APP_CC xrdp_wm_help_clicked(struct xrdp_bitmap *wnd);

#endif /* XRDP_LOGIN_WND_H_ */
