#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <stdint.h>
#include <gst/video/video.h>

static GstElement *processed_src = NULL;
static guint64 pts_counter = 0;


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
  g_print("Format: %s\n", format);gstreamer/onlyVideo.c

  GstVideoInfo info;
  if (!gst_video_info_from_caps(&info, caps)) {
      g_printerr("Failed to parse video info\n");
      gst_sample_unref(sample);
      return GST_FLOW_ERROR;
  }

  g_print("Width:  %d\n", GST_VIDEO_INFO_WIDTH(&info));
  g_print("Height: %d\n", GST_VIDEO_INFO_HEIGHT(&info));
  g_print("Format: %s\n",
         gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(&info)));

  int stride1 = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

  g_print("Stride: %d\n", stride1);

  GstVideoFrame frame;

  if (!gst_video_frame_map(
          &frame,
          &info,
          buffer,
          GST_MAP_READWRITE))
  {
      g_printerr("Failed to map frame\n");
      gst_sample_unref(sample);
      return GST_FLOW_ERROR;
  }

  uint8_t *data = GST_VIDEO_FRAME_PLANE_DATA(&frame, 0);
  int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);

  int width  = GST_VIDEO_FRAME_WIDTH(&frame);
  int height = GST_VIDEO_FRAME_HEIGHT(&frame);

  int size = 0;
  int sum_x = 0;
  int sum_y = 0;
  for (int y = 0; y < height; y++) {

    uint8_t *row = data + y * stride;

    for (int x = 0; x < width; x++) {

      uint8_t *pixel = row + x * 3;

      uint8_t r = pixel[0];
      uint8_t g = pixel[1];
      uint8_t b = pixel[2];

      if (g > r+20 && g > b+20 && g > 60) {
        size += 1;
        sum_x += x;
        sum_y += y;
      }
      //  Make pixel black here in an else statement
      else{
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
      }
      if (x == 160 && y == 120) {
        g_print("Center: r: %d, g: %d, b: %d\n", r, g, b);
      }
    }
  }

  double x_pos = size > 1000 ? (double)sum_x / size : -1;
  double y_pos = size > 1000 ? (double)sum_y / size : -1;
  g_print("Position: (%f, %f)\tSize: %d\n", x_pos, y_pos, size);

  gst_video_frame_unmap(&frame);

  // GST_BUFFER_PTS(buffer) = pts_counter;
  // GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 30);
  pts_counter += GST_BUFFER_DURATION(buffer);

  gst_buffer_ref(buffer);

  gst_app_src_push_buffer(
      GST_APP_SRC(processed_src),
      buffer);

  gst_sample_unref(sample);
  g_print("Pushed frame\n");
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
  GstElement *convert = gst_element_factory_make("videoconvert", "convert");
  GstElement *capsf = gst_element_factory_make("capsfilter", NULL);
  GstElement *sink1 = gst_element_factory_make("appsink", NULL);


  if (!pipeline || !source || !capsf || !convert)
  {
    g_printerr("pipeline, source, capsf or tee element could not be created. Exiting.\n");
    return -1;
  }
  GstCaps *caps = gst_caps_new_simple(
    "video/x-raw",
    "format", G_TYPE_STRING, "RGB",
    "width", G_TYPE_INT, 320,
    "height", G_TYPE_INT, 240,
    NULL);

  g_object_set(capsf, "caps", caps, NULL);
  gst_caps_unref(caps);

  /* Branch 1: UDP Sink */
  processed_src = gst_element_factory_make("appsrc", NULL); 
  GstElement *queue = gst_element_factory_make("queue", NULL);
  GstElement *enc = gst_element_factory_make("jpegenc", NULL);
  GstElement *pay  = gst_element_factory_make("rtpjpegpay", NULL);
  GstElement *udp = gst_element_factory_make("udpsink", NULL);    

  g_object_set(udp,
    "host", "145.126.84.124",
    "port", 5000,
    "sync", FALSE,
    NULL);
  
  g_object_set(processed_src,
    "is-live", TRUE,
    "format", GST_FORMAT_TIME,
    "block", TRUE,
    "do-timestamp", TRUE,
    NULL);

  g_object_set(processed_src,
  "caps",
  gst_caps_new_simple(
      "video/x-raw",
      "format", G_TYPE_STRING, "RGB",
      "width", G_TYPE_INT, 320,
      "height", G_TYPE_INT, 240,
      "framerate", GST_TYPE_FRACTION, 30, 1,
      NULL),
  NULL);

  /* Branch 2: Appsink */
  GstElement *q2 = gst_element_factory_make("queue", NULL);
  GstElement *decoder = gst_element_factory_make("jpegdec", "jpeg-decoder");

  GstAppSinkCallbacks callbacks = { NULL, NULL, on_new_sample };
  gst_app_sink_set_callbacks(GST_APP_SINK(sink1), &callbacks, NULL, NULL);

  /* we set the input filename to the source element */
  g_object_set(G_OBJECT(source), "device", argv[1], NULL);

  /* Build pipeline */
  gst_bin_add_many(GST_BIN(pipeline),
    source, convert, capsf, sink1, NULL);
  
  gst_bin_add_many(GST_BIN(pipeline),
                processed_src,
                queue,
                enc,
                pay,
                udp,
                NULL);

  gst_element_link_many(processed_src,
                        queue,
                        enc,
                        pay,
                        udp,
                        NULL);

  /*Link elements*/
  gst_element_link_many(source, convert, capsf, sink1, NULL);

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