#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <stdint.h>
#include <gst/video/video.h>


static GstElement *appsrc_global = NULL;

static GstFlowReturn on_new_sample(GstAppSink *appsink, gpointer user_data)
{
  g_print("New sample received\n");
  GstSample *sample = gst_app_sink_pull_sample(appsink);
  if (!sample)
    return GST_FLOW_ERROR;

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  GstCaps *caps = gst_sample_get_caps(sample);
  
  GstStructure *s = gst_caps_get_structure(caps, 0);
  const char *format = gst_structure_get_string(s, "format");
  g_print("Format: %s\n", format);

  GstVideoInfo info;
  if (!gst_video_info_from_caps(&info, caps)) {
      g_printerr("Failed to parse video info\n");
      gst_sample_unref(sample);
      return GST_FLOW_ERROR;
  }

  g_print("Width:  %d\n", GST_VIDEO_INFO_WIDTH(&info));
  g_print("Height: %d\n", GST_VIDEO_INFO_HEIGHT(&info));
  g_print("Format: %s\n", gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(&info)));
  
  /* Copy buffer because ownership is tricky across elements */
  GstBuffer *copy = gst_buffer_copy(buffer);

  /* Push into appsrc */
  GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc_global), copy);

  gst_sample_unref(sample);
  return ret;
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

  GstElement *sinkcap  = gst_element_factory_make("appsink", "sinkcap");
  GstElement *appsrc   = gst_element_factory_make("appsrc", "appsrc");
  GstElement *pay      = gst_element_factory_make("x264enc", "encoder");
  GstElement *rtppay   = gst_element_factory_make("rtph264pay", "payloader");
  GstElement *udpsink  = gst_element_factory_make("udpsink", "udpsink");

  if (!pipeline || !source || !capsf || !udpsink || !pay)
  {
    g_printerr("pipeline, source, convert or tee element could not be created. Exiting.\n");
    return -1;
  }
  /* Configure appsrc */
  g_object_set(appsrc,
                "caps",
                gst_caps_new_simple("video/x-raw",
                                    "format", G_TYPE_STRING, "RGB",
                                    "width", G_TYPE_INT, 640,
                                    "height", G_TYPE_INT, 480,
                                    "framerate", GST_TYPE_FRACTION, 30, 1,
                                    NULL),
                "format", GST_FORMAT_TIME,
                NULL);

  g_object_set(udpsink,
    "host", "145.126.84.124",
    "port", 5000,
    "sync", FALSE,
    NULL);

  /* we set the input filename to the source element */
  g_object_set(G_OBJECT(source), "device", argv[1], NULL);

  /* Save appsrc globally for callback access */
  appsrc_global = appsrc;

  /* Link pipeline: source → appsink */
  gst_bin_add_many(GST_BIN(pipeline),
                    source, sinkcap,
                    appsrc, pay, rtppay, udpsink,
                    NULL);

  if (!gst_element_link(source, sinkcap)) {
      g_printerr("Failed to link source → appsink\n");
      return -1;
  }
  /* appsrc → encoder → RTP → UDP */
  if (!gst_element_link_many(appsrc, pay, rtppay, udpsink, NULL)) {
      g_printerr("Failed to link appsrc chain\n");
      return -1;
  }

  /* Connect appsink callback */
  g_signal_connect(sinkcap, "new-sample", G_CALLBACK(on_new_sample), NULL);

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