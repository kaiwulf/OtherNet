#include <gtk/gtk.h>

/* gcc -o program program.c `pkg-config --cflags --libs gtk4` */

static gboolean animate = TRUE;
static int rectangle_x = 50;
static int rectangle_y = 50;
static int rectangle_width = 200;
static int rectangle_height = 100;
static int step = 5;

static void on_draw(GtkDrawingArea* drawing_area, cairo_t* cr, int width, int height, gpointer data) {
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_rectangle(cr, rectangle_x, rectangle_y, rectangle_width, rectangle_height);
    cairo_fill(cr);
}

static gboolean update_animation(gpointer data) {
    GtkDrawingArea *drawing_area = GTK_DRAWING_AREA(data);
    rectangle_x += step;
    if( rectangle_x <= 0 || rectangle_x >= gtk_widget_get_allocated_width(GTK_WIDGET(drawing_area)) - rectangle_width)
        step = -step;

    gtk_widget_queue_draw(GTK_WIDGET(drawing_area));
    return animate;
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "MTHL");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), on_draw, NULL, NULL);
    gtk_window_set_child(GTK_WINDOW(window), drawing_area);

    g_timeout_add(50, update_animation, drawing_area);

    gtk_window_present(GTK_WINDOW(window));
}

static void on_shutdown(GtkApplication *app, gpointer user_data) {
    animate = FALSE;
}

int main(int argc, char** argv) {
    GtkApplication *app = gtk_application_new("org.othernet.mhhl", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}