#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

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
  g_print("Format: %s\n",
         gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(&info)));

  int stride1 = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

  g_print("Stride: %d\n", stride1);

  GstVideoFrame frame;

  if (!gst_video_frame_map(
          &frame,
          &info,
          buffer,
          GST_MAP_READ))
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
      if (x == 160 && y == 120) {
        g_print("Center: r: %d, g: %d, b: %d\n", r, g, b);
      }
    }
  }

  double x_pos = size > 0 ? (double)sum_x / size : 0;
  double y_pos = size > 0 ? (double)sum_y / size : 0;
  g_print("Position: (%f, %f)\tSize: %d\n", x_pos, y_pos, size);


  gst_video_frame_unmap(&frame);

  // if (gst_buffer_map(buffer, &map, GST_MAP_READ))
  // {
  //   // Access raw frame data
  //   g_print("Received buffer of size: %zu\n", map.size);

  //   int size = 0;
  //   int sum_x = 0;
  //   int sum_y = 0;
  //   for (gsize i = 0; i < map.size/3; i++)
  //   {
  //     uint8_t r = map.data[i*3];
  //     uint8_t g = map.data[i*3 + 1];
  //     uint8_t b = map.data[i*3 + 2];
  //     if (g > 100 && r < 50 && b < 50) // Simple green pixel detection
  //     {
  //       size += 1;
  //       sum_x += (i % 320); // Assuming width of 320
  //       sum_y += (i / 320); // Assuming width of 320
  //     }
  //   }
  //   double x_pos = size > 0 ? (double)sum_x / 320 : 0;
  //   double y_pos = size > 0 ? (double)sum_y / 240 : 0;
  //   g_print("Position: (%f, %f)\tSize: %d\n", x_pos, y_pos, size);
  //   g_print("Center: r: %d, g: %d, b: %d\n", map.data[120*320*3+160], map.data[120*320*3+160+1], map.data[120*320*3+160+2]);

  //   gst_buffer_unmap(buffer, &map);
  // }

  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

int main(int argc,
         char *argv[])
{
  GMainLoop *loop;

  GstElement *pipeline, *source, *encoder, *capsfilter, *decoder, *sink, *convert;
  GstBus *bus;
  GstCaps *caps;
  guint bus_watch_id;

  /* Initialisation */
  gst_init(&argc, &argv);

  loop = g_main_loop_new(NULL, FALSE);

  /* Check input arguments */
  if (argc != 2)
  {
    g_printerr("Usage: %s <Ogg/Vorbis filename>\n", argv[0]);
    return -1;
  }

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new("video-pipeline");
  source = gst_element_factory_make("v4l2src", "camera-source");
  encoder = gst_element_factory_make("jpegenc", "jpeg-encoder");
  capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
  decoder = gst_element_factory_make("jpegdec", "jpeg-decoder");
  sink = gst_element_factory_make("appsink", "appsink");
  convert = gst_element_factory_make("videoconvert", "convert");

  if (!pipeline || !source || !encoder || !capsfilter || !decoder || !sink)
  {
    g_printerr("One element could not be created. Exiting.\n");
    return -1;
  }

  /* Set up the pipeline */

  /* we set the input filename to the source element */
  g_object_set(G_OBJECT(source), "device", argv[1], NULL);
  // g_object_set (G_OBJECT (sink), "location", "file.yuv", NULL);

  caps = gst_caps_new_simple("image/jpeg", "format", G_TYPE_STRING, "RGB", "width", G_TYPE_INT, 160, "height", G_TYPE_INT, 120, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
  g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
  gst_caps_unref(caps);

  // g_signal_connect(sink, "new-sample", G_CALLBACK(on_new_sample), NULL);
  GstAppSinkCallbacks callbacks = { NULL, NULL, on_new_sample };
  gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, NULL, NULL);

  /* we add a message handler */
  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
  gst_object_unref(bus);

  /* we add all elements into the pipeline */
  /* camera-source | jpeg-encoder | jpeg-decoder | file-output */
  gst_bin_add_many(GST_BIN(pipeline),
                   source, encoder, capsfilter, decoder, sink, convert, NULL);

  /* we link the elements together */
  /* camera-source -> jpeg-encoder -> jpeg-decoder -> file-output */
  // gst_element_link(source, decoder);
  gst_element_link(source, convert);
  gst_element_link(convert, sink);
  // gst_element_link(decoder, sink);
  // gst_element_link(encoder, decoder);

  caps = gst_caps_new_simple(
    "video/x-raw",
    "format", G_TYPE_STRING, "RGB",
    "width", G_TYPE_INT, 320,
    "height", G_TYPE_INT, 240,
    NULL);

  gst_app_sink_set_caps(GST_APP_SINK(sink), caps);
  gst_caps_unref(caps);

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
  gst_object_unref(GST_OBJECT(pipeline));
  g_source_remove(bus_watch_id);
  g_main_loop_unref(loop);

  return 0;
}