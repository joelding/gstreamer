#include <gtk/gtk.h>
#include <gst/gst.h>
#include <string.h>
//#include <gst/video/videooverlay.h>

#include <gdk/gdk.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined (GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif

#include <gst/video/video.h>

typedef struct {
	GstElement *playbin;
	GtkWidget *slider;              /* Slider widget to keep track of current position */
	GtkWidget *streams_list;        /* Text widget to display info about the streams */
	gulong slider_update_signal_id; /* Signal ID for the slider update signal */

	GstState state;                 /* Current state of the pipeline */
	gint64 duration;
} graph_t;

/* This function is called when the GUI toolkit creates the physical window that will hold the video.
 * At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
 * and pass it to GStreamer through the VideoOverlay interface. */
//static 
void realize_cb (GtkWidget *widget, graph_t *graph) {
	GdkWindow *window = gtk_widget_get_window (widget);
	guintptr window_handle;

	if (!gdk_window_ensure_native (window))
		g_error ("Couldn't create native window needed for GstVideoOverlay!");

	/* Retrieve window handler from GDK */
#if defined (GDK_WINDOWING_WIN32)
	window_handle = (guintptr)GDK_WINDOW_HWND (window);
#elif defined (GDK_WINDOWING_QUARTZ)
	window_handle = gdk_quartz_window_get_nsview (window);
#elif defined (GDK_WINDOWING_X11)
	window_handle = GDK_WINDOW_XID (window);
#endif
	/* Pass it to playbin, which implements VideoOverlay and will forward it to the video sink */
	gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (graph->playbin), window_handle);
}

/* metadata found in the stream */
static void tags_changed(GstElement *playbin, gint stream, graph_t *graph)
{
	gst_element_post_message(playbin, 
				 gst_message_new_application(GST_OBJECT(playbin),
							     gst_structure_new_empty("tags-changed")));
}

//static 
void destroy_event(GtkWidget *widget, 
		GdkEvent *event,
		graph_t *graph)
{
	gst_element_set_state(graph->playbin, GST_STATE_READY);
	gtk_main_quit();
}

/* This function is called when the PLAY button is clicked */
//static 
void play_cb (GtkButton *button, graph_t *graph) 
{
	gst_element_set_state (graph->playbin, GST_STATE_PLAYING);
}

/* This function is called when the PAUSE button is clicked */
//static 
void pause_cb (GtkButton *button, graph_t *graph) 
{
	gst_element_set_state (graph->playbin, GST_STATE_PAUSED);
}

/* This function is called when the STOP button is clicked */
//static 
void stop_cb (GtkButton *button, graph_t *graph) 
{
	gst_element_set_state (graph->playbin, GST_STATE_READY);
}
/* This function is called when the slider changes its position. We perform a seek to the
 * new position here. */
//static 
void slider_cb (GtkRange *range, graph_t *graph) 
{
	gdouble value = gtk_range_get_value (GTK_RANGE (graph->slider));
	gst_element_seek_simple (graph->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
			(gint64)(value * GST_SECOND));
}

static gboolean refresh_ui(graph_t *graph)
{
	gint64 current = -1;
	
	/* We do not want to update anything unless we are in the PAUSED or PLAYING states */
	if (graph->state < GST_STATE_PAUSED)
		return TRUE;
	
	/* If we didn't know it yet, query the stream duration */
	if (!GST_CLOCK_TIME_IS_VALID(graph->duration)) {
		if (!gst_element_query_duration (graph->playbin, GST_FORMAT_TIME, &graph->duration)) {
			g_printerr ("Could not query current duration.\n");
		} else {
			/* Set the range of the slider to the clip duration, in SECONDS */
			gtk_range_set_range(GTK_RANGE(graph->slider), 0, (gdouble)graph->duration / GST_SECOND);
		}
	}

	if (gst_element_query_position (graph->playbin, GST_FORMAT_TIME, &current)) {
		/* Block the "value-changed" signal, so the slider_cb function is not called
		 * (which would trigger a seek the user has not requested) */
		g_signal_handler_block(graph->slider, graph->slider_update_signal_id);
		/* Set the position of the slider to the current pipeline positoin, in SECONDS */
		gtk_range_set_value(GTK_RANGE(graph->slider), (gdouble)current / GST_SECOND);
		/* Re-enable the signal */
		g_signal_handler_unblock (graph->slider, graph->slider_update_signal_id);
	}
	
	return TRUE;
}

/* This function is called everytime the video window needs to be redrawn (due to damage/exposure,
 * rescaling, etc). GStreamer takes care of this in the PAUSED and PLAYING states, otherwise,
 * we simply draw a black rectangle to avoid garbage showing up. */
//static 
gboolean draw_cb (GtkWidget *widget, cairo_t *cr, graph_t *graph) 
{
	if (graph->state < GST_STATE_PAUSED) {
		GtkAllocation allocation;

		/* Cairo is a 2D graphics library which we use here to clean the video window.
		 * It is used by GStreamer for other reasons, so it will always be available to us. */
		gtk_widget_get_allocation (widget, &allocation);
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
		cairo_fill (cr);
	}

	return FALSE;
}

static void create_ui(graph_t *graph)
{
	GtkWidget *main_window;  /* The uppermost window, containing all other windows */
	GtkWidget *video_window; /* The drawing area where the video will be shown */
	GtkWidget *main_box;     /* VBox to hold main_hbox and the controls */
	GtkWidget *main_hbox;    /* HBox to hold the video_window and the stream info text widget */
	GtkWidget *controls;     /* HBox to hold the buttons and the slider */
	GtkWidget *play_button, *pause_button, *stop_button; /* Buttons */

	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(main_window), "Hello World!");

	g_signal_connect(G_OBJECT(main_window), "destroy", //"delete-event", 
			 G_CALLBACK(destroy_event), graph);

	video_window = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(video_window, FALSE);
	g_signal_connect (video_window, "realize", G_CALLBACK (realize_cb), graph);
	g_signal_connect (video_window, "draw", G_CALLBACK (draw_cb), graph);

	play_button = gtk_button_new_from_icon_name ("media-playback-start", GTK_ICON_SIZE_SMALL_TOOLBAR);
	g_signal_connect (G_OBJECT (play_button), "clicked", G_CALLBACK (play_cb), graph);

	pause_button = gtk_button_new_from_icon_name ("media-playback-pause", GTK_ICON_SIZE_SMALL_TOOLBAR);
	g_signal_connect (G_OBJECT (pause_button), "clicked", G_CALLBACK (pause_cb), graph);

	stop_button = gtk_button_new_from_icon_name ("media-playback-stop", GTK_ICON_SIZE_SMALL_TOOLBAR);
	g_signal_connect (G_OBJECT (stop_button), "clicked", G_CALLBACK (stop_cb), graph);

	graph->slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
	gtk_scale_set_draw_value (GTK_SCALE (graph->slider), 0);
	graph->slider_update_signal_id = g_signal_connect (G_OBJECT (graph->slider), "value-changed", G_CALLBACK (slider_cb), graph);

	graph->streams_list = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (graph->streams_list), FALSE);

	controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (controls), play_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (controls), pause_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (controls), stop_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (controls), graph->slider, TRUE, TRUE, 2);

	main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (main_hbox), video_window, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (main_hbox), graph->streams_list, FALSE, FALSE, 2);

	main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (main_box), main_hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (main_box), controls, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (main_window), main_box);
	gtk_window_set_default_size (GTK_WINDOW (main_window), 640, 480);
	gtk_widget_show(main_window);

}

static void error_callback(GstBus *bus, 
			   GstMessage *msg, 
			   graph_t *graph)
{
	GError *err;
	gchar *debug_info;

	gst_message_parse_error(msg, &err, &debug_info);
	g_printerr("Element %s complained %s\n", GST_OBJECT_NAME(msg->src), err->message);
	g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");
	g_clear_error(&err);
	g_free(debug_info);

	gst_element_set_state(graph->playbin, GST_STATE_READY);

}

static void eos_callback(GstBus *bus, 
			 GstMessage *msg, 
			 graph_t *graph)
{
	g_print("End of stream\n");
	gst_element_set_state(graph->playbin, GST_STATE_READY);
}

static void status_changed_callback(GstBus *bus, 
				    GstMessage *msg, 
				    graph_t *graph)
{
	GstState old_state, new_state, pending_state;

	gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
	if (GST_MESSAGE_SRC(msg) == GST_OBJECT(graph->playbin)) {
		g_print("Pipeline change from %s to %s\n", 
			gst_element_state_get_name(old_state),
			gst_element_state_get_name(new_state));
		if ((old_state == GST_STATE_READY) &&
		    (new_state == GST_STATE_PAUSED))
			refresh_ui(graph);
	}
}

static void analyze_streams(graph_t *graph)
{
}

static void application_callback(GstBus *bus, 
				 GstMessage *msg, 
				 graph_t *graph)
{
	if (g_strcmp0(gst_structure_get_name(gst_message_get_structure(msg)), "tags-changed") == 0) {
		analyze_streams(graph);
	}
}

int main(int argc, char *argv[])
{
	graph_t graph;
	gchar *pathtofile, *currdir;
	GstStateChangeReturn retval;
	GstBus *bus;

	if (argc != 2) {
		g_printerr("Usage: %s <path/to/file>\n", argv[0]);
		return -1;
	}

	/* Initialize GStreamer */
	gst_init(&argc, &argv);

	/* Initialize GTK */
	gtk_init(&argc, &argv);

	/* Initialize data structure */
	memset(&graph, 0, sizeof(graph_t));
	graph.duration = GST_CLOCK_TIME_NONE;

	/* Create elements */
	graph.playbin = gst_element_factory_make("playbin", "playbin");
	if (!graph.playbin) {
		g_printerr("fail to create elements\n");
		return -1;
	}

	/* Set uri to play */
	currdir = g_get_current_dir();
	pathtofile = g_strconcat("file:///", currdir, "/", argv[1], NULL);
	g_object_set(graph.playbin, "uri", pathtofile, NULL);
	g_free(pathtofile);
	g_free(currdir);

	g_signal_connect(G_OBJECT(graph.playbin), "video-tags-changed", 
			 (GCallback)tags_changed, &graph);
	g_signal_connect(G_OBJECT(graph.playbin), "audio-tags-changed", 
			 (GCallback)tags_changed, &graph);
	g_signal_connect(G_OBJECT(graph.playbin), "text-tags-changed", 
			 (GCallback)tags_changed, &graph);

	create_ui(&graph);

	
	/* Listen to the bus */
	bus = gst_element_get_bus(graph.playbin);
	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message::error", (GCallback) error_callback, &graph);
	g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback) eos_callback, &graph);
	g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback) status_changed_callback, &graph);
	g_signal_connect(G_OBJECT(bus), "message::application", (GCallback) application_callback, &graph);
	gst_object_unref(bus);

	/* Start playing */
	retval = gst_element_set_state(graph.playbin, GST_STATE_PLAYING);
	if (retval == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Fail to change state\n");
		g_object_unref(graph.playbin);
		return -1;
	}

	/* Register a function that GLib calls every second */	
	g_timeout_add_seconds(1, (GSourceFunc) refresh_ui, &graph);

	/* Start the GTK main loop */
	gtk_main();
	
	gst_element_set_state(graph.playbin, GST_STATE_NULL);
	gst_object_unref(graph.playbin);
	
	return 0;
}

