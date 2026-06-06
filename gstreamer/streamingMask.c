#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <stdint.h>

static gboolean
bus_call(GstBus *bus,
         GstMessage *msg,
         gpointer data)
{
  GMainLoop *loop = (GMainLoop *)data;

  switch (GST_MESSAGE_TYPE(msg))
  {

  case GST_MESSAGE_EOS:
    g_print("End of stream\n");
    g_main_loop_quit(loop);
    break;

  case GST_MESSAGE_ERROR:
  {
    gchar *debug;
    GError *error;

    gst_message_parse_error(msg, &error, &debug);
    g_free(debug);

    g_printerr("Error: %s\n", error->message);
    g_error_free(error);

    g_main_loop_quit(loop);
    break;
  }
  default:
    break;
  }

  return TRUE;
}

static GstFlowReturn on_new_sample(GstAppSink *appsink, gpointer user_data)
{
  // g_print("New sample received\n");
  GstSample *sample = gst_app_sink_pull_sample(appsink);
  if (!sample)
    return GST_FLOW_ERROR;

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  GstMapInfo map;

  GstCaps *caps = gst_sample_get_caps(sample);
  GstStructure *s = gst_caps_get_structure(caps, 0);

  int width = 0, height = 0;
  gst_structure_get_int(s, "width", &width);
  gst_structure_get_int(s, "height", &height);

  if (gst_buffer_map(buffer, &map, GST_MAP_READWRITE))
  {
      unsigned char *data = map.data;
      int pixels = width * height;

      for (int i = 0; i < pixels; i++)
      {
          int idx = i * 3;

          unsigned char r = data[idx];
          unsigned char g = data[idx + 1];
          unsigned char b = data[idx + 2];

          // simple green detection
          int is_green = (g > 180 && r < 100 && b < 100);

          if (!is_green)
          {
              data[idx]     = 0;
              data[idx + 1] = 0;
              data[idx + 2] = 0;
          }
      }
      gst_buffer_unmap(buffer, &map);
  }
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

static GstElement* create_output_pipeline()
{
    GstElement *pipeline = gst_pipeline_new("output");

    appsrc_global = gst_element_factory_make("appsrc", "source");
    GstElement *conv = gst_element_factory_make("videoconvert", NULL);
    GstElement *enc  = gst_element_factory_make("x264enc", NULL);
    GstElement *pay  = gst_element_factory_make("rtph264pay", NULL);
    GstElement *sink = gst_element_factory_make("udpsink", NULL);

    if (!pipeline || !appsrc_global || !conv || !enc || !pay || !sink)
        return NULL;

    // IMPORTANT: caps for raw RGB input
    GstCaps *caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB",
        "width", G_TYPE_INT, WIDTH,
        "height", G_TYPE_INT, HEIGHT,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        NULL);

    g_object_set(appsrc_global,
        "caps", caps,
        "format", GST_FORMAT_TIME,
        "is-live", TRUE,
        "block", TRUE,
        NULL);

    gst_caps_unref(caps);

    // UDP config
    g_object_set(sink,
        "host", "127.0.0.1",
        "port", 5000,
        "sync", FALSE,
        NULL);

    // encoding tuning (optional but useful)
    g_object_set(enc,
        "tune", 0x00000004,   // zerolatency
        "speed-preset", 1,
        NULL);

    gst_bin_add_many(GST_BIN(pipeline),
        appsrc_global, conv, enc, pay, sink, NULL);

    gst_element_link_many(
        appsrc_global, conv, enc, pay, sink, NULL);

    return pipeline;
}

int main(int argc, char *argv[])
{
  /* Initialisation */
  gst_init(&argc, &argv);

  /* Check input arguments */
  if (argc != 2){
    g_printerr("Usage: %s <Ogg/Vorbis filename>\n", argv[0]);
    return -1;
  }

  GstElement *input = create_input_pipeline();
  GstElement *output = create_output_pipeline();

  /* we add a message handler */
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  guint bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
  gst_object_unref(bus);

  
  /* Set the pipeline to "playing" state*/
  g_print("Now playing: %s\n", argv[1]);
  gst_element_set_state(output, GST_STATE_PLAYING);
  gst_element_set_state(input, GST_STATE_PLAYING);

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  /* Iterate */
  g_print("Running...\n");
  g_main_loop_run(loop);

  /* Out of the main loop, clean up nicely */
  g_print("Returned, stopping playback\n");
  gst_element_set_state(input, GST_STATE_NULL);
  gst_element_set_state(output, GST_STATE_NULL);
  g_print("Deleting pipeline\n");
  gst_object_unref(input);
  gst_object_unref(output);
  g_source_remove(bus_watch_id);
  g_main_loop_unref(loop);

  return 0;
}