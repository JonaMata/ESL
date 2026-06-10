#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <stdint.h>

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
  GstElement *pay  = gst_element_factory_make("rtpjpegpay", NULL);
  GstElement *udp = gst_element_factory_make("udpsink", NULL);    

  if (!pipeline || !source || !capsf || !udp || !pay)
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

  g_object_set(udp,
    "host", "145.126.84.124",
    "port", 5000,
    "sync", FALSE,
    NULL);

  /* we set the input filename to the source element */
  g_object_set(G_OBJECT(source), "device", argv[1], NULL);

  // build pipeline
  gst_bin_add_many(GST_BIN(pipeline),
    source, capsf, pay, udp,
    NULL);

  // link
  gst_element_link_many(source, capsf, pay, udp, NULL);

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