#ifndef _LINUX_CONSOLE_DECOR_H_
#define _LINUX_CONSOLE_DECOR_H_ 1

/* A structure used by the framebuffer console decorations (drivers/video/console/fbcondecor.c) */
struct vc_decor {
 __u8 bg_color;        /* The color that is to be treated as transparent */
 __u8 state;       /* Current decor state: 0 = off, 1 = on */
 __u16 tx, ty;       /* Top left corner coordinates of the text field */
 __u16 twidth, theight;      /* Width and height of the text field */
 char* theme;
};

#endif

