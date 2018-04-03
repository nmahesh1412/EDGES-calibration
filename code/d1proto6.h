#include <gtk/gtk.h>
double tosecs (int, int, int, int, int);
void toyrday (double, int *, int *, int *, int *, int *);
char *to_date (int, int);
int dayofyear (int, int, int);
void plotbox (void);
double readclock (void);
void plotspec (int);
void plotp (double);
int disp(void);
gint Repaint (double);
void cleararea (void);
void quit (void);
void vquit (void);
gint expose_event (GtkWidget *, GdkEventExpose *);
gint configure_event (GtkWidget *);
gint vexpose_event (GtkWidget *, GdkEventExpose *);
gint vconfigure_event (GtkWidget *);
void button_exit_clicked (void);
gint button_press_event (GtkWidget *,GdkEventButton *);

void clearpaint (void);
void vclearpaint (void);


