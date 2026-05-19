#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2\highgui.hpp>
#include <opencv2\videoio.hpp>

extern "C" {
#include "vc.h"
}

int hue_in_range(unsigned char hue, int hmin, int hmax)
{
	int hmin255 = (hmin * 255) / 360;
	int hmax255 = (hmax * 255) / 360;

	if (hmin255 <= hmax255) return (hue >= hmin255 && hue <= hmax255);
	return (hue >= hmin255 || hue <= hmax255);
}

int blob_hsv_box_color_count(IVC *image_hsv, OVC blob, int hmin, int hmax, int smin, int vmin)
{
	long int pos;
	int x, y;
	int count = 0;

	if (image_hsv == NULL) return 0;

	for (y = blob.y; y < blob.y + blob.height; y++)
	{
		for (x = blob.x; x < blob.x + blob.width; x++)
		{
			pos = y * image_hsv->bytesperline + x * image_hsv->channels;

			if (hue_in_range(image_hsv->data[pos], hmin, hmax) &&
				image_hsv->data[pos + 1] >= smin &&
				image_hsv->data[pos + 2] >= vmin)
			{
				count++;
			}
		}
	}

	return count;
}

int blob_mask_binary_paint(IVC *mask, IVC *image_labels, OVC blob, unsigned char value)
{
	long int pos_mask, pos_label;
	int x, y;

	if (mask == NULL || image_labels == NULL) return 0;

	for (y = blob.y; y < blob.y + blob.height; y++)
	{
		for (x = blob.x; x < blob.x + blob.width; x++)
		{
			pos_label = y * image_labels->bytesperline + x * image_labels->channels;

			if (image_labels->data[pos_label] == blob.label)
			{
				pos_mask = y * mask->bytesperline + x * mask->channels;
				mask->data[pos_mask] = value;
			}
		}
	}

	return 1;
}

typedef struct
{
	int id;
	int x, y;
	int previousx, previousy;
	int missingframes;
	int counted;
} ORANGETRACK;

int main(void)
{
	char videofile[20] = "video.avi";
	cv::VideoCapture capture;
	cv::Mat frame;
	std::string str;
	int key = 0;

	struct
	{
		int width, height;
		int ntotalframes;
		int fps;
		int nframe;
	} video;

	capture.open(videofile);

	if (!capture.isOpened())
	{
		std::cerr << "Erro ao abrir o ficheiro de video!\n";
		return 1;
	}

	video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
	video.fps = (int)capture.get(cv::CAP_PROP_FPS);
	video.width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
	video.height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

	IVC *image_rgb = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_hsv = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_seg_rgb = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_seg = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_tmp = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_labels = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_final = vc_image_new(video.width, video.height, 1, 255);

	if (image_rgb == NULL || image_hsv == NULL || image_seg_rgb == NULL ||
		image_seg == NULL || image_tmp == NULL || image_labels == NULL || image_final == NULL)
	{
		std::cerr << "Erro ao alocar memoria para imagens IVC!\n";
		vc_image_free(image_rgb);
		vc_image_free(image_hsv);
		vc_image_free(image_seg_rgb);
		vc_image_free(image_seg);
		vc_image_free(image_tmp);
		vc_image_free(image_labels);
		vc_image_free(image_final);
		capture.release();
		return 1;
	}

	cv::namedWindow("VC - VIDEO", cv::WINDOW_AUTOSIZE);
	cv::namedWindow("VC - SEGMENTACAO", cv::WINDOW_AUTOSIZE);
	cv::namedWindow("VC - LARANJAS", cv::WINDOW_AUTOSIZE);

	std::vector<ORANGETRACK> tracks;
	int nexttrackid = 1;
	int countingliney = video.height / 2;
	int total_laranjas = 0;

	while (key != 'q')
	{
		capture.read(frame);
		if (frame.empty()) break;

		video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

		for (int y = 0; y < video.height; y++)
		{
			unsigned char *data = frame.ptr<unsigned char>(y);

			for (int x = 0; x < video.width; x++)
			{
				long int pos = y * image_rgb->bytesperline + x * image_rgb->channels;
				image_rgb->data[pos] = data[x * 3 + 2];
				image_rgb->data[pos + 1] = data[x * 3 + 1];
				image_rgb->data[pos + 2] = data[x * 3];
			}
		}

		vc_rgb_to_hsv(image_rgb, image_hsv);
		vc_hsv_segmentation(image_hsv, image_seg_rgb, 8, 45, 35, 255, 35, 255);
		vc_rgb_to_gray(image_seg_rgb, image_seg);

		memset(image_tmp->data, 0, image_tmp->bytesperline * image_tmp->height);
		vc_binary_close(image_seg, image_tmp, 3, 3);
		memset(image_seg->data, 0, image_seg->bytesperline * image_seg->height);
		vc_binary_open(image_tmp, image_seg, 3, 3);
		memset(image_final->data, 0, image_final->bytesperline * image_final->height);

		int nlabels = 0;
		int nsegmentados = 0;
		int ngeometria = 0;
		int nlaranjas = 0;
		std::vector<int> trackused(tracks.size(), 0);
		OVC *blobs = vc_binary_blob_labelling(image_seg, image_labels, &nlabels);

		if (blobs != NULL)
		{
			vc_binary_blob_info(image_labels, blobs, nlabels);

			for (int i = 0; i < nlabels; i++)
			{
				float aspectratio;
				float circularidade;
				int orangepixels;
				int redpixels;
				int yellowpixels;
				int greenpixels;
				int totalcolorpixels;

				if (blobs[i].height == 0 || blobs[i].perimeter == 0) continue;
				if (blobs[i].area < 1500 || blobs[i].area > 150000) continue;

				nsegmentados++;

				aspectratio = (float)blobs[i].width / (float)blobs[i].height;
				circularidade = (float)((4.0 * 3.1415926535 * blobs[i].area) /
					((double)blobs[i].perimeter * (double)blobs[i].perimeter));

				if (aspectratio < 0.60f || aspectratio > 1.60f) continue;
				if (circularidade < 0.45f) continue;

				ngeometria++;

				orangepixels = blob_hsv_box_color_count(image_hsv, blobs[i], 10, 35, 35, 35);
				redpixels = blob_hsv_box_color_count(image_hsv, blobs[i], 340, 15, 35, 35);
				yellowpixels = blob_hsv_box_color_count(image_hsv, blobs[i], 36, 65, 35, 35);
				greenpixels = blob_hsv_box_color_count(image_hsv, blobs[i], 66, 140, 25, 25);
				totalcolorpixels = orangepixels + redpixels + yellowpixels + greenpixels;

				if (totalcolorpixels == 0) continue;
				if (orangepixels < (totalcolorpixels * 30 / 100)) continue;
				if (orangepixels <= redpixels) continue;
				if (orangepixels <= yellowpixels) continue;
				if (orangepixels <= greenpixels) continue;
				if (redpixels > (totalcolorpixels * 25 / 100)) continue;
				if (yellowpixels > (totalcolorpixels * 45 / 100)) continue;
				if (greenpixels > (totalcolorpixels * 15 / 100)) continue;

				nlaranjas++;
				blob_mask_binary_paint(image_final, image_labels, blobs[i], 255);

				int besttrack = -1;
				double bestdistance = 180.0;

				for (int t = 0; t < (int)tracks.size(); t++)
				{
					double dx, dy, distance;

					if (trackused[t] != 0) continue;

					dx = (double)blobs[i].xc - (double)tracks[t].x;
					dy = (double)blobs[i].yc - (double)tracks[t].y;
					distance = sqrt((dx * dx) + (dy * dy));

					if (distance < bestdistance)
					{
						bestdistance = distance;
						besttrack = t;
					}
				}

				if (besttrack >= 0)
				{
					int previousside;
					int currentside;

					tracks[besttrack].previousx = tracks[besttrack].x;
					tracks[besttrack].previousy = tracks[besttrack].y;
					tracks[besttrack].x = blobs[i].xc;
					tracks[besttrack].y = blobs[i].yc;
					tracks[besttrack].missingframes = 0;
					trackused[besttrack] = 1;

					previousside = (tracks[besttrack].previousy < countingliney) ? 0 : 1;
					currentside = (tracks[besttrack].y < countingliney) ? 0 : 1;

					if ((tracks[besttrack].counted == 0) &&
						(previousside != currentside))
					{
						tracks[besttrack].counted = 1;
						total_laranjas++;
					}
				}
				else
				{
					ORANGETRACK newtrack;
					newtrack.id = nexttrackid++;
					newtrack.x = blobs[i].xc;
					newtrack.y = blobs[i].yc;
					newtrack.previousx = blobs[i].xc;
					newtrack.previousy = blobs[i].yc;
					newtrack.missingframes = 0;
					newtrack.counted = 0;

					tracks.push_back(newtrack);
					trackused.push_back(1);
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

				if (besttrack >= 0)
				{
					str = std::string("ID=").append(std::to_string(tracks[besttrack].id));
				}
				else
				{
					str = std::string("ID=").append(std::to_string(nexttrackid - 1));
				}

				cv::putText(frame, str, cv::Point(blobs[i].x, blobs[i].y - 10),
					cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
			}

			free(blobs);
		}

		for (int t = 0; t < (int)tracks.size(); t++)
		{
			if (trackused[t] == 0) tracks[t].missingframes++;
		}

		for (int t = (int)tracks.size() - 1; t >= 0; t--)
		{
			if (tracks[t].missingframes > 15)
			{
				tracks.erase(tracks.begin() + t);
			}
		}

		str = std::string("RESOLUCAO: ").append(std::to_string(video.width)).append("x").append(std::to_string(video.height));
		cv::putText(frame, str, cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
		cv::putText(frame, str, cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);

		str = std::string("TOTAL DE FRAMES: ").append(std::to_string(video.ntotalframes));
		cv::putText(frame, str, cv::Point(20, 60), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
		cv::putText(frame, str, cv::Point(20, 60), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);

		str = std::string("FRAME RATE: ").append(std::to_string(video.fps));
		cv::putText(frame, str, cv::Point(20, 90), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
		cv::putText(frame, str, cv::Point(20, 90), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);

		str = std::string("N. DA FRAME: ").append(std::to_string(video.nframe));
		cv::putText(frame, str, cv::Point(20, 120), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
		cv::putText(frame, str, cv::Point(20, 120), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);

		str = std::string("BLOBS SEGMENTADOS: ").append(std::to_string(nsegmentados));
		cv::putText(frame, str, cv::Point(20, 150), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
		cv::putText(frame, str, cv::Point(20, 150), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);

		str = std::string("BLOBS GEOMETRIA: ").append(std::to_string(ngeometria));
		cv::putText(frame, str, cv::Point(20, 180), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);
		cv::putText(frame, str, cv::Point(20, 180), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);

		str = std::string("LARANJAS DETETADAS: ").append(std::to_string(nlaranjas));
		cv::putText(frame, str, cv::Point(20, 210), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 165, 255), 2);
		cv::putText(frame, str, cv::Point(20, 210), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);

		str = std::string("TOTAL LARANJAS: ").append(std::to_string(total_laranjas));
		cv::putText(frame, str, cv::Point(20, 240), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 0), 2);
		cv::putText(frame, str, cv::Point(20, 240), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);

		cv::line(frame, cv::Point(0, countingliney), cv::Point(video.width, countingliney), cv::Scalar(255, 255, 0), 2);

		cv::Mat frame_segmentada(video.height, video.width, CV_8UC1, image_seg->data);
		cv::Mat frame_laranjas(video.height, video.width, CV_8UC1, image_final->data);

		cv::imshow("VC - VIDEO", frame);
		cv::imshow("VC - SEGMENTACAO", frame_segmentada);
		cv::imshow("VC - LARANJAS", frame_laranjas);

		key = cv::waitKey(1);
	}

	vc_image_free(image_rgb);
	vc_image_free(image_hsv);
	vc_image_free(image_seg_rgb);
	vc_image_free(image_seg);
	vc_image_free(image_tmp);
	vc_image_free(image_labels);
	vc_image_free(image_final);
	capture.release();
	cv::destroyAllWindows();

	return 0;
}
