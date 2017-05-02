#include <gtk/gtk.h>
static gboolean time_handler(GtkWidget *widget);
static char buffer[256];


int main( int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *kickU3;
  GtkWidget *takeAway;
  GtkWidget *normal;
  GtkWidget *pause;
  GtkWidget *beamON;
  GtkWidget *beamOFF;
  GtkWidget *liteON;
  GtkWidget *mtcON;
  GtkWidget *vbox;
  time_t time0,curtime;
  int ii = 0;
  
  gtk_init(&argc, &argv);

  if (ii == 0){
    ii=1;
    time0=time(NULL);
  }
 
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_title(GTK_WINDOW(window), "Testing");
  gtk_window_set_default_size(GTK_WINDOW(window), 350, 400);

  kickU3 = gtk_label_new("Kick-u3 status");
  takeAway = gtk_label_new("TakeAway Mode");
  normal = gtk_label_new("Normal Mode");
  pause = gtk_label_new("Pause On");
  beamON = gtk_label_new("Beam On");
  beamOFF = gtk_label_new("Beam Off");
  liteON = gtk_label_new("Laser On");
  mtcON = gtk_label_new("MTC On");

  //  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
  //  gtk_container_add(GTK_CONTAINER(window), label);
  //  gtk_container_add(GTK_CONTAINER(window), takeAway);
  curtime=time(NULL);
  vbox = gtk_box_new(TRUE, 1);
  gtk_container_add(GTK_CONTAINER(window), vbox);
  gtk_box_pack_start(GTK_BOX(vbox), kickU3, TRUE, TRUE, 0);
  if ((curtime - time0) > 10) gtk_box_pack_start(GTK_BOX(vbox), takeAway, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), normal, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), pause, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), beamON, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), beamOFF, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), liteON, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), mtcON, TRUE, TRUE, 0);

  
  //  g_signal_connect(G_OBJECT(button), "clicked", 
  //     G_CALLBACK(gtk_main_quit), G_OBJECT(window));
   g_signal_connect_swapped(window, "destroy",
      G_CALLBACK (gtk_main_quit), NULL);
  g_timeout_add(1000, (GSourceFunc) time_handler, (gpointer) window);
  gtk_widget_show_all(window);
  time_handler(window);
  gtk_main();

  return 0;
}


static gboolean
time_handler(GtkWidget *widget)
{
  //  if (widget->window == NULL) return FALSE;
  if (gtk_widget_get_window(widget) == NULL) return FALSE;

  time_t curtime;
  struct tm *loctime;
 
  curtime = time(NULL);
  loctime = localtime(&curtime);
  strftime(buffer, 256, "%T", loctime);
 
  gtk_widget_queue_draw(widget);
  return TRUE;
}
