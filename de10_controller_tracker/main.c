// #include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>


#include "soc_system.h"

#include "xxsubmod.h"
#include "yysubmod.h"

#define MOTOR_YAW 0
#define MOTOR_PITCH 1

#define DEADZONE 10

GMainLoop *loop;

uint8_t* jiwy_map = NULL;

int yaw_setpoint = 5000;
int pitch_setpoint = 5000;

bool running = true;

int frame_count = 0;

int HISTORY_SIZE = 5;
double x_history[HISTORY_SIZE];
double y_history[HISTORY_SIZE];
double history_time = HISTORY_SIZE * 0.033; // Assuming 30 FPS
bool initialized_history = false;

void set_pwm(uint8_t yaw_duty_cycle, bool yaw_direction, bool yaw_enable, uint8_t pitch_duty_cycle, bool pitch_direction, bool pitch_enable, bool yaw_reset, bool pitch_reset) {
    *((uint32_t *)jiwy_map) = yaw_duty_cycle | yaw_enable << 8 | yaw_direction << 9
        | pitch_duty_cycle << 10 | pitch_enable << 18 | pitch_direction << 19
        | yaw_reset << 20 | pitch_reset << 21;
}

void get_encoders(uint16_t* yaw_encoder, uint16_t* pitch_encoder) {
    uint32_t encoder_values = *((uint32_t *)(jiwy_map));
    *yaw_encoder = encoder_values & 0xFFFF;
    *pitch_encoder = (encoder_values >> 16) & 0xFFFF;
}

int home(int motor) {
    uint16_t prev_enc = 0;
    uint16_t enc = 0;
    uint16_t enc_no = 0;
    if (motor == MOTOR_YAW) {
        get_encoders(&enc, &enc_no);
        set_pwm(20, true, true, 0, false, false, false, false);
    } else {
        get_encoders(&enc_no, &enc);
        set_pwm(0, false, false, 20, true, true, false, false);
    }
    sleep(1);
    do {
        printf("Homing... Current encoder: %d\n", enc);
        sleep(1);
        prev_enc = enc;
        if (motor == MOTOR_YAW) {
            get_encoders(&enc, &enc_no);
        } else {
            get_encoders(&enc_no, &enc);
        }
    } while (prev_enc-enc != 0);
    if (motor == MOTOR_YAW) {
        set_pwm(20, true, true, 0, false, false, true, false);
        set_pwm(20, false, true, 0, false, false, false, false);
    } else {
        set_pwm(0, false, false, 20, true, true, false, true);
        set_pwm(0, false, false, 20, false, true, false, false);
    }
    sleep(1);
    do {
        printf("Homing... Current encoder: %d\n", enc);
        sleep(1);
        prev_enc = enc;
        if (motor == MOTOR_YAW) {
            get_encoders(&enc, &enc_no);
        } else {
            get_encoders(&enc_no, &enc);
        }
    } while (prev_enc-enc != 0);
    set_pwm(0, false, false, 0, false, false, false, false);
    return enc;
}


void* controller(void* arg) {
    int fd = 0;


	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("Couldn't open /dev/mem\n");
		// return -1;
	}

	jiwy_map = (uint8_t*)mmap(NULL, HPS_0_ARM_A9_0_JIWY_0_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HPS_0_ARM_A9_0_JIWY_0_BASE);
	if (jiwy_map == MAP_FAILED) {
		perror("Couldn't map JIWY.");
		close(fd);
		// return -1;
	}

    timer_t timer_id;
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;
    sev.sigev_value.sival_ptr = &timer_id;


    struct itimerspec its;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 1e6;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 1e6;

    timer_create(CLOCK_MONOTONIC, &sev, &timer_id);

    // create a sigset for sigwait to wait for SIGUSR1
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    timer_settime(timer_id, 0, &its, NULL);

    printf("Start homing...\n");
    int yaw_max = home(MOTOR_YAW);
    int pitch_max = home(MOTOR_PITCH);
    printf("Homing complete.\n");

    XXDouble yaw_u [2 + 1];
    XXDouble yaw_y [2 + 1];
    YYDouble pitch_u [3 + 1];
    YYDouble pitch_y [1 + 1];


    /* Initialize the inputs and outputs with correct initial values */
    yaw_u[0] = 0.0;		/* in */
    yaw_u[1] = 0.0;		/* position */
    pitch_u[0] = 0.0;   /* corr */
    pitch_u[1] = 0.0;	/* in */
    pitch_u[2] = 0.0;	/* position */

    yaw_y[0] = 0.0;		/* corr */
    yaw_y[1] = 0.0;		/* out */
    pitch_y[0] = 0.0;	/* out */

	XXInitializeSubmodel (yaw_u, yaw_y, xx_time);
    YYInitializeSubmodel (pitch_u, pitch_y, yy_time);

    uint16_t yaw_encoder;
    uint16_t pitch_encoder;
    uint16_t prev_yaw_encoder = 0;
    uint16_t prev_pitch_encoder = 0;
    int raw_yaw_position = 0;
    int raw_pitch_position = 0;

    printf("Starting control loop...\n");
    while (1) {
        int sig;
        sigwait(&sigset, &sig);

        // Setpoint clamping
        // yaw_setpoint = (yaw_setpoint > yaw_max) ? yaw_max : ((yaw_setpoint < 0) ? 0 : yaw_setpoint);
        // pitch_setpoint = (pitch_setpoint > pitch_max) ? pitch_max : ((pitch_setpoint < 0) ? 0 : pitch_setpoint);
        if (yaw_setpoint > yaw_max) yaw_setpoint = yaw_max;
        if (yaw_setpoint < 0) yaw_setpoint = 0;
        if (pitch_setpoint > pitch_max) pitch_setpoint = pitch_max;
        if (pitch_setpoint < 0) pitch_setpoint = 0;

        // printf("Timer tick\n");
        get_encoders(&yaw_encoder, &pitch_encoder);
        int yaw_diff = yaw_encoder - prev_yaw_encoder;
        prev_yaw_encoder = yaw_encoder;
        if (abs(yaw_diff) > 32768) {
            if (yaw_diff > 0) {
                yaw_diff -= 65536;
            } else {
                yaw_diff += 65536;
            }
        }
        raw_yaw_position += yaw_diff;
        XXDouble yaw_position = (XXDouble)raw_yaw_position / 21708.8 * 2 * 3.1415926;
        yaw_u[0] = (double)yaw_setpoint / 21708.8 * 2 * 3.1415926;
        yaw_u[1] = yaw_position;
        XXCalculateSubmodel (yaw_u, yaw_y, xx_time);
        uint8_t yaw_duty_cycle = (uint8_t)(abs(yaw_y[1] * 255));
        if (yaw_duty_cycle > 64) yaw_duty_cycle = 64;
        bool yaw_direction = yaw_y[1] < 0;

        int pitch_diff = pitch_encoder - prev_pitch_encoder;
        prev_pitch_encoder = pitch_encoder;
        if (abs(pitch_diff) > 32768) {
            if (pitch_diff > 0) {
                pitch_diff -= 65536;
            } else {
                pitch_diff += 65536;
            }
        }
        raw_pitch_position += pitch_diff;
        YYDouble pitch_position = (YYDouble)raw_pitch_position / 21708.8 * 2 * 3.1415926;
        pitch_u[1] = (double)pitch_setpoint / 21708.8 * 2 * 3.1415926;
        pitch_u[2] = pitch_position;
        YYCalculateSubmodel (pitch_u, pitch_y, yy_time);
        uint8_t pitch_duty_cycle = (uint8_t)(abs(pitch_y[0] * 255));
        if (pitch_duty_cycle > 64) pitch_duty_cycle = 64;
        bool pitch_direction = pitch_y[0] < 0;

        set_pwm(yaw_duty_cycle, yaw_direction, true, pitch_duty_cycle, pitch_direction, true, false, false);
    }


	XXTerminateSubmodel (yaw_u, yaw_y, xx_time);
    YYTerminateSubmodel (pitch_u, pitch_y, yy_time);


	close(fd);
}

void exit(int signum) {
    running = false;
    g_main_loop_quit(loop);
}


static gboolean
bus_call(GstBus *bus,
         GstMessage *msg,
         gpointer data)
{
  loop = (GMainLoop *)data;

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
  //g_print("New sample received\n");
  GstSample *sample = gst_app_sink_pull_sample(appsink);
  if (!sample)
    return GST_FLOW_ERROR;

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  GstMapInfo map;

  GstCaps *caps = gst_sample_get_caps(sample);
  GstStructure *s = gst_caps_get_structure(caps, 0);

  const char *format = gst_structure_get_string(s, "format");

  //g_print("Format: %s\n", format);

  GstVideoInfo info;
  if (!gst_video_info_from_caps(&info, caps)) {
      g_printerr("Failed to parse video info\n");
      gst_sample_unref(sample);
      return GST_FLOW_ERROR;
  }

  //g_print("Width:  %d\n", GST_VIDEO_INFO_WIDTH(&info));
  //g_print("Height: %d\n", GST_VIDEO_INFO_HEIGHT(&info));
  //g_print("Format: %s\n",
  //       gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(&info)));

  int stride1 = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

  //g_print("Stride: %d\n", stride1);

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
        //g_print("Center: r: %d, g: %d, b: %d\n", r, g, b);
      }
    }
  }

  double x_pos = size > 1000 ? (double)sum_x / size : -1;
  double y_pos = size > 1000 ? (double)sum_y / size : -1;
  //g_print("Position: (%f, %f)\tSize: %d\n", x_pos, y_pos, size);

  /* Shift the history arrays */
  if (x_pos >= 0 && y_pos >= 0) {
    if (!initialized_history) {
        for (int i = 0; i < HISTORY_SIZE; i++) {
            x_history[i] = x_pos;
            y_history[i] = y_pos;
        }
        initialized_history = true;
    } else {
        for (int i = HISTORY_SIZE - 1; i > 0; i--) {
            x_history[i] = x_history[i-1];
            y_history[i] = y_history[i-1];
        }
  }
  x_history[0] = x_pos;
  y_history[0] = y_pos;

  /* Calculate the speed */
  double x_speed = (x_history[0] - x_history[HISTORY_SIZE-1])/history_time;
  double y_speed = (y_history[0] - y_history[HISTORY_SIZE-1])/history_time;
  g_print("Speed: (%f, %f)\n", x_speed, y_speed);

  frame_count++;
  if (frame_count == 1) {
    frame_count = 0;
    if (size > 1000) {
        int yaw_diff = (int)(x_pos - width/2);
        if (abs(yaw_diff) > DEADZONE) {
            yaw_setpoint += yaw_diff*2;
        }
        int pitch_diff = (int)(y_pos - height/2);
        if (abs(pitch_diff) > DEADZONE) {
            pitch_setpoint += pitch_diff*2;
        }
    }
    if((x_pos < 0 || y_pos < 0) && initialized_history) {
        int yaw_diff = (int)((x_history[0]+x_speed*0.033) - width/2);
        if (abs(yaw_diff) > DEADZONE) {
            yaw_setpoint += yaw_diff*2;
        }
        int pitch_diff = (int)((y_history[0]+y_speed*0.033) - height/2);
        if (abs(pitch_diff) > DEADZONE) {
            pitch_setpoint += pitch_diff*2;
        }
    }
  }

  gst_video_frame_unmap(&frame);


  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

int main(int argc, char** argv) {
    sigset_t sigusr_set;
    sigemptyset(&sigusr_set);
    sigaddset(&sigusr_set, SIGUSR1);

    pthread_sigmask(SIG_BLOCK, &sigusr_set, NULL);

    pthread_t thread;
    pthread_create(&thread, NULL, controller, NULL);

    signal(SIGINT, exit);

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

    caps = gst_caps_new_simple("image/jpeg", "format", G_TYPE_STRING, "RGB", "width", G_TYPE_INT, 160, "height", G_TYPE_INT, 120, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    GstAppSinkCallbacks callbacks = { NULL, NULL, on_new_sample };
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, NULL, NULL);

    /* we add a message handler */
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    /* we add all elements into the pipeline */
    gst_bin_add_many(GST_BIN(pipeline),
                    source, encoder, capsfilter, decoder, sink, convert, NULL);

    gst_element_link(source, convert);
    gst_element_link(convert, sink);

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

    printf("Exiting...\n");
    pthread_cancel(thread);
    pthread_join(thread, NULL);
    set_pwm(0, false, false, 0, false, false, false, false);
    printf("Exited.\n");
    return 0;
}
