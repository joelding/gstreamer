#include <gst/gst.h>

typedef struct {
	GstElement *playbin;
	gboolean playing;
	gboolean terminate;
	gboolean seek_enabled;
	gboolean seek_done;
	gint64 duration;
} graph_t;

static void handle_message(graph_t *graph, GstMessage *msg)
{
	GError *err;
	gchar *debug_info;

	switch (GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &err, &debug_info);
			g_printerr("Element %s complained %s\n", GST_OBJECT_NAME(msg->src), err->message);
			g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");
			g_clear_error(&err);
			g_free(debug_info);
			graph->terminate = TRUE;
			break;
		case GST_MESSAGE_EOS:
			g_print("End of stream\n");
			graph->terminate = TRUE;
			break;
		case GST_MESSAGE_STATE_CHANGED: 
			{
					GstState old_state, new_state, pending_state;

					gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
					if (GST_MESSAGE_SRC(msg) == GST_OBJECT(graph->playbin)) {
							g_print("Pipeline change from %s to %s\n", 
									gst_element_state_get_name(old_state),
									gst_element_state_get_name(new_state));

							graph->playing = (new_state == GST_STATE_PLAYING);

							if (graph->playing) {
									gint64 start, end;
									GstQuery *query = gst_query_new_seeking(GST_FORMAT_TIME);

									if (gst_element_query(graph->playbin, query)) {
											/* Check if query is possible */
											gst_query_parse_seeking(query, NULL, &graph->seek_enabled, &start, &end);
											if (graph->seek_enabled) {
													g_print("Seeking is enabled from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT"\n",
															GST_TIME_ARGS(start), GST_TIME_ARGS(end));
											} else {
													g_print("Seeking is disabled\n");
											}
									} else {
											g_printerr("Fail to query for seeking");
									}

									gst_query_unref(query);
							}
					}
			}
			break;
		case GST_MESSAGE_DURATION:
			/* The duration has changed, mark the current one as invalid */
			graph->duration = GST_CLOCK_TIME_NONE;
			break;
		default:
			g_printerr("Received unexpected message\n");
			break;
	}

	gst_message_unref(msg);
}


int main(int argc, char *argv[])
{
	graph_t graph;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn retval;

	graph.playing = FALSE;
	graph.terminate = FALSE;

	/* Initialize GStreamer */
	gst_init(&argc, &argv);

	/* Create elements */
	graph.playbin = gst_element_factory_make("playbin", "playbin");
	if (!graph.playbin) {
		g_printerr("fail to create elements\n");
		return -1;
	}

	/* Set source's properties */
	g_object_set(graph.playbin, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);

	/* Start playing */
	retval = gst_element_set_state(graph.playbin, GST_STATE_PLAYING);
	if (retval == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Fail to change state\n");
		g_object_unref(graph.playbin);
		return -1;
	}

	/* Listen to the bus */
	bus = gst_element_get_bus(graph.playbin);

	do {
		msg = gst_bus_timed_pop_filtered(bus, 
						 GST_CLOCK_TIME_NONE, 
						 GST_MESSAGE_ERROR | GST_MESSAGE_EOS | 
						 GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_DURATION);

		if (NULL != msg) {
				handle_message(&graph, msg);
		} else { /* got no message, the timeout expired */
				if (graph.playing) {
						gint64 current = -1;

						if (!gst_element_query_position(graph.playbin, GST_FORMAT_TIME, &current)) {
								g_printerr("Fail to query current duration\n");
						}

						g_print("Position %" GST_TIME_FORMAT "/%" GST_TIME_FORMAT "\n", 
								GST_TIME_ARGS(current), GST_TIME_ARGS(graph.duration));

						if (graph.seek_enabled && !graph.seek_done && (current > (10 * GST_SECOND))) {
								g_print("Reached 10s, performing seek...\n");
								gst_element_seek_simple(graph.playbin, GST_FORMAT_TIME, 
														GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 30 * GST_SECOND);
								graph.seek_done = TRUE;
						}
				}
		}
	} while (!graph.terminate);

	gst_object_unref(bus);
	gst_element_set_state(graph.playbin, GST_STATE_NULL);
	gst_object_unref(graph.playbin);

	return 0;
}
