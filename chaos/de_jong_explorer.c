/*
 * de Jong Explorer - interactive exploration of the Peter de Jong attractor
 *
 * Copyright (C) 2004 David Trowbridge and Micah Dowty
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

struct {
  double x,y;
} point;

/* Convert a gray level from 0 to 255 to an ARGB color in little endian.
 * This macro can be redefined to colorize the image differently.
 */
#define GRAY_TO_RGBA(gray) (GUINT32_TO_LE( gray | (gray<<8) | (gray<<16) | 0xFF000000UL ))

GtkWidget *window, *drawing_area, *iterl;
GtkWidget *as, *bs, *cs, *ds, *ls, *zs, *xos, *yos;
GtkWidget *start, *stop, *save, *randbutton;
double iterations;
GdkGC *gc;
guint *counts;
int width, height;
guint countsMax;
guchar *pixels;
double a, b, c, d, exposure, zoom, xoffset, yoffset;
guint idler;

void update_gui();
void update_pixels();
void clear();
void usage();
void resize(int w, int h);
void set_defaults();
float get_pixel_scale();
void update_drawing_area();
void load_parameters_from_file(const char *name);
void interactive_main(int argc, char **argv);
void render_main(const char *filename, guint targetDensity);
void save_to_file(const char *name);
void run_iterations(int count);
int interactive_idle_handler(gpointer user_data);
gboolean expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
void param_spinner_changed(GtkWidget *widget, gpointer user_data);
void exposure_changed(GtkWidget *widget, gpointer user_data);
void startclick(GtkWidget *widget, gpointer user_data);
void stopclick(GtkWidget *widget, gpointer user_data);
void saveclick(GtkWidget *widget, gpointer user_data);
void randomclick(GtkWidget *widget, gpointer user_data);
gboolean deletee(GtkWidget *widget, GdkEvent *event, gpointer user_data);
GtkWidget *build_sidebar();


int main(int argc, char ** argv) {
  enum {INTERACTIVE, RENDER} mode = INTERACTIVE;
  const char *outputFile;
  int opt;
  char *cptr;
  guint targetDensity = 5000;

  srand(time(NULL));
  g_type_init();
  set_defaults();

  while (1) {
    opt = getopt(argc, argv, "a:b:c:d:x:y:z:s:e:o:i:t:h");
    if (opt == -1)
      break;

    switch (opt) {
    case 'a':  a        = atof(optarg);  break;
    case 'b':  b        = atof(optarg);  break;
    case 'c':  c        = atof(optarg);  break;
    case 'd':  d        = atof(optarg);  break;
    case 'e':  exposure = atof(optarg);  break;
    case 'x':  xoffset  = atof(optarg);  break;
    case 'y':  yoffset  = atof(optarg);  break;
    case 'z':  zoom     = atof(optarg);  break;

    case 's':
      /* A single number can be given to make a square image,
       * or a size in WIDTHxHEIGHT form can be given.
       */
      width = strtol(optarg, &cptr, 10);
      if (*cptr == 'x')
	height = atoi(cptr+1);
      else
	height = width;
      break;

    case 'i':
      load_parameters_from_file(optarg);
      break;

    case 't':
      targetDensity = atol(optarg);
      break;

    case 'o':
      mode = RENDER;
      outputFile = optarg;
      break;

    default:
      usage(argv, targetDensity);
      return 1;
    }
  }

  if (optind < argc) {
    usage(argv, targetDensity);
    return 1;
  }

  resize(width, height);

  switch (mode) {

  case INTERACTIVE:
    interactive_main(argc, argv);
    break;

  case RENDER:
    render_main(outputFile, targetDensity);
    break;
  }

  return 0;
}

void usage(const char **argv, guint defaultDensity) {
  printf("Usage: %s [options]\n"
	 "Interactive exploration of the Peter de Jong attractor\n"
	 "\n"
	 "Parameters:\n"
	 "  -a VALUE           Set the 'a' parameter [%f]\n"
	 "  -b VALUE           Set the 'b' parameter [%f]\n"
	 "  -c VALUE           Set the 'c' parameter [%f]\n"
	 "  -d VALUE           Set the 'd' parameter [%f]\n"
	 "  -e EXPOSURE        Set the image exposure [%f]\n"
	 "  -x OFFSET          Set the X offest [%f]\n"
	 "  -y OFFSET          Set the Y offset [%f]\n"
	 "  -z ZOOM            Set the zoom factor [%f]\n"
	 "\n"
	 "Quality:\n"
	 "  -s WIDTH[xHEIGHT]  Set the image size in pixels. If only one value is\n"
	 "                       given, a square image is produced [%d]\n"
	 "  -t DENSITY         In noninteractive rendering, set the count\n"
	 "                       density to stop rendering at. Larger numbers give\n"
	 "                       smoother and more detailed results, but increase\n"
	 "                       running time linearly [%d]\n"
	 "\n"
	 "Actions:\n"
	 "  -i FILE            Load all parameters from the tEXt chunk of any\n"
	 "                       .png image file generated by this program.\n"
	 "  -o FILE            Instead of presenting an interactive GUI, render\n"
	 "                       an image with the provided settings and write it\n"
	 "                       in PNG format to FILE.\n",
	 argv[0], a, b, c, d, exposure, xoffset, yoffset, zoom,
	 width, defaultDensity);
}

void interactive_main(int argc, char ** argv) {
  /* After common initialization code needed whether or not we're running
   * interactively, this takes over to provide the gtk UI for playing with
   * the de jong attractor in mostly-real-time. Yay.
   */
  GtkWidget *hbox, *vsep;
  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(deletee), NULL);
  drawing_area = gtk_drawing_area_new();
  vsep = gtk_vseparator_new();
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), build_sidebar(), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), vsep, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), drawing_area, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), hbox);
  g_signal_connect(G_OBJECT(drawing_area), "expose-event", G_CALLBACK(expose), NULL);
  gtk_widget_show_all(window);

  gc = gdk_gc_new(drawing_area->window);
  gtk_widget_set_size_request(drawing_area, width, height);
  update_gui();

  idler = g_idle_add(interactive_idle_handler, NULL);
  gtk_main();
}

void render_main(const char *filename, guint targetDensity) {
  /* Main function for noninteractive rendering. This renders an image with the
   * current settings until countsMax reaches the provided targetDensity.
   * We show helpful progress doodads on stdout while the poor user has to wait.
   */
  time_t start_time, now, elapsed, remaining;
  start_time = time(NULL);

  while (countsMax < targetDensity) {
    run_iterations(1000000);

    /* This should be a fairly accurate time estimate, since countsMax increases linearly */
    now = time(NULL);
    elapsed = now - start_time;
    remaining = ((float)elapsed) * targetDensity / countsMax - elapsed;

    /* After each batch of iterations, show the percent completion, number
     * of iterations (in scientific notation), iterations per second,
     * density / target density, and elapsed time / remaining time.
     */
    if (elapsed > 0) {
      printf("%6.02f%%   %.3e   %.2e/sec %6d / %d   %02d:%02d:%02d / %02d:%02d:%02d\n",
	     100.0 * countsMax / targetDensity,
	     iterations, iterations / elapsed, countsMax, targetDensity,
	     elapsed / (60*60), (elapsed / 60) % 60, elapsed % 60,
	     remaining / (60*60), (remaining / 60) % 60, remaining % 60);
    }
  }

  printf("Creating image...\n");
  save_to_file(filename);
}

void set_defaults() {
  a = 1.4191403;
  b = -2.2841323;
  c = 2.4275403;
  d = -2.177196;
  exposure = 0.05;
  zoom = 1;
  xoffset = 0;
  yoffset = 0;
  width = 800;
  height = 800;
}

GtkWidget *build_sidebar() {
  GtkWidget *table;

  table = gtk_table_new(11, 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(table), 6);
  gtk_table_set_col_spacings(GTK_TABLE(table), 6);

  /* Labels */
  {
    GtkWidget *label;

    label = gtk_label_new("a:");
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    label = gtk_label_new("b:");
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    label = gtk_label_new("c:");
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    label = gtk_label_new("d:");
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    label = gtk_label_new("zoom:");
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    label = gtk_label_new("xoffset:");
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    label = gtk_label_new("yoffset:");
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 6, 7, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    label = gtk_label_new("exposure:");
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 7, 8, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
  }

  /* Spin buttons */
  {
    as = gtk_spin_button_new_with_range(-9.999, 9.999, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(as), a);
    gtk_table_attach(GTK_TABLE(table), as, 1, 2, 0, 1, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
    g_signal_connect(G_OBJECT(as), "changed", G_CALLBACK(param_spinner_changed), NULL);

    bs = gtk_spin_button_new_with_range(-9.999, 9.999, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(bs), b);
    gtk_table_attach(GTK_TABLE(table), bs, 1, 2, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
    g_signal_connect(G_OBJECT(bs), "changed", G_CALLBACK(param_spinner_changed), NULL);

    cs = gtk_spin_button_new_with_range(-9.999, 9.999, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cs), c);
    gtk_table_attach(GTK_TABLE(table), cs, 1, 2, 2, 3, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
    g_signal_connect(G_OBJECT(cs), "changed", G_CALLBACK(param_spinner_changed), NULL);

    ds = gtk_spin_button_new_with_range(-9.999, 9.999, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ds), d);
    gtk_table_attach(GTK_TABLE(table), ds, 1, 2, 3, 4, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
    g_signal_connect(G_OBJECT(ds), "changed", G_CALLBACK(param_spinner_changed), NULL);

    zs = gtk_spin_button_new_with_range(0.01, 100, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(zs), zoom);
    gtk_table_attach(GTK_TABLE(table), zs, 1, 2, 4, 5, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
    g_signal_connect(G_OBJECT(zs), "changed", G_CALLBACK(param_spinner_changed), NULL);

    xos = gtk_spin_button_new_with_range(-1.999, 1.999, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(xos), xoffset);
    gtk_table_attach(GTK_TABLE(table), xos, 1, 2, 5, 6, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
    g_signal_connect(G_OBJECT(xos), "changed", G_CALLBACK(param_spinner_changed), NULL);

    yos = gtk_spin_button_new_with_range(-1.999, 1.999, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(yos), yoffset);
    gtk_table_attach(GTK_TABLE(table), yos, 1, 2, 6, 7, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
    g_signal_connect(G_OBJECT(yos), "changed", G_CALLBACK(param_spinner_changed), NULL);

    ls = gtk_spin_button_new_with_range(0.001, 9.999, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ls), exposure);
    gtk_table_attach(GTK_TABLE(table), ls, 1, 2, 7, 8, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
    g_signal_connect(G_OBJECT(ls), "changed", G_CALLBACK(exposure_changed), NULL);
  }

  /* Iteration counter */
  iterl = gtk_label_new("");
  gtk_table_attach(GTK_TABLE(table), iterl, 0, 2, 8, 9, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

  /* Buttons */
  {
    start = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    gtk_widget_set_sensitive(start, FALSE);
    g_signal_connect(G_OBJECT(start), "clicked", G_CALLBACK(startclick), NULL);
    gtk_table_attach(GTK_TABLE(table), start, 0, 2, 9, 10, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    stop = gtk_button_new_from_stock(GTK_STOCK_STOP);
    g_signal_connect(G_OBJECT(stop), "clicked", G_CALLBACK(stopclick), NULL);
    gtk_table_attach(GTK_TABLE(table), stop, 0, 2, 10, 11, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    save = gtk_button_new_from_stock(GTK_STOCK_SAVE);
    g_signal_connect(G_OBJECT(save), "clicked", G_CALLBACK(saveclick), NULL);
    gtk_table_attach(GTK_TABLE(table), save, 0, 2, 11, 12, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);

    randbutton = gtk_button_new_with_label("Random");
    g_signal_connect(G_OBJECT(randbutton), "clicked", G_CALLBACK(randomclick), NULL);
    gtk_table_attach(GTK_TABLE(table), randbutton, 0, 2, 12, 13, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) 0, 6, 0);
  }

  return table;
}

void resize(int w, int h) {
  width = w;
  height = h;

  if (counts)
    g_free(counts);
  counts = g_malloc(sizeof(counts[0]) * width * height);

  if (pixels)
    g_free(pixels);
  pixels = g_malloc(4 * width * height);

  clear();
}

int limit_update_rate(float max_rate) {
  /* Limit the frame rate to the given value. This should be called once per
   * frame, and will return 0 if it's alright to render another frame, or 1
   * otherwise.
   */
  static struct timeval last_update;
  struct timeval now;
  gulong diff;

  /* Figure out how much time has passed, in milliseconds */
  gettimeofday(&now, NULL);
  diff = ((now.tv_usec - last_update.tv_usec) / 1000 +
	  (now.tv_sec  - last_update.tv_sec ) * 1000);

  if (diff < (1000 / max_rate)) {
    return 1;
  }
  else {
    last_update = now;
    return 0;
  }
}

int auto_limit_update_rate(void) {
  /* Automatically determine a good maximum frame rate based on the current
   * number of iterations, and use limit_update_rate() to limit us to that.
   * Returns 1 if a frame should not be rendered.
   *
   * When we just start rendering an image, we want a quite high frame rate
   * (but not high enough we bog down the GUI) so the user can interactively
   * set parameters. After the rendering has been running for a while though,
   * the image changes much less and a very slow frame rate will leave more
   * CPU for calculations.
   */
  return limit_update_rate(200 / (1 + (log(iterations) - 9.21) * 4));
}

float get_pixel_scale() {
  /* Calculate the scale factor for converting count[] values to luminance
   * values between 0 and 1.
   */
  float density, fscale;

  /* Scale our counts to a luminance between 0 and 1 that gets fed through our
   * colormap[] to generate an actual gdk color. 'p' contains the number of
   * times our point has passed the current pixel.
   *
   * iterations / (width * height) gives us the average density of counts[].
   */
  density = iterations / (width * height);

  /* fscale is a floating point number that, when multiplied by a raw
   * counts[] value, gives values between 0 and 1 corresponding to full
   * white and full black.
   */
  fscale = (exposure * zoom) / density;

  /* The very first frame we render will often be very underexposed.
   * If fscale > 0.5, this makes countsclamp negative and we get incorrect
   * results. The lowest usable value of countsclamp is 1.
   */
  if (fscale > 0.5)
    fscale = 0.5;

  return fscale;
}

void fast_update_pixels(int steps) {
  /* Progressively update the pixels[] with RGBA values corresponding to the
   * raw point counts in counts[]. This uses an interlacing method where only
   * 1/steps of the image is updated each call. The entire image will be updated
   * if steps == 1.
   *
   * 'steps' MUST be a power of two!
   */
  const int rowstride = width * 4;
  const int progressive_mask = steps - 1;
  guchar *row;
  guint32 *p;
  guint32 gray, iscale;
  guint countsclamp, dval, *count_p, *count_row;
  int x, y;
  static int progressive_row = 0;
  float fscale = get_pixel_scale();

  /* This is the maximum allowed value for counts[], corresponding to full black */
  countsclamp = (int)(1 / fscale) - 1;

  /* Convert fscale to an 8:24 fixed point number that will map counts[]
   * values to gray levels between 0 and 255.
   */
  iscale = (guint32)(fscale * 0xFF000000L);

  row = pixels;
  count_row = counts;
  for (y=0; y<height; y++) {
    if ((y & progressive_mask) == progressive_row) {
      p = (guint32*) row;
      count_p = count_row;
      for (x=0; x<width; x++) {

	dval = *(count_p++);
	if (dval > countsclamp)
	  dval = countsclamp;

	gray = 255 - ((dval * iscale) >> 24);

	*(p++) = GRAY_TO_RGBA(gray);
      }
    }
    row += rowstride;
    count_row += width;
  }

  progressive_row = (progressive_row + 1) & progressive_mask;
}

void update_pixels() {
  /* A slower but higher quality method of converting counts[] to pixels[].
   * This uses floating point math, and doesn't perform any progressive rendering.
   */
  const int rowstride = width * 4;
  guchar *row;
  guint32 *p;
  guint32 gray;
  guint *count_p, *count_row;
  int x, y;
  float fscale = get_pixel_scale();
  float luma;

  row = pixels;
  count_row = counts;

  for (y=0; y<height; y++) {
    p = (guint32*) row;
    count_p = count_row;
    for (x=0; x<width; x++) {

      luma = *(count_p++) * fscale;
      if (luma > 1)
	luma = 1;

      gray = 255 - (luma * 255);
      *(p++) = GRAY_TO_RGBA(gray);
    }
    row += rowstride;
    count_row += width;
  }
}

void update_gui() {
  /* If the GUI needs updating, update it. This includes limiting the maximum
   * update rate, updating the iteration count display, and actually rendering
   * frames to the drawing area.
   */
  gchar *iters;

  if (auto_limit_update_rate())
    return;

  /* Update the iteration counter */
  iters = g_strdup_printf("Iterations:\n%.3e\n\nmax density:\n%d", iterations, countsMax);
  gtk_label_set_text(GTK_LABEL(iterl), iters);
  g_free(iters);

  /* Update our pixels[] from counts[], 1/4 of the rows at a time */
  fast_update_pixels(4);

  update_drawing_area();
}

void update_drawing_area() {
  /* Update our drawing area */
  gdk_draw_rgb_32_image(drawing_area->window, gc,
			0, 0, width, height, GDK_RGB_DITHER_NORMAL,
			pixels, width * 4);
}

void clear() {
  memset(counts, 0, width * height * sizeof(int));
  countsMax = 0;
  iterations = 0;
  point.x = ((float) rand()) / RAND_MAX;
  point.y = ((float) rand()) / RAND_MAX;
}

void run_iterations(int count) {
  double x, y;
  int i, ix, iy;
  guint *p;
  guint d;
  const double xcenter = width / 2.0;
  const double ycenter = height / 2.0;
  const double scale = xcenter / 2.5 * zoom;

  for(i=count; i; --i) {
    x = sin(a * point.y) - cos(b * point.x);
    y = sin(c * point.x) - cos(c * point.y);
    point.x = x;
    point.y = y;

    ix = (int)((x + xoffset) * scale + xcenter);
    iy = (int)((y + yoffset) * scale + ycenter);

    if (ix >= 0 && iy >= 0 && ix < width && iy < height) {
      p = counts + ix + width * iy;
      d = *p = *p + 1;
      if (d > countsMax)
	countsMax = d;
    }
  }
  iterations += count;
}

int interactive_idle_handler(gpointer user_data) {
  /* An idle handler used for interactively rendering. This runs a relatively
   * small number of iterations, then calls update_gui() to update our visible image.
   */
  run_iterations(10000);
  update_gui();
  return 1;
}

gboolean expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data) {
  update_drawing_area();
  return TRUE;
}

gboolean deletee(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  g_source_remove(idler);
  gtk_main_quit();
}

void startclick(GtkWidget *widget, gpointer user_data) {
  gtk_widget_set_sensitive(stop, TRUE);
  gtk_widget_set_sensitive(start, FALSE);
  clear();
  a = gtk_spin_button_get_value(GTK_SPIN_BUTTON(as));
  b = gtk_spin_button_get_value(GTK_SPIN_BUTTON(bs));
  c = gtk_spin_button_get_value(GTK_SPIN_BUTTON(cs));
  d = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ds));
  zoom = gtk_spin_button_get_value(GTK_SPIN_BUTTON(zs));
  xoffset = gtk_spin_button_get_value(GTK_SPIN_BUTTON(xos));
  yoffset = gtk_spin_button_get_value(GTK_SPIN_BUTTON(yos));
  idler = g_idle_add(interactive_idle_handler, NULL);
}

void stopclick(GtkWidget *widget, gpointer user_data) {
  gtk_widget_set_sensitive(stop, FALSE);
  gtk_widget_set_sensitive(start, TRUE);
  g_source_remove(idler);
}

void param_spinner_changed(GtkWidget *widget, gpointer user_data) {
  stopclick(widget, user_data);
  startclick(widget, user_data);
}

void exposure_changed(GtkWidget *widget, gpointer user_data) {
  exposure = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ls));
  fast_update_pixels(1);
  update_drawing_area();
}

float generateRandomParameter() {
  return ((float) rand()) / RAND_MAX * 12 - 6;
}

void randomclick(GtkWidget *widget, gpointer user_data) {
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(as), generateRandomParameter());
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(bs), generateRandomParameter());
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(cs), generateRandomParameter());
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(ds), generateRandomParameter());

  stopclick(widget, user_data);
  startclick(widget, user_data);
}

gchar* save_parameters() {
  /* Save the current parameters to a freshly allocated human and machine readable string */
  return g_strdup_printf("a = %f\n"
			 "b = %f\n"
			 "c = %f\n"
			 "d = %f\n"
			 "zoom = %f\n"
			 "xoffset = %f\n"
			 "yoffset = %f\n"
			 "exposure = %f\n",
			 a, b, c, d, zoom, xoffset, yoffset, exposure);
}

void load_parameters(const gchar *params) {
  /* Load all recognized parameters from a string given in the same
   * format as the one produced by save_parameters()
   */
  gchar *copy, *line, *nextline;
  gchar *key;
  float value;

  /* Make a copy of the parameters, since we'll be modifying it */
  copy = g_strdup(params);

  /* Iterate over lines... */
  line = copy;
  while (line) {
    nextline = strchr(line, '\n');
    if (nextline) {
      *nextline = '\0';
      nextline++;
    }

    /* Separate it into key and value */
    key = g_malloc(strlen(line)+1);
    if (sscanf(line, " %s = %f", key, &value) == 2) {
      printf("%s = %f", key, value);

      /* We found a valid key/value line.. see if we recognize the key */
      if (!strcmp(key, "a"))
	a = value;
      else if (!strcmp(key, "b"))
	b = value;
      else if (!strcmp(key, "c"))
	c = value;
      else if (!strcmp(key, "d"))
	d = value;
      else if (!strcmp(key, "zoom"))
	zoom = value;
      else if (!strcmp(key, "xoffset"))
	xoffset = value;
      else if (!strcmp(key, "yoffset"))
	yoffset = value;
      else if (!strcmp(key, "exposure"))
	exposure = value;
      else
	printf(" (unrecognized)");

      printf("\n");
    }
    g_free(key);
    line = nextline;
  }
  g_free(copy);
}

void load_parameters_from_file(const char *name) {
  /* Try to open the given PNG file and load parameters from it */
  const gchar *params;
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(name, NULL);
  params = gdk_pixbuf_get_option(pixbuf, "tEXt::de_jong_params");
  if (params)
    load_parameters(params);
  else
    printf("No parameters chunk found\n");
  gdk_pixbuf_unref(pixbuf);
}

void save_to_file(const char *name) {
  /* Save the current contents of pixels[] to a .PNG file */
  GdkPixbuf *pixbuf;
  gchar *params;

  /* Get a higher quality rendering */
  update_pixels();

  pixbuf = gdk_pixbuf_new_from_data(pixels, GDK_COLORSPACE_RGB, TRUE,
				    8, width, height, width*4, NULL, NULL);

  /* Save our current parameters in a tEXt chunk, using a format that
   * is both human-readable and easy to load parameters from automatically.
   */
  params = save_parameters();
  gdk_pixbuf_save(pixbuf, name, "png", NULL, "tEXt::de_jong_params", params, NULL);
  g_free(params);
  gdk_pixbuf_unref(pixbuf);
}

#if (GTK_MAJOR_VERSION > 2) || (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION >= 3)
static void update_preview(GtkFileChooser *chooser, gpointer data) {
  GtkWidget *preview;
  char *filename;
  GdkPixbuf *pixbuf;
  gboolean have_preview;

  preview = GTK_WIDGET(data);
  filename = gtk_file_chooser_get_preview_filename(chooser);

  pixbuf = gdk_pixbuf_new_from_file_at_size(filename, 128, 128, NULL);
  have_preview = (pixbuf != NULL);
  g_free(filename);

  gtk_image_set_from_pixbuf(GTK_IMAGE(preview), pixbuf);
  if(pixbuf)
    gdk_pixbuf_unref(pixbuf);
  gtk_file_chooser_set_preview_widget_active(chooser, have_preview);
}
#endif

void saveclick(GtkWidget *widget, gpointer user_data) {
  /* Sorry, saving only works with gtk 2.3's file selector for now */
#if (GTK_MAJOR_VERSION > 2) || (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION >= 3)
  GtkWidget *dialog, *preview;

  dialog = gtk_file_chooser_dialog_new("Save", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE,
  				       GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				       NULL);
  GtkFileFilter *filter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(filter, "*.png");
  gtk_file_filter_set_name(filter, "PNG Image");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

  preview = gtk_image_new();
  gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog), preview);
  g_signal_connect(dialog, "update-preview", G_CALLBACK(update_preview), preview);
  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    save_to_file(filename);
    g_free(filename);
  }
  g_object_unref(filter);
  gtk_widget_destroy(dialog);
#else
#warning "If you had gtk 2.3, you'd be able to save images"
#endif
}

/* The End */
