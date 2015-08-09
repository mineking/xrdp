/*
 * xrdp_wm.h
 *
 *  Created on: Aug 9, 2015
 *      Author: maillard
 */

#ifndef XRDP_WM_H_
#define XRDP_WM_H_

#define LOGIN_MODE_INITIALISING_LOGIN_WINDOW 0
#define LOGIN_MODE_LOGIN_WINDOW_READY 1

#define LOGIN_MODE_CONNECTING_TO_DISPLAY 2
#define LOGIN_MODE_CONNECTING_TO_DISPLAY_WITH_NEW_SESSION 20
#define LOGIN_MODE_CONNECTED_TO_DISPLAY 3

#define LOGIN_MODE_CHECKING_SESSION 4
#define LOGIN_MODE_CHECKING_SESSION_SENT 5

#define LOGIN_MODE_INITIALISING_SESSION_WINDOW 6
#define LOGIN_MODE_SESSION_WINDOW_READY 7

#define LOGIN_MODE_CLOSING 10
#define LOGIN_MODE_CLOSED 11

int DEFAULT_CC xrdp_wm_notify(struct xrdp_bitmap *wnd, struct xrdp_bitmap *sender, int msg, long param1, long param2);

#endif /* XRDP_WM_H_ */
