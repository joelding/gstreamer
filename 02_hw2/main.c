#include <gst/gst.h>

int main(int argc, char *argv[])
{
	GstElement *pipeline, *source, *sink;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn retval;
	GError *err;
	gchar *debug_info;

	/* Initialize GStreamer */
	gst_init(&argc, &argv);

	/* Create elements */
	source = gst_element_factory_make("videotestsrc", "source");
	sink = gst_element_factory_make("autovideosink", "sink");

	/* Create an empty pipeline */
	pipeline = gst_pipeline_new("test-pipeline"); 

	if ((!source) || (!sink) || (!pipeline)) {
		g_printerr("fail to create elements\n");
		return -1;
	}

	/* Build the pipeline */
	gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
	if (gst_element_link(source, sink) != TRUE) {
		g_printerr("Fail to link\n");
		g_object_unref(pipeline);
		return -1;
	}

	/* Set source property */
	g_object_set(source, "pattern", 0, NULL);

	/* Start playing */
	retval = gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if (retval == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Fail to change state\n");
		g_object_unref(pipeline);
		return -1;
	}

	bus = gst_element_get_bus(pipeline);
	msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

	/* parse message */
	if (NULL != msg) {
		switch (GST_MESSAGE_TYPE(msg)) {
			case GST_MESSAGE_ERROR:
				gst_message_parse_error(msg, &err, &debug_info);
				g_printerr("Element %s complained %s\n", GST_OBJECT_NAME(msg->src), err->message);
				g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");
				g_clear_error(&err);
				g_free(debug_info);
				break;
			case GST_MESSAGE_EOS:
				g_print("Reached end of stream\n");
				break;
			default:
				g_printerr("Received unexpected message\n");
				break;
		}
		gst_message_unref(msg);
	}

	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);

	return 0;
}
