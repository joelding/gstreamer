#include <gtk/gtk.h>
#include <gst/gst.h>
#include <string.h>

typedef struct {
	GstElement *playbin;
	GstState state;
	gint64 duration;
} graph_t;

/* metadata found in the stream */
static void tags_changed(GstElement *playbin, gint stream, graph_t *graph)
{
	gst_element_post_message(playbin, 
				 gst_message_new_application(GST_OBJECT(playbin),
							     gst_structure_new_empty("tags-changed")));
}

static void destroy_event(GtkWidget *widget, 
			  GdkEvent *event,
			  graph_t *graph)
{
	gst_element_set_state(graph->playbin, GST_STATE_READY);
	gtk_main_quit();
}

static gboolean refresh_ui(graph_t *graph)
{
	gint64 current = -1;

	if (graph->state < GST_STATE_PAUSED)
		return TRUE;
		
	return TRUE;
}

static void create_ui(graph_t *graph)
{
	GtkWidget *video_window,
		  *main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_title(GTK_WINDOW(window), "Hello World!");

	g_signal_connect(GTK_OBJECT(window), "destroy", //"delete-event", 
			 G_CALLBACK(destroy_event), graph);

	video_window = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(video_window, FALSE);
	g_signal_connect (video_window, "realize", G_CALLBACK (realize_cb), data);
	g_signal_connect (video_window, "draw", G_CALLBACK (draw_cb), data);

	play_button = gtk_button_new_from_icon_name ("media-playback-start", GTK_ICON_SIZE_SMALL_TOOLBAR);
	g_signal_connect (G_OBJECT (play_button), "clicked", G_CALLBACK (play_cb), data);

	pause_button = gtk_button_new_from_icon_name ("media-playback-pause", GTK_ICON_SIZE_SMALL_TOOLBAR);
	g_signal_connect (G_OBJECT (pause_button), "clicked", G_CALLBACK (pause_cb), data);

	stop_button = gtk_button_new_from_icon_name ("media-playback-stop", GTK_ICON_SIZE_SMALL_TOOLBAR);
	g_signal_connect (G_OBJECT (stop_button), "clicked", G_CALLBACK (stop_cb), data);

	data->slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
	gtk_scale_set_draw_value (GTK_SCALE (data->slider), 0);
	data->slider_update_signal_id = g_signal_connect (G_OBJECT (data->slider), "value-changed", G_CALLBACK (slider_cb), data);

	data->streams_list = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (data->streams_list), FALSE);

	controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (controls), play_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (controls), pause_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (controls), stop_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (controls), data->slider, TRUE, TRUE, 2);

	main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (main_hbox), video_window, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (main_hbox), data->streams_list, FALSE, FALSE, 2);

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

