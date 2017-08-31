#include "network.h"
#include "detection_layer.h"
#include "region_layer.h"
#include "cost_layer.h"
#include "utils.h"
#include "parser.h"
#include "box.h"
#include "image.h"
#include "demo.h"
#include <sys/time.h>

#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>

#define DEMO 1

#ifdef OPENCV

static char **demo_names;
static image **demo_alphabet;
static int demo_classes;

static float **probs;
static box *boxes;
static network net;
static image buff [3];
static image buff_letter[3];
static int buff_index = 0;
static CvCapture *cap;
static IplImage   *ipl;
static float fps = 0;
static float demo_thresh = 0;
static float demo_hier = .5;
static int running = 0;

static int demo_delay = 0;
static int demo_frame = 3;
static int demo_detections = 0;
static float **predictions;
static int demo_index = 0;
static int demo_done = 0;
static float *last_avg2;
static float *last_avg;
static float *avg;
double demo_time;
extern float frame_time_g;
int display_picture = 0;
double get_wall_time()
{
	struct timeval time;
	if (gettimeofday(&time, NULL))
	{
		return 0;
	}
	return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

void *detect_in_thread(void *ptr)
{
	running = 1;
	float nms = .4;

	layer l = net.layers[net.n - 1];
	float *X = buff_letter[(buff_index + 2) % 3].data;
	float *prediction = network_predict(net, X);

	memcpy(predictions[demo_index], prediction, l.outputs * sizeof(float));
	mean_arrays(predictions, demo_frame, l.outputs, avg);
	l.output = last_avg2;
	if (demo_delay == 0) l.output = avg;
	if (l.type == DETECTION)
	{
		get_detection_boxes(l, 1, 1, demo_thresh, probs, boxes, 0);
	}
	else if (l.type == REGION)
	{
		get_region_boxes(l, buff[0].w, buff[0].h, net.w, net.h, demo_thresh, probs, boxes, 0, 0, 0, demo_hier, 1);
	}
	else
	{
		error("Last layer must produce detections\n");
	}
	if (nms > 0) do_nms_obj(boxes, probs, l.w * l.h * l.n, l.classes, nms);

	/*printf("\033[2J");*/
	/*printf("\033[1;1H");*/
	printf("\nFPS:%.1f\n", fps);
	printf("Objects:\n\n");
	image display = buff[(buff_index + 2) % 3];
	draw_detections(display, demo_detections, demo_thresh, boxes, probs, 0, demo_names, demo_alphabet, demo_classes);

	demo_index = (demo_index + 1) % demo_frame;
	running = 0;
	printf("detect finish\n");
	return 0;
}

void *fetch_in_thread(void *ptr)
{
	int status = fill_image_from_stream(cap, buff[buff_index]);
	printf("captrue finish\n");
	letterbox_image_into(buff[buff_index], net.w, net.h, buff_letter[buff_index]);
	printf("thread fetch finish\n");
	if (status == 0) demo_done = 1;
	return 0;
}

void *display_in_thread(void *ptr)
{
	show_image_cv(buff[(buff_index + 1) % 3], "Demo", ipl);
	if (1 == display_picture)
	{
		cvShowImage("Demo", ipl);
	}
	int c = cvWaitKey(1);
	if (c != -1) c = c % 256;
	if (c == 10)
	{
		if (demo_delay == 0) demo_delay = 60;
		else if (demo_delay == 5) demo_delay = 0;
		else if (demo_delay == 60) demo_delay = 5;
		else demo_delay = 0;
	}
	else if (c == 27)
	{
		demo_done = 1;
		return 0;
	}
	else if (c == 82)
	{
		demo_thresh += .02;
	}
	else if (c == 84)
	{
		demo_thresh -= .02;
		if (demo_thresh <= .02) demo_thresh = .02;
	}
	else if (c == 83)
	{
		demo_hier += .02;
	}
	else if (c == 81)
	{
		demo_hier -= .02;
		if (demo_hier <= .0) demo_hier = .0;
	}
	return 0;
}

void *display_loop(void *ptr)
{
	while (1)
	{
		display_in_thread(0);
	}
}

void *detect_loop(void *ptr)
{
	while (1)
	{
		detect_in_thread(0);
	}
}
static struct termios ori_attr, cur_attr;

static __inline
int tty_reset(void)
{
	if (tcsetattr(STDIN_FILENO, TCSANOW, &ori_attr) != 0)
		return -1;

	return 0;
}


static __inline
int tty_set(void)
{

	if ( tcgetattr(STDIN_FILENO, &ori_attr) )
		return -1;

	memcpy(&cur_attr, &ori_attr, sizeof(cur_attr) );
	cur_attr.c_lflag &= ~ICANON;
	//        cur_attr.c_lflag |= ECHO;
	cur_attr.c_lflag &= ~ECHO;
	cur_attr.c_cc[VMIN] = 1;
	cur_attr.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSANOW, &cur_attr) != 0)
		return -1;

	return 0;
}

static __inline
int kbhit(void)
{

	fd_set rfds;
	struct timeval tv;
	int retval;

	/* Watch stdin (fd 0) to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	/* Wait up to five seconds. */
	tv.tv_sec  = 0;
	tv.tv_usec = 0;

	retval = select(1, &rfds, NULL, NULL, &tv);
	/* Don't rely on the value of tv now! */

	if (retval == -1)
	{
		printf("select errorf\n");
		perror("select()");
		return 0;
	}
	else if (retval)
		return 1;
	/* FD_ISSET(0, &rfds) will be true. */
	else
		return 0;
	return 0;
}

void demo(char *cfgfile, char *weightfile, float thresh, char *cam_index, const char *filename_video, char **names, int classes, int delay, char *prefix, int avg_frames, float hier, int w, int h, int frames, int fullscreen)
{

	demo_delay = delay;
	demo_frame = avg_frames;
	predictions = calloc(demo_frame, sizeof(float *));
	image **alphabet = load_alphabet();
	demo_names = names;
	demo_alphabet = alphabet;
	demo_classes = classes;
	demo_thresh = thresh;
	demo_hier = hier;
	printf("Demo\n");
	net = parse_network_cfg(cfgfile);
	if (weightfile)
	{
		load_weights(&net, weightfile);
	}
	set_batch_network(&net, 1);
	pthread_t detect_thread;
	pthread_t fetch_thread;

	srand(2222222);

	int tty_set_flag;
	tty_set_flag = tty_set();


	if (filename_video)
	{
		printf("video file: %s\n", filename_video);
		cap = cvCaptureFromFile(filename_video);
	}
	else
	{
		printf("cam:%s\n", cam_index);
		cap = NULL;
		while (!cap)
		{
			printf("begin camera capture!\n");
			if ('-' == cam_index[0])
				cap = cvCaptureFromCAM(-1);
			else
				cap = cvCreateFileCapture(cam_index);
		}
		if (w)
		{
			cvSetCaptureProperty(cap, CV_CAP_PROP_FRAME_WIDTH, w);
		}
		if (h)
		{
			cvSetCaptureProperty(cap, CV_CAP_PROP_FRAME_HEIGHT, h);
		}
		if (frames)
		{
			cvSetCaptureProperty(cap, CV_CAP_PROP_FPS, frames);
		}
	}

	if (!cap) error("Couldn't connect to webcam.\n");

	layer l = net.layers[net.n - 1];
	demo_detections = l.n * l.w * l.h;
	int j;

	avg = (float *) calloc(l.outputs, sizeof(float));
	last_avg  = (float *) calloc(l.outputs, sizeof(float));
	last_avg2 = (float *) calloc(l.outputs, sizeof(float));
	for (j = 0; j < demo_frame; ++j) predictions[j] = (float *) calloc(l.outputs, sizeof(float));

	boxes = (box *)calloc(l.w * l.h * l.n, sizeof(box));
	probs = (float **)calloc(l.w * l.h * l.n, sizeof(float *));
	for (j = 0; j < l.w * l.h * l.n; ++j) probs[j] = (float *)calloc(l.classes + 1, sizeof(float));

	buff[0] = get_image_from_stream(cap);
	buff[1] = copy_image(buff[0]);
	buff[2] = copy_image(buff[0]);
	buff_letter[0] = letterbox_image(buff[0], net.w, net.h);
	buff_letter[1] = letterbox_image(buff[0], net.w, net.h);
	buff_letter[2] = letterbox_image(buff[0], net.w, net.h);
	ipl = cvCreateImage(cvSize(buff[0].w, buff[0].h), IPL_DEPTH_8U, buff[0].c);

	int count = 0;
	/*  if (!prefix)
	    {
	        cvNamedWindow("Demo", CV_WINDOW_NORMAL);
	        if (fullscreen)
	        {
	            cvSetWindowProperty("Demo", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);
	        }
	        else
	        {
	            cvMoveWindow("Demo", 0, 0);
	            cvResizeWindow("Demo", 1352, 1013);
	        }
	    }*/

	demo_time = get_wall_time();
	int frame_again_count = 5;
	int frame_again_index = 0;
	int static frame_capture_failed = 0;
	while (1)
	{

		if (1 == demo_done)
		{
			printf("begin capture!\n");
			if (!cap)
				cvReleaseCapture(&cap);
			cap = cvCreateFileCapture(cam_index);
			if (!cap)
			{
				if (frame_capture_failed > 10)
				{
					printf("again capture greate 10 exit\n");
					exit(0);
				}
				else
				{
					printf("again capture,but still error,count=%d\n", frame_capture_failed);
					frame_capture_failed++;
				}
				continue;
			}
			demo_done = 0;
		}
		buff_index = (buff_index + 1) % 3;
		printf("create fetch and detect");
		if (pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
		if (pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");
		if (!prefix)
		{
			printf("fps conut\n");
			if (count % (demo_delay + 1) == 0)
			{
				float during = (get_wall_time() - demo_time);
				if (during > frame_time_g)
				{
					//      if(frame_again_index=frame_again_count)
					printf("frame time is during=%f,thresh=%f,so reset capture\n", during, frame_time_g);
					if (2 == count % 3)
						demo_done = 1;
				}
				fps = 1. / (get_wall_time() - demo_time);
				demo_time = get_wall_time();
				float *swap = last_avg;
				last_avg  = last_avg2;
				last_avg2 = swap;
				memcpy(last_avg, avg, l.outputs * sizeof(float));
			}
			printf("display_in_");
			// display_in_thread(0);
		}
		else
		{
			char name[256];
			sprintf(name, "%s_%08d", prefix, count);
			//save_image(buff[(buff_index + 1)%3], name);
		}
		printf("thread join\n");
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 5;
		int s = pthread_timedjoin_np(fetch_thread, NULL, &ts);
		if (0 != s)
		{
			printf("capture timeOut again \n");
			demo_done = 1;
		}
		//pthread_join(fetch_thread, 0);
		pthread_join(detect_thread, 0);
		display_in_thread(0);
		printf("next frame,count=%d\n", count);
		++count;
		if (count < 1)
		{
			demo_done = 1;
			count = 0;
		}
		if ( kbhit() )
		{
			const int key = getchar();
			printf("%c pressed\n", key);
			if (key == 'q')
				break;
			if (key == 'Q')
			{
				if (display_picture == 0)
					display_picture = 1;
				else
				{
					display_picture = 0;
					cvDestroyWindow("Demo");
				}
			}
		}
		printf("demo frame finish!\n");
		/*  else
		    {
		        fprintf(stderr, "<no key detected>\n");
		    }*/
	}
	if (tty_set_flag == 0)
		tty_reset();
}
#else
void demo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int delay, char *prefix, int avg, float hier, int w, int h, int frames, int fullscreen)
{
	fprintf(stderr, "Demo needs OpenCV for webcam images.\n");
}
#endif
