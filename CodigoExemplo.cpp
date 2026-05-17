#include <iostream>
#include <string>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2\highgui.hpp>
#include <opencv2\videoio.hpp>

extern "C" {
#include "vc.h"
}


void vc_timer(void) {
	static bool running = false;
	static std::chrono::steady_clock::time_point previousTime = std::chrono::steady_clock::now();

	if (!running) {
		running = true;
	}
	else {
		std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
		std::chrono::steady_clock::duration elapsedTime = currentTime - previousTime;

		// Tempo em segundos.
		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(elapsedTime);
		double nseconds = time_span.count();

		std::cout << "Tempo decorrido: " << nseconds << "segundos" << std::endl;
		std::cout << "Pressione qualquer tecla para continuar...\n";
		std::cin.get();
	}
}

int blob_rgb_mean(IVC *image_rgb, IVC *image_labels, OVC blob, int *meanr, int *meang, int *meanb)
{
	long int pos_rgb, pos_label;
	long int sumr = 0, sumg = 0, sumb = 0;
	int x, y;
	int count = 0;

	if (image_rgb == NULL || image_labels == NULL || meanr == NULL || meang == NULL || meanb == NULL) return 0;

	for (y = blob.y; y < blob.y + blob.height; y++)
	{
		for (x = blob.x; x < blob.x + blob.width; x++)
		{
			pos_label = y * image_labels->bytesperline + x * image_labels->channels;

			if (image_labels->data[pos_label] == blob.label)
			{
				pos_rgb = y * image_rgb->bytesperline + x * image_rgb->channels;
				sumr += image_rgb->data[pos_rgb];
				sumg += image_rgb->data[pos_rgb + 1];
				sumb += image_rgb->data[pos_rgb + 2];
				count++;
			}
		}
	}

	if (count == 0) return 0;

	*meanr = (int)(sumr / count);
	*meang = (int)(sumg / count);
	*meanb = (int)(sumb / count);

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


int main(void) {
	// Vídeo
	char videofile[20] = "video.avi";
	cv::VideoCapture capture;
	struct
	{
		int width, height;
		int ntotalframes;
		int fps;
		int nframe;
	} video;
	// Outros
	std::string str;
	int key = 0;

	/* Leitura de vídeo de um ficheiro */
	/* NOTA IMPORTANTE:
	O ficheiro video.avi deverá estar localizado no mesmo directório que o ficheiro de código fonte.
	*/
	capture.open(videofile);

	/* Em alternativa, abrir captura de vídeo pela Webcam #0 */
	//capture.open(0, cv::CAP_DSHOW); // Pode-se utilizar apenas capture.open(0);

	/* Verifica se foi possível abrir o ficheiro de vídeo */
	if (!capture.isOpened())
	{
		std::cerr << "Erro ao abrir o ficheiro de vídeo!\n";
		return 1;
	}

	/* Número total de frames no vídeo */
	video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
	/* Frame rate do vídeo */
	video.fps = (int)capture.get(cv::CAP_PROP_FPS);
	/* Resolução do vídeo */
	video.width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
	video.height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

	/* Imagens IVC para processamento */
	IVC *image_rgb = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_hsv = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_seg_rgb = vc_image_new(video.width, video.height, 3, 255);
	IVC *image_seg = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_tmp1 = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_tmp2 = vc_image_new(video.width, video.height, 1, 255);
	IVC *image_labels = vc_image_new(video.width, video.height, 1, 255);

	if (image_rgb == NULL || image_hsv == NULL || image_seg_rgb == NULL || image_seg == NULL ||
		image_tmp1 == NULL || image_tmp2 == NULL || image_labels == NULL)
	{
		std::cerr << "Erro ao alocar memoria para imagens IVC!\n";
		vc_image_free(image_rgb);
		vc_image_free(image_hsv);
		vc_image_free(image_seg_rgb);
		vc_image_free(image_seg);
		vc_image_free(image_tmp1);
		vc_image_free(image_tmp2);
		vc_image_free(image_labels);
		capture.release();
		return 1;
	}

	/* Cria uma janela para exibir o vídeo */
	cv::namedWindow("VC - VIDEO", cv::WINDOW_AUTOSIZE);
	cv::namedWindow("VC - SEGMENTACAO", cv::WINDOW_AUTOSIZE);

	/* Inicia o timer */
	vc_timer();

	std::vector<ORANGETRACK> tracks;
	int nexttrackid = 1;
	int total_laranjas = 0;
	int countinglinex = video.width / 2;

	cv::Mat frame;
	while (key != 'q') {
		/* Leitura de uma frame do vídeo */
		capture.read(frame);

		/* Verifica se conseguiu ler a frame */
		if (frame.empty()) break;

		/* Número da frame a processar */
		video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

		// Faça o seu código aqui...
		// OpenCV usa BGR, enquanto a nossa biblioteca vc trabalha em RGB.
		for (int y = 0; y < video.height; y++)
		{
			unsigned char *data = frame.ptr<unsigned char>(y);

			for (int x = 0; x < video.width; x++)
			{
				long int pos = y * image_rgb->bytesperline + x * image_rgb->channels;

				image_rgb->data[pos] = data[x * 3 + 2];     // R
				image_rgb->data[pos + 1] = data[x * 3 + 1]; // G
				image_rgb->data[pos + 2] = data[x * 3];     // B
			}
		}

		// Converte a frame atual de RGB para HSV.
		vc_rgb_to_hsv(image_rgb, image_hsv);

		// Segmentação inicial da cor laranja em HSV.
		vc_hsv_segmentation(image_hsv, image_seg_rgb, 8, 30, 40, 255, 40, 255);

		// Conversão da segmentação RGB binária para 1 canal.
		vc_rgb_to_gray(image_seg_rgb, image_seg);

		// Limpeza da máscara sem alocar imagens novas em cada frame.
		memset(image_tmp1->data, 0, image_tmp1->bytesperline * image_tmp1->height);
		vc_binary_erode(image_seg, image_tmp1, 3);
		memset(image_tmp2->data, 0, image_tmp2->bytesperline * image_tmp2->height);
		vc_binary_dilate(image_tmp1, image_tmp2, 3);
		memset(image_tmp1->data, 0, image_tmp1->bytesperline * image_tmp1->height);
		vc_binary_dilate(image_tmp2, image_tmp1, 5);
		memset(image_seg->data, 0, image_seg->bytesperline * image_seg->height);
		vc_binary_erode(image_tmp1, image_seg, 5);

		// Etiquetagem de blobs na máscara limpa.
		int nlabels = 0;
		int nlaranjas = 0;
		int nblobsgeometria = 0;
		std::vector<int> trackused(tracks.size(), 0);
		OVC *blobs = vc_binary_blob_labelling(image_seg, image_labels, &nlabels);

		if (blobs != NULL)
		{
			for (int i = 0; i < nlabels; i++) blobs[i].perimeter = 0;

			vc_binary_blob_info(image_labels, blobs, nlabels);

			for (int i = 0; i < nlabels; i++)
			{
				float aspectratio;
				float circularidade;
				int meanr, meang, meanb;

				if (blobs[i].height == 0 || blobs[i].perimeter == 0) continue;

				aspectratio = (float)blobs[i].width / (float)blobs[i].height;
				circularidade = (float)((4.0 * 3.1415926535 * blobs[i].area) /
					((double)blobs[i].perimeter * (double)blobs[i].perimeter));

				// Filtros iniciais para rejeitar ruído e objetos que não parecem laranjas.
				if (blobs[i].area < 1500 || blobs[i].area > 120000) continue;
				if (aspectratio < 0.60f || aspectratio > 1.45f) continue;
				if (circularidade < 0.20f) continue;
				nblobsgeometria++;
				if (!blob_rgb_mean(image_rgb, image_labels, blobs[i], &meanr, &meang, &meanb)) continue;
				if (meanr < 90) continue;
				if (meanr <= meanb) continue;
				if (meang > meanr + 25) continue;

				nlaranjas++;

				int besttrack = -1;
				double bestdistance = 120.0;

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
					tracks[besttrack].previousx = tracks[besttrack].x;
					tracks[besttrack].previousy = tracks[besttrack].y;
					tracks[besttrack].x = blobs[i].xc;
					tracks[besttrack].y = blobs[i].yc;
					tracks[besttrack].missingframes = 0;
					trackused[besttrack] = 1;

					if ((tracks[besttrack].counted == 0) &&
						(tracks[besttrack].previousx > countinglinex) &&
						(tracks[besttrack].x <= countinglinex))
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

					if (blobs[i].xc <= countinglinex)
					{
						newtrack.counted = 1;
						total_laranjas++;
					}

					tracks.push_back(newtrack);
					trackused.push_back(1);
				}

				cv::rectangle(frame,
					cv::Point(blobs[i].x, blobs[i].y),
					cv::Point(blobs[i].x + blobs[i].width, blobs[i].y + blobs[i].height),
					cv::Scalar(0, 255, 0), 2);
				cv::circle(frame, cv::Point(blobs[i].xc, blobs[i].yc), 4, cv::Scalar(0, 0, 255), -1);

				str = std::string("A=").append(std::to_string(blobs[i].area))
					.append(" P=").append(std::to_string(blobs[i].perimeter));
				cv::putText(frame, str, cv::Point(blobs[i].x, blobs[i].y - 10),
					cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

				str = std::string("R=").append(std::to_string(meanr))
					.append(" G=").append(std::to_string(meang))
					.append(" B=").append(std::to_string(meanb));
				cv::putText(frame, str, cv::Point(blobs[i].x, blobs[i].y - 28),
					cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 255, 255), 1);
			}

			free(blobs);

		}
		for (int t = 0; t < (int)tracks.size(); t++)
		{
			if (trackused[t] == 0) tracks[t].missingframes++;
		}

		for (int t = (int)tracks.size() - 1; t >= 0; t--)
		{
			if (tracks[t].missingframes > 10)
			{
				tracks.erase(tracks.begin() + t);
			}
		}

		cv::line(frame, cv::Point(countinglinex, 0), cv::Point(countinglinex, video.height), cv::Scalar(255, 255, 0), 2);
		str = std::string("LARANJAS DETETADAS: ").append(std::to_string(nlaranjas));
		cv::putText(frame, str, cv::Point(20, 130), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
		cv::putText(frame, str, cv::Point(20, 130), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);
		str = std::string("BLOBS APOS GEOMETRIA: ").append(std::to_string(nblobsgeometria));
		cv::putText(frame, str, cv::Point(20, 155), cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(255, 255, 0), 2);
		cv::putText(frame, str, cv::Point(20, 155), cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 0, 0), 1);
		str = std::string("TOTAL LARANJAS: ").append(std::to_string(total_laranjas));
		cv::putText(frame, str, cv::Point(20, 180), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 165, 255), 2);
		cv::putText(frame, str, cv::Point(20, 180), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 1);
		// +++++++++++++++++++++++++

		/* Exemplo de inserção texto na frame */
		str = std::string("RESOLUCAO: ").append(std::to_string(video.width)).append("x").append(std::to_string(video.height));
		cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
		str = std::string("TOTAL DE FRAMES: ").append(std::to_string(video.ntotalframes));
		cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
		str = std::string("FRAME RATE: ").append(std::to_string(video.fps));
		cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
		str = std::string("N. DA FRAME: ").append(std::to_string(video.nframe));
		cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);

		/* Exibe a frame */
		cv::imshow("VC - VIDEO", frame);

		/* Exibe a máscara segmentada */
		cv::Mat frame_segmentada(video.height, video.width, CV_8UC1, image_seg->data);
		cv::imshow("VC - SEGMENTACAO", frame_segmentada);

		/* Sai da aplicação, se o utilizador premir a tecla 'q' */
		key = cv::waitKey(1);
	}

	/* Para o timer e exibe o tempo decorrido */
	vc_timer();

	/* Fecha a janela */
	cv::destroyWindow("VC - VIDEO");
	cv::destroyWindow("VC - SEGMENTACAO");

	/* Fecha o ficheiro de vídeo */
	capture.release();

	/* Liberta memória das imagens IVC */
	vc_image_free(image_rgb);
	vc_image_free(image_hsv);
	vc_image_free(image_seg_rgb);
	vc_image_free(image_seg);
	vc_image_free(image_tmp1);
	vc_image_free(image_tmp2);
	vc_image_free(image_labels);

	return 0;
}
