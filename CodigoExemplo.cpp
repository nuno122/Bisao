#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2\highgui.hpp>
#include <opencv2\videoio.hpp>

extern "C" {
#include "lib/vc.h"
}

typedef struct
{
	int width, height;
} VIDEOINFO;

typedef struct
{
	int id;
	int x, y;
	int vx, vy;
	int hits;
	int seenabove;
	int missingframes;
	int counted;
} ORANGETRACK;

int hue_in_range(unsigned char hue, int hmin, int hmax)
{
	int hmin255 = (hmin * 255) / 360;
	int hmax255 = (hmax * 255) / 360;

	if (hmin255 <= hmax255) return (hue >= hmin255 && hue <= hmax255);
	return (hue >= hmin255 || hue <= hmax255);
}

int blob_hsv_color_count(IVC *image_hsv, IVC *image_labels, OVC blob, int hmin, int hmax, int smin, int vmin)
{
	long int pos_hsv, pos_label;
	int x, y;
	int count = 0;

	if (image_hsv == NULL || image_labels == NULL) return 0;

	for (y = blob.y; y < blob.y + blob.height; y++)
	{
		for (x = blob.x; x < blob.x + blob.width; x++)
		{
			pos_label = y * image_labels->bytesperline + x * image_labels->channels;

			if (image_labels->data[pos_label] == blob.label)
			{
				pos_hsv = y * image_hsv->bytesperline + x * image_hsv->channels;

				if (hue_in_range(image_hsv->data[pos_hsv], hmin, hmax) &&
					image_hsv->data[pos_hsv + 1] >= smin &&
					image_hsv->data[pos_hsv + 2] >= vmin)
				{
					count++;
				}
			}
		}
	}

	return count;
}

void draw_text(cv::Mat &frame, const std::string &text, int x, int y, cv::Scalar color, double scale)
{
	cv::putText(frame, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, scale, color, 2);
	cv::putText(frame, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, scale, cv::Scalar(0, 0, 0), 1);
}

void frame_bgr_to_rgb(const cv::Mat &frame, IVC *image_rgb)
{
	for (int y = 0; y < frame.rows; y++)
	{
		const unsigned char *data = frame.ptr<unsigned char>(y);

		for (int x = 0; x < frame.cols; x++)
		{
			long int pos = y * image_rgb->bytesperline + x * image_rgb->channels;
			image_rgb->data[pos] = data[x * 3 + 2];
			image_rgb->data[pos + 1] = data[x * 3 + 1];
			image_rgb->data[pos + 2] = data[x * 3];
		}
	}
}

void segment_oranges(IVC *image_rgb, IVC *image_hsv, IVC *image_seg_rgb, IVC *image_seg, IVC *image_tmp)
{
	vc_rgb_to_hsv(image_rgb, image_hsv);
	vc_hsv_segmentation(image_hsv, image_seg_rgb, 6, 50, 25, 255, 20, 255);
	vc_rgb_to_gray(image_seg_rgb, image_seg);

	memset(image_tmp->data, 0, image_tmp->bytesperline * image_tmp->height);
	vc_binary_close(image_seg, image_tmp, 5, 5);
	memset(image_seg->data, 0, image_seg->bytesperline * image_seg->height);
	vc_binary_open(image_tmp, image_seg, 3, 3);
}

int blob_passes_geometry(OVC blob, int maxblobarea)
{
	float aspectratio;
	float circularidade;

	if (blob.height == 0 || blob.perimeter == 0) return 0;
	if (blob.area < 1500 || blob.area > maxblobarea) return 0;

	aspectratio = (float)blob.width / (float)blob.height;
	circularidade = (float)((4.0 * 3.1415926535 * blob.area) /
		((double)blob.perimeter * (double)blob.perimeter));

	if (aspectratio < 0.45f || aspectratio > 2.20f) return 0;
	if (circularidade < 0.20f) return 0;

	return 1;
}

int blob_passes_color(IVC *image_hsv, IVC *image_labels, OVC blob)
{
	int orangepixels;
	int redpixels;
	int yellowpixels;
	int greenpixels;
	int totalcolorpixels;

	orangepixels = blob_hsv_color_count(image_hsv, image_labels, blob, 8, 38, 25, 20);
	redpixels = blob_hsv_color_count(image_hsv, image_labels, blob, 340, 15, 30, 20);
	yellowpixels = blob_hsv_color_count(image_hsv, image_labels, blob, 39, 70, 25, 20);
	greenpixels = blob_hsv_color_count(image_hsv, image_labels, blob, 66, 140, 20, 20);
	totalcolorpixels = orangepixels + redpixels + yellowpixels + greenpixels;

	if (totalcolorpixels == 0) return 0;
	if (totalcolorpixels < (blob.area * 35 / 100)) return 0;
	if (orangepixels < (totalcolorpixels * 25 / 100)) return 0;
	if (orangepixels <= redpixels) return 0;
	if (orangepixels <= yellowpixels) return 0;
	if (orangepixels <= greenpixels) return 0;
	if (redpixels > (totalcolorpixels * 18 / 100)) return 0;
	if (yellowpixels > (totalcolorpixels * 55 / 100)) return 0;
	if (greenpixels > (totalcolorpixels * 15 / 100)) return 0;
	if ((redpixels + yellowpixels) > orangepixels) return 0;

	return 1;
}

int find_best_track(std::vector<ORANGETRACK> &tracks, std::vector<int> &trackused, int xc, int yc)
{
	int besttrack = -1;
	double bestdistance = 1000000.0;

	for (int t = 0; t < (int)tracks.size(); t++)
	{
		double dx, dy, distance;
		double predictedx, predictedy;
		double maxdistance;

		if (trackused[t] != 0) continue;

		predictedx = (double)tracks[t].x + (double)(tracks[t].vx * (tracks[t].missingframes + 1));
		predictedy = (double)tracks[t].y + (double)(tracks[t].vy * (tracks[t].missingframes + 1));
		maxdistance = 120.0 + (30.0 * (double)tracks[t].missingframes);
		if (maxdistance > 320.0) maxdistance = 320.0;

		dx = (double)xc - predictedx;
		dy = (double)yc - predictedy;
		distance = sqrt((dx * dx) + (dy * dy));

		if ((distance < maxdistance) && (distance < bestdistance))
		{
			bestdistance = distance;
			besttrack = t;
		}
	}

	return besttrack;
}

void update_existing_track(ORANGETRACK *track, OVC blob, int countingliney, int countbandtopy, int *total_laranjas)
{
	int oldx = track->x;
	int oldy = track->y;

	track->x = blob.xc;
	track->y = blob.yc;
	track->vx = track->x - oldx;
	track->vy = track->y - oldy;
	track->hits++;
	if (track->y < countingliney) track->seenabove = 1;
	track->missingframes = 0;

	if ((track->counted == 0) &&
		(track->hits >= 3) &&
		(track->seenabove != 0) &&
		(track->y >= countbandtopy) &&
		(track->vy >= 0))
	{
		track->counted = 1;
		(*total_laranjas)++;
	}
}

void add_new_track(std::vector<ORANGETRACK> &tracks, std::vector<int> &trackused, int *nexttrackid, OVC blob, int countingliney)
{
	ORANGETRACK newtrack;

	newtrack.id = (*nexttrackid)++;
	newtrack.x = blob.xc;
	newtrack.y = blob.yc;
	newtrack.vx = 0;
	newtrack.vy = 0;
	newtrack.hits = 1;
	newtrack.seenabove = (blob.yc < countingliney) ? 1 : 0;
	newtrack.missingframes = 0;
	newtrack.counted = 0;

	tracks.push_back(newtrack);
	trackused.push_back(1);
}

void remove_missing_tracks(std::vector<ORANGETRACK> &tracks, std::vector<int> &trackused)
{
	for (int t = 0; t < (int)tracks.size(); t++)
	{
		if (trackused[t] == 0) tracks[t].missingframes++;
	}

	for (int t = (int)tracks.size() - 1; t >= 0; t--)
	{
		if (tracks[t].missingframes > 25)
		{
			tracks.erase(tracks.begin() + t);
		}
	}
}

int main(void)
{
	char videofile[20] = "video.avi";
	cv::VideoCapture capture;
	cv::Mat frame;
	int key = 0;

	VIDEOINFO video;
	std::vector<ORANGETRACK> tracks;
	int nexttrackid = 1;
	int total_laranjas = 0;

	capture.open(videofile);

	if (!capture.isOpened())
	{
		std::cerr << "Erro ao abrir o ficheiro de video!\n";
		return 1;
	}

	video.width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
	video.height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

	int countingliney = (video.height * 60) / 100;
	int countbandtopy = (video.height * 55) / 100;
	int maxblobarea = (video.width * video.height * 70) / 100;

	IVC *image_rgb = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_hsv = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_seg_rgb = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_seg = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_tmp = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_labels = vc_image_new(video.width, video.height, 1, 255);

	if (image_rgb == NULL || image_hsv == NULL || image_seg_rgb == NULL ||
		image_seg == NULL || image_tmp == NULL || image_labels == NULL)
	{
		std::cerr << "Erro ao alocar memoria para imagens IVC!\n";
		vc_image_free(image_rgb);
		vc_image_free(image_hsv);
		vc_image_free(image_seg_rgb);
		vc_image_free(image_seg);
		vc_image_free(image_tmp);
		vc_image_free(image_labels);
		capture.release();
		return 1;
	}

	cv::namedWindow("VC - VIDEO", cv::WINDOW_AUTOSIZE);
	cv::namedWindow("VC - SEGMENTACAO", cv::WINDOW_AUTOSIZE);

	while (key != 'q')
	{
		capture.read(frame);
		if (frame.empty()) break;

		frame_bgr_to_rgb(frame, image_rgb);
		segment_oranges(image_rgb, image_hsv, image_seg_rgb, image_seg, image_tmp);

		int nlabels = 0;
		int nlaranjas = 0;
		std::vector<int> trackused(tracks.size(), 0);
		OVC *blobs = vc_binary_blob_labelling(image_seg, image_labels, &nlabels);

		if (blobs != NULL)
		{
			vc_binary_blob_info(image_labels, blobs, nlabels);

			for (int i = 0; i < nlabels; i++)
			{
				int besttrack;
				std::string str;

				if (!blob_passes_geometry(blobs[i], maxblobarea)) continue;
				if (!blob_passes_color(image_hsv, image_labels, blobs[i])) continue;

				nlaranjas++;

				besttrack = find_best_track(tracks, trackused, blobs[i].xc, blobs[i].yc);

				if (besttrack >= 0)
				{
					update_existing_track(&tracks[besttrack], blobs[i], countingliney, countbandtopy, &total_laranjas);
					trackused[besttrack] = 1;
				}
				else
				{
					add_new_track(tracks, trackused, &nexttrackid, blobs[i], countingliney);
				}

				cv::rectangle(frame,
					cv::Point(blobs[i].x, blobs[i].y),
					cv::Point(blobs[i].x + blobs[i].width, blobs[i].y + blobs[i].height),
					cv::Scalar(0, 255, 0), 2);

				cv::circle(frame,
					cv::Point(blobs[i].xc, blobs[i].yc),
					4,
					cv::Scalar(0, 0, 255),
					-1);

				str = std::string("A=").append(std::to_string(blobs[i].area))
					.append(" P=").append(std::to_string(blobs[i].perimeter));
				draw_text(frame, str, blobs[i].x, blobs[i].y - 10, cv::Scalar(0, 255, 0), 0.5);
			}

			free(blobs);
		}

		remove_missing_tracks(tracks, trackused);

		draw_text(frame, std::string("LARANJAS DETETADAS: ").append(std::to_string(nlaranjas)),
			20, 40, cv::Scalar(0, 165, 255), 0.8);
		draw_text(frame, std::string("TOTAL LARANJAS: ").append(std::to_string(total_laranjas)),
			20, 80, cv::Scalar(255, 255, 0), 0.8);

		cv::line(frame, cv::Point(0, countbandtopy), cv::Point(video.width, countbandtopy), cv::Scalar(255, 255, 0), 2);
		cv::line(frame, cv::Point(0, countingliney), cv::Point(video.width, countingliney), cv::Scalar(0, 200, 255), 1);

		cv::Mat frame_segmentada(video.height, video.width, CV_8UC1, image_seg->data);

		cv::imshow("VC - VIDEO", frame);
		cv::imshow("VC - SEGMENTACAO", frame_segmentada);

		key = cv::waitKey(1);
	}

	vc_image_free(image_rgb);
	vc_image_free(image_hsv);
	vc_image_free(image_seg_rgb);
	vc_image_free(image_seg);
	vc_image_free(image_tmp);
	vc_image_free(image_labels);
	capture.release();
	cv::destroyAllWindows();

	return 0;
}
