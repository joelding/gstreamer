#include <gst/gst.h>

typedef struct {
	GstElement *pipeline;
	GstElement *source;
	GstElement *sink;
	GstElement *convert;
} graph_t;

static void pad_added_handler(GstElement *src,
			      GstPad *pad, 
			      graph_t *args)
{
	GstPadLinkReturn retval;
	GstCaps *caps = NULL;
	GstStructure *st = NULL;
	const gchar *type = NULL;
	GstPad *sink_pad = gst_element_get_static_pad(args->convert, "sink");

	g_print("Receive new pad %s from %s\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(src));

	if (gst_pad_is_linked(sink_pad)) {
		g_print("Alread linked. Ignore!\n");
		goto exit;
	}

	caps = gst_pad_query_caps(pad, NULL);
	st = gst_caps_get_structure(caps, 0);
	type = gst_structure_get_name(st);
	if (!g_str_has_prefix(type, "audio/x-raw")) {
		g_print("Has type '%s', not raw audio. Ignore!\n", type);
		goto exit;
	}

	retval = gst_pad_link(pad, sink_pad);
	if (GST_PAD_LINK_FAILED(retval)) {
		g_print("Fail to link type '%s'", type);
	} else {
		g_print("Succeed to link type '%s'", type);
	}

exit:
	if (caps)
		gst_caps_unref(caps);

	gst_object_unref(sink_pad);
}

int main(int argc, char *argv[])
{
	graph_t graph;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn retval;
	GError *err;
	gchar *debug_info;
	gboolean terminate = FALSE;

	/* Initialize GStreamer */
	gst_init(&argc, &argv);

	/* Create elements */
	graph.source = gst_element_factory_make("uridecodebin", "source");
	graph.convert = gst_element_factory_make("audioconvert", "convert");
	graph.sink = gst_element_factory_make("autoaudiosink", "sink");
	graph.pipeline = gst_pipeline_new("test-pipeline"); 
	if ((!graph.source) || (!graph.sink) || (!graph.convert) || (!graph.pipeline)) {
		g_printerr("fail to create elements\n");
		return -1;
	}

	/* Create an empty pipeline */
	gst_bin_add_many(GST_BIN(graph.pipeline), graph.source, graph.convert, graph.sink, NULL);
	if (gst_element_link(graph.convert, graph.sink) != TRUE) {
		g_printerr("Fail to link\n");
		g_object_unref(graph.pipeline);
		return -1;
	}

	/* Set source's properties */
	g_object_set(graph.source, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);

	/* Connect pad-added signal */
	g_signal_connect(graph.source, "pad-added", G_CALLBACK(pad_added_handler), &graph);

	/* Start playing */
	retval = gst_element_set_state(graph.pipeline, GST_STATE_PLAYING);
	if (retval == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Fail to change state\n");
		g_object_unref(graph.pipeline);
		return -1;
	}

	bus = gst_element_get_bus(graph.pipeline);

	do {
		msg = gst_bus_timed_pop_filtered(bus, 
						 GST_CLOCK_TIME_NONE, 
						 GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED);

		if (NULL != msg) {
			switch (GST_MESSAGE_TYPE(msg)) {
				case GST_MESSAGE_ERROR:
					gst_message_parse_error(msg, &err, &debug_info);
					g_printerr("Element %s complained %s\n", GST_OBJECT_NAME(msg->src), err->message);
					g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");
					g_clear_error(&err);
					g_free(debug_info);
					terminate = TRUE;
					break;
				case GST_MESSAGE_EOS:
					g_print("End of stream\n");
					terminate = TRUE;
					break;
				case GST_MESSAGE_STATE_CHANGED:
					if (GST_MESSAGE_SRC(msg) == GST_OBJECT(graph.pipeline)) {
						GstState old_state, new_state, pending_state;
						gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
						g_print("Pipeline change from %s to %s\n", 
							gst_element_state_get_name(old_state),
							gst_element_state_get_name(new_state));
					}
					break;
				default:
					g_printerr("Received unexpected message\n");
					break;
			}
			gst_message_unref(msg);
		}
	} while (!terminate);

	gst_object_unref(bus);
	gst_element_set_state(graph.pipeline, GST_STATE_NULL);
	gst_object_unref(graph.pipeline);

	return 0;
}
