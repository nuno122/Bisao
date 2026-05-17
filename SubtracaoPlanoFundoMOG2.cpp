#include <algorithm>
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

namespace {

constexpr double kMotionRatioThreshold = 0.01;

bool OpenVideoSource(int argc, char* argv[], cv::VideoCapture& capture, std::string& sourceLabel)
{
    if (argc > 1)
    {
        const std::string source = argv[1];
        if (source == "0" || source == "camera")
        {
            sourceLabel = "Webcam 0";
            return capture.open(0, cv::CAP_DSHOW);
        }

        sourceLabel = source;
        return capture.open(source);
    }

    sourceLabel = "video.avi";
    return capture.open("video.avi");
}

void DrawText(cv::Mat& image, const std::string& text, const cv::Point& origin, const cv::Scalar& color)
{
    cv::putText(image, text, origin, cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 0, 0), 3);
    cv::putText(image, text, origin, cv::FONT_HERSHEY_SIMPLEX, 0.65, color, 1);
}

} // namespace

int main(int argc, char* argv[])
{
    cv::VideoCapture capture;
    std::string sourceLabel;

    if (!OpenVideoSource(argc, argv, capture, sourceLabel))
    {
        std::cerr << "Erro: nao foi possivel abrir a fonte de video.\n";
        return 1;
    }

    const int fps = static_cast<int>(capture.get(cv::CAP_PROP_FPS));
    const int waitTime = (fps > 0) ? std::max(1, 1000 / fps) : 30;
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));

    cv::Ptr<cv::BackgroundSubtractorMOG2> backgroundSubtractor =
        cv::createBackgroundSubtractorMOG2(500, 16.0, false);

    cv::namedWindow("VC - Frame Atual", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - Fundo MOG2", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - Mascara MOG2", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - Primeiro Plano", cv::WINDOW_AUTOSIZE);

    cv::Mat frame;
    int frameNumber = 0;

    while (capture.read(frame))
    {
        ++frameNumber;

        cv::Mat foregroundMaskRaw;
        cv::Mat foregroundMask;
        cv::Mat foreground = cv::Mat::zeros(frame.size(), frame.type());
        cv::Mat backgroundImage = cv::Mat::zeros(frame.size(), frame.type());

        backgroundSubtractor->apply(frame, foregroundMaskRaw);
        cv::morphologyEx(foregroundMaskRaw, foregroundMask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(foregroundMask, foregroundMask, cv::MORPH_CLOSE, kernel);
        frame.copyTo(foreground, foregroundMask);
        backgroundSubtractor->getBackgroundImage(backgroundImage);

        const double motionRatio =
            static_cast<double>(cv::countNonZero(foregroundMask)) / static_cast<double>(foregroundMask.total());
        const bool motionDetected = motionRatio > kMotionRatioThreshold;

        cv::Mat frameDisplay = frame.clone();
        DrawText(frameDisplay,
                 motionDetected ? "Movimento detetado por MOG2" : "Cena maioritariamente estavel",
                 cv::Point(20, 30),
                 motionDetected ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0));
        DrawText(frameDisplay, "Fonte: " + sourceLabel, cv::Point(20, 60), cv::Scalar(255, 255, 255));
        DrawText(frameDisplay, "Frame: " + std::to_string(frameNumber), cv::Point(20, 90), cv::Scalar(255, 255, 255));
        DrawText(frameDisplay,
                 "Mascara ativa: " + std::to_string(motionRatio * 100.0).substr(0, 5) + "%",
                 cv::Point(20, 120),
                 cv::Scalar(255, 255, 255));

        if (backgroundImage.empty())
        {
            backgroundImage = frame.clone();
            DrawText(backgroundImage, "Modelo de fundo em aprendizagem", cv::Point(20, 30), cv::Scalar(0, 255, 255));
        }

        cv::imshow("VC - Frame Atual", frameDisplay);
        cv::imshow("VC - Fundo MOG2", backgroundImage);
        cv::imshow("VC - Mascara MOG2", foregroundMask);
        cv::imshow("VC - Primeiro Plano", foreground);

        const int key = cv::waitKey(waitTime);
        if (key == 'q' || key == 27) break;
    }

    capture.release();
    cv::destroyAllWindows();
    return 0;
}
