#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "d1typ6.h"
#include "d1glob6.h"
#include "d1proto6.h"
#define NUMBUTTONS 1

int disp(void)
{
    GtkWidget *window;
    GdkGeometry geometry;
    GdkWindowHints geo_mask;

//        gtk_init(&argc, &argv);
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        geometry.min_width = 500;
        geometry.min_height = 300;
        geo_mask = GDK_HINT_MIN_SIZE;
        gtk_window_set_geometry_hints(GTK_WINDOW(window), window, &geometry, geo_mask);
        //Table size determines number of buttons across the top
        table = gtk_table_new(30, NUMBUTTONS, TRUE);
        drawing_area = gtk_drawing_area_new();
        gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
        gtk_widget_show(drawing_area);
        gtk_table_attach_defaults(GTK_TABLE(table), drawing_area, 0, NUMBUTTONS, 3, 30);

        g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(quit), NULL);

        gtk_container_add(GTK_CONTAINER(window), table);

        g_signal_connect(G_OBJECT(drawing_area), "expose_event", (GtkSignalFunc) expose_event, NULL);
        g_signal_connect(G_OBJECT(drawing_area), "configure_event", (GtkSignalFunc) configure_event, NULL);


        gtk_widget_set_events(drawing_area, GDK_EXPOSURE_MASK
                              | GDK_LEAVE_NOTIFY_MASK
                              | GDK_BUTTON_PRESS_MASK
                              | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);


        button_exit = gtk_button_new_with_label("exit");

        g_signal_connect(G_OBJECT(button_exit), "clicked", G_CALLBACK(button_exit_clicked), NULL);

        gtk_table_attach(GTK_TABLE(table), button_exit, 0, 1, 0, 2, GTK_FILL, GTK_FILL, 0, 0);

        gtk_widget_show(button_exit);

        gtk_widget_show(table);
        gtk_widget_show(window);
        clearpaint();
    return 0;
}

void button_exit_clicked(void)
{
  d1.run=0;
}

void clearpaint(void)
{
//  GtkWidget *drawing_area = (GtkWidget *) data;
    GdkRectangle update_rect;
    gdk_draw_rectangle(pixmap,
                       drawing_area->style->white_gc, TRUE, 0, 0,
                       drawing_area->allocation.width, drawing_area->allocation.height);
    update_rect.x = 0;
    update_rect.y = 0;
    update_rect.width = drawing_area->allocation.width;
    update_rect.height = drawing_area->allocation.height;
    midx = drawing_area->allocation.width / 2;
    midy = drawing_area->allocation.height / 2;
    gtk_widget_draw(drawing_area, &update_rect);
//  gtk_widget_queue_draw_area(drawing_area,update_rect.x,update_rect.y,update_rect.width,update_rect.height);
}

gint configure_event(GtkWidget * widget)
{
    int newp = 0;
    int sz = 140;
    char txt[80];
    GtkSettings *settings;
//    if (!pixmap)
        newp = 1;
// if (pixmap && newp)  printf("new pixmap\n");   // only occurs on resize
//    if (pixmap && newp)
//        gdk_pixmap_unref(pixmap);
    if (newp) {

        sz = (widget->allocation.width / 72) * 10; // font = 110 with wid = 800  - works best over range of size
//  printf("sz %d wid %d %d %d\n",sz,widget->allocation.width,midx,drawing_area->allocation.width);
        if (sz > 140)
            sz = 140;
        sprintf(txt, "-misc-fixed-medium-r-*-*-*-%d-*-*-*-*-iso8859-*", sz); // specified iso to avoid 16-bit encode
// look into difference between  gdk_font_load and  gdk_fontset_load
//  printf("%s\n",txt);
//  sprintf(txt,"-adobe-helvetica-bold-r-normal--12-120-75-75-p-70-iso8859-1");
//  sprintf(txt,"-adobe-helvetica-medium-r-normal--*-%d-*-*-*-*-iso8859-1",sz);
        fixed_font = gdk_font_load(txt);
        settings = gtk_settings_get_default();
        sz = sz / 11;
        if (sz < 1)
            sz = 1;
        sprintf(txt, "Sans %d", sz);
        g_object_set(G_OBJECT(settings), "gtk-font-name", txt, NULL);
//  gtk_table_resize(GTK_TABLE(table), 10, 12);
//  gtk_table_set_row_spacing(GTK_TABLE(table),0,20);

        pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
        gdk_draw_rectangle(pixmap, drawing_area->style->white_gc, TRUE, 0, 0,
                           widget->allocation.width, widget->allocation.height);
        clearpaint();
    }
    return TRUE;
}

gint expose_event(GtkWidget * widget, GdkEventExpose * event)
{
    gdk_draw_pixmap(widget->window,
                    widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                    pixmap,
                    event->area.x, event->area.y,
                    event->area.x, event->area.y, event->area.width, event->area.height);
//  printf("draw pixmap\n");   // the above actually draws to screen
    return FALSE;
}

void quit()
{
    gtk_exit(0);
}
