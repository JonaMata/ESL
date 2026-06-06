#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <stdint.h>

static GstFlowReturn on_new_sample(GstAppSink *appsink, gpointer user_data)
{
  g_print("New sample received\n");
  GstSample *sample = gst_app_sink_pull_sample(appsink);
  if (!sample)
    return GST_FLOW_ERROR;

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  GstMapInfo map;

  // GstCaps *caps = gst_sample_get_caps(sample);
  // GstStructure *s = gst_caps_get_structure(caps, 0);
  // const char *format = gst_structure_get_string(s, "format");
  // g_print("Format: %s\n", format);  

  if (gst_buffer_map(buffer, &map, GST_MAP_READ))
  {
    // Access raw frame data
    g_print("Received buffer of size: %zu\n", map.size);

    double sum = 0;
    for (gsize i = 0; i < map.size; i++)
    {
      sum += map.data[i];
    }
    g_print("Average pixel value: %f\n", sum / map.size);

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
  GstElement *capsf = gst_element_factory_make("capsfilter", NULL);
  GstElement *tee = gst_element_factory_make("tee", NULL);

  if (!pipeline || !source || !capsf || !tee)
  {
    g_printerr("pipeline, source, convert or tee element could not be created. Exiting.\n");
    return -1;
  }

  GstCaps *caps = gst_caps_new_simple(
    "image/jpeg",
    "width", G_TYPE_INT, 320,
    "height", G_TYPE_INT, 240,
    "framerate", GST_TYPE_FRACTION, 30, 1,
    NULL);

  g_object_set(capsf, "caps", caps, NULL);
  gst_caps_unref(caps);

  /* Branch 1: UDP Sink */
  GstElement *q1 = gst_element_factory_make("queue", NULL);
  GstElement *pay  = gst_element_factory_make("rtpjpegpay", NULL);
  GstElement *udp = gst_element_factory_make("udpsink", NULL);    

  g_object_set(udp,
    "host", "192.168.68.70",
    "port", 5000,
    "sync", FALSE,
    NULL);

  /* Branch 2: Appsink */
  GstElement *q2 = gst_element_factory_make("queue", NULL);
  GstElement *sink1 = gst_element_factory_make("appsink", NULL);
  GstAppSinkCallbacks callbacks = { NULL, NULL, on_new_sample };
  gst_app_sink_set_callbacks(GST_APP_SINK(sink1), &callbacks, NULL, NULL);

  /* we set the input filename to the source element */
  g_object_set(G_OBJECT(source), "device", argv[1], NULL);

  /* Build pipeline */
  gst_bin_add_many(GST_BIN(pipeline),
    source, capsf, tee,
    q1, pay, udp,
    q2, sink1,
    NULL);

  /*Link elements*/
  gst_element_link_many(source, capsf, tee, NULL);
  gst_element_link_many(tee, q1, pay, udp, NULL);
  gst_element_link_many(tee, q2, sink1, NULL);

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
  g_main_loop_unref(loop);

  return 0;
}