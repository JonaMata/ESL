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
  g_print("New sample received\n");
  GstSample *sample = gst_app_sink_pull_sample(appsink);
  if (!sample)
    return GST_FLOW_ERROR;

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  GstMapInfo map;

  if (gst_buffer_map(buffer, &map, GST_MAP_READ))
  {
    int width = 320; // Set to your frame width
    int height = 240; // Set to your frame height
    uint8_t *Y = map.data;
    uint8_t *U = Y + width * height;
    uint8_t *V = U + (width * height) / 4;
    // Process the data (e.g., apply a mask)
    

    long sum_x = 0;
    long sum_y = 0;
    long count = 0;

    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width; x++)
      {
          uint8_t Yv = Y[y * width + x];
          uint8_t Uv = U[(y/2) * (width/2) + (x/2)];
          uint8_t Vv = V[(y/2) * (width/2) + (x/2)];

          if (Yv > 40 && Uv < 110 && Vv < 110) // Example condition for a color mask (green)
          {
              sum_x += x;
              sum_y += y;
              count++;
          }
      }
    }

    if (count > 0)
    {
      double cx = (double)sum_x / count;
      double cy = (double)sum_y / count;
      g_print("Ball centre: (%f, %f)\n", cx, cy);
    }
    gst_buffer_unmap(buffer, &map);
  }
  gst_sample_unref(sample);
  return GST_FLOW_OK;
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

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);

  /* Create gstreamer elements */
  GstElement *pipeline = gst_pipeline_new("main");
  GstElement *source = gst_element_factory_make("v4l2src", NULL);
  GstElement *convert = gst_element_factory_make("videoconvert", NULL);
  GstElement *capsf = gst_element_factory_make("capsfilter", NULL);
  GstElement *tee = gst_element_factory_make("tee", "tee");
  if (!pipeline || !source || !convert || !tee || !capsf)
  {
    g_printerr("pipeline, source, convert or tee element could not be created. Exiting.\n");
    return -1;
  }
  GstCaps *caps = gst_caps_new_simple(
    "video/x-raw",
    "format", G_TYPE_STRING, "I420",
    "width", G_TYPE_INT, 320,
    "height", G_TYPE_INT, 240,
    "framerate", GST_TYPE_FRACTION, 30, 1,
    NULL);

  g_object_set(capsf, "caps", caps, NULL);
  gst_caps_unref(caps);

  /*Branch 1: Appsink*/
  GstElement *q1 = gst_element_factory_make("queue", NULL);
  GstElement *sink1 = gst_element_factory_make("appsink", NULL);
  GstAppSinkCallbacks callbacks = { NULL, NULL, on_new_sample };
  if (!q1 || !sink1)
  {
    g_printerr("q1 or sink1 element could not be created. Exiting.\n");
    return -1;
  }
  g_object_set(sink1,
    "emit-signals", TRUE,
    "sync", FALSE,
    "max-buffers", 1,
    "drop", TRUE,
    NULL);

  gst_app_sink_set_callbacks(
    GST_APP_SINK(sink1),
    &callbacks,
    NULL,
    NULL);

  /*Branch 2: UDP streaming*/
  GstElement *q2 = gst_element_factory_make("queue", NULL);
  GstElement *conv2 = gst_element_factory_make("videoconvert", NULL);
  GstElement *enc  = gst_element_factory_make("v4l2h264enc", NULL);
  // GstElement *parse = gst_element_factory_make("h264parse", NULL);
  GstElement *pay  = gst_element_factory_make("rtph264pay", NULL);
  GstElement *udp = gst_element_factory_make("udpsink", NULL);
  if (!q2 || !conv2 || !enc || !pay || !udp)
  {
    // Print which element failed to create
    if (!q2) g_printerr("q2 element could not be created. Exiting.\n");
    if (!conv2) g_printerr("conv2 element could not be created. Exiting.\n");
    if (!enc) g_printerr("enc element could not be created. Exiting.\n");
    // if (!parse) g_printerr("parse element could not be created. Exiting.\n");
    if (!pay) g_printerr("pay element could not be created. Exiting.\n");
    if (!udp) g_printerr("udp element could not be created. Exiting.\n");
    return -1;
  }
  // GstElement *capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
  // GstElement *sink = gst_element_factory_make("appsink", "appsink");

  g_object_set(udp,
    "host", "10.0.110.137",
    "port", 5000,
    "sync", FALSE,
    NULL);

  // g_object_set(enc,
  //   "tune", 0x00000004, // zerolatency
  //   "speed-preset", 1,
  //   NULL);

  if (!pipeline || !source || !convert || !tee || !q1 || !sink1 || !q2 || !conv2 || !enc || !pay || !udp)
  {
    g_printerr("One element could not be created. Exiting.\n");
    return -1;
  }

  /* we set the input filename to the source element */
  g_object_set(G_OBJECT(source), "device", argv[1], NULL);

  // force RGB output
  // GstCaps *caps = gst_caps_new_simple(
  //     "video/x-raw",
  //     "format", G_TYPE_STRING, "RGB",
  //     "width", G_TYPE_INT, 320,
  //     "height", G_TYPE_INT, 240,
  //     "framerate", GST_TYPE_FRACTION, 30, 1,
  //     NULL);

  // g_object_set(capsfilter, "caps", caps, NULL);
  // gst_caps_unref(caps);
  
  // GstAppSinkCallbacks callbacks = { NULL, NULL, on_new_sample };
  // gst_app_sink_set_callbacks(GST_APP_SINK(sink1), &callbacks, NULL, NULL);

  /* we add a message handler */
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  guint bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
  gst_object_unref(bus);

  // build pipeline
  gst_bin_add_many(GST_BIN(pipeline),
    source, convert, capsf, tee,
    q1, sink1,
    q2, conv2, enc, pay, udp,
    NULL);

  // link
  gst_element_link_many(source, convert, capsf, tee, NULL);
  gst_element_link_many(tee, q1, sink1, NULL);
  gst_element_link_many(tee, q2, conv2, enc, pay, udp, NULL);

  /* Set the pipeline to "playing" state*/
  g_print("Now playing: %s\n", argv[1]);
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  
  /* Iterate */
  g_print("Running...\n");
  g_main_loop_run(loop);

  /* Out of the main loop, clean up nicely */
  g_print("Returned, stopping playback\n");
  gst_element_set_state(pipeline, GST_STATE_NULL);
  g_print("Deleting pipeline\n");
  gst_object_unref(pipeline);
  g_source_remove(bus_watch_id);
  g_main_loop_unref(loop);

  return 0;
}