#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "d1typ6.h"
#include "d1proto6.h"
#include "d1glob6.h"


gint Repaint(double mfreq)
{
    GdkRectangle update_rect;
    char txt[80];
    int i, j, x, y, yr, da, hr, mn, sc, ix, iy;
    int x1, y1, x2, y2, iax, iay, scaledb, nspec;
    double secs, c, max;

    nspec = d1.nspec;
    scaledb = 100;
    iax = 20; iay = 4;
    x1 = y1 = 0;
    midx = drawing_area->allocation.width / 2;
    midy = drawing_area->allocation.height / 2;
    gdk_draw_line(pixmap, drawing_area->style->black_gc, iax, midy * 2 - iay, midx * 2 - iax, midy * 2 - iay);
    gdk_draw_line(pixmap, drawing_area->style->black_gc, iax, iay, midx * 2 - iax, iay);
    gdk_draw_line(pixmap, drawing_area->style->black_gc, iax , midy * 2 - iay, iax, iay);
    gdk_draw_line(pixmap, drawing_area->style->black_gc, midx * 2 - iax, midy * 2 - iay, midx * 2 - iax, iay);
    for (i = 0; i < nspec-(nspec/20); i += (nspec/20)) {
        x = i * (midx - 20) / (nspec/2) + iax;
        gdk_draw_line(pixmap, drawing_area->style->black_gc, x, midy * 2 - iay -2, x, midy * 2 - iay);
        if (i % 1638 == 0) {
                sprintf(txt, "%3d", i*(int)(mfreq*0.1)/(nspec/10));
            gdk_draw_text(pixmap, fixed_font, drawing_area->style->black_gc,
                          x - 8, midy * 2 - 8 - iay, txt, strlen(txt));
        }
    }
    for (i = 0; i < scaledb; i += 10) {
        y = (int)(midy * 2 - i * midy * 1.8 / scaledb - iay);
        gdk_draw_line(pixmap, drawing_area->style->black_gc, iax, y, iax + 2, y);
        if (i % 10 == 0) {
            sprintf(txt, "%02d", i);
            gdk_draw_text(pixmap, fixed_font, drawing_area->style->black_gc, 1, y + 5, txt, strlen(txt));
        }
    }

    secs = d1.secs;
    toyrday(secs, &yr, &da, &hr, &mn, &sc);
    sprintf(txt, "%4d:%03d:%02d:%02d:%02d", yr, da, hr, mn, sc);
    ix = (int)(midx * 1.65);
    iy = (int)(midy * 0.15);
    gdk_draw_text(pixmap, fixed_font, drawing_area->style->black_gc, ix, iy, txt, strlen(txt));
            max = scaledb;
            for (i = 0; i < nspec; i++) {
                    j = i;
                    x2 = iax + i * (midx - iax) * 2 / nspec;

                     c = avspec[j]+40.0;

                    y2 = (int)(midy * 2 - c * midy * 1.8 / max  - iax);

                if (i > 1)
                    gdk_draw_line(pixmap, drawing_area->style->black_gc, x1, y1, x2, y2);
                x1 = x2;
                y1 = y2;
            }

    sprintf(txt, "temp %3.0f C", d1.temp);
    iy = (int)(midy * 0.20);
    gdk_draw_text(pixmap, fixed_font, drawing_area->style->black_gc, ix, iy, txt, strlen(txt));
    sprintf(txt, "mode %d max %6.4f", d1.mode,d1.adcmax);
    iy = (int)(midy * 0.25);
    gdk_draw_text(pixmap, fixed_font, drawing_area->style->black_gc, ix, iy, txt, strlen(txt));
    sprintf(txt, "totpwr %5.1e", d1.totp);
    iy = (int)(midy * 0.30);
    gdk_draw_text(pixmap, fixed_font, drawing_area->style->black_gc, ix, iy, txt, strlen(txt));
    sprintf(txt, "file: %s", d1.filname);
    iy = (int)(midy * 0.35);
        gdk_draw_text(pixmap, fixed_font, drawing_area->style->black_gc,
                      midx, iy, txt, strlen(txt));
    update_rect.x = update_rect.y = 0;
    update_rect.width = drawing_area->allocation.width;
    update_rect.height = drawing_area->allocation.height;
//  printf("draw %d %d %d %d \n",update_rect.x,update_rect.y,update_rect.width,update_rect.height);
    gtk_widget_draw(drawing_area, &update_rect);
//  gtk_widget_queue_draw_area(drawing_area, update_rect.x,update_rect.y,update_rect.width,update_rect.height);

    return 1;
}
