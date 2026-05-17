#include <algorithm>
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

namespace {

constexpr int kPixelThreshold = 16;
constexpr double kStaticRatioThreshold = 0.01;

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

double ComputeMotionRatio(const cv::Mat& referenceGray,
                          const cv::Mat& currentGray,
                          cv::Mat& diffGray,
                          cv::Mat& motionMaskRaw)
{
    cv::absdiff(currentGray, referenceGray, diffGray);
    cv::threshold(diffGray, motionMaskRaw, kPixelThreshold, 255, cv::THRESH_BINARY);
    return static_cast<double>(cv::countNonZero(motionMaskRaw)) / static_cast<double>(motionMaskRaw.total());
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

    cv::namedWindow("VC - Frame Atual", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - Plano de Fundo", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - Mascara de Movimento", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - Primeiro Plano", cv::WINDOW_AUTOSIZE);

    cv::Mat frame;
    cv::Mat currentGray;
    cv::Mat previousGray;
    cv::Mat backgroundGray;
    cv::Mat backgroundColor;
    int frameNumber = 0;

    while (capture.read(frame))
    {
        ++frameNumber;
        cv::cvtColor(frame, currentGray, cv::COLOR_BGR2GRAY);

        cv::Mat diffGray = cv::Mat::zeros(currentGray.size(), currentGray.type());
        cv::Mat motionMaskRaw = cv::Mat::zeros(currentGray.size(), currentGray.type());
        cv::Mat motionMask = cv::Mat::zeros(currentGray.size(), currentGray.type());
        cv::Mat foreground = cv::Mat::zeros(frame.size(), frame.type());

        std::string status = "A aguardar fundo estavel";
        double motionRatio = 0.0;
        bool backgroundUpdated = false;

        if (backgroundGray.empty())
        {
            if (!previousGray.empty())
            {
                motionRatio = ComputeMotionRatio(previousGray, currentGray, diffGray, motionMaskRaw);
                if (motionRatio <= kStaticRatioThreshold)
                {
                    backgroundGray = currentGray.clone();
                    backgroundColor = frame.clone();
                    backgroundUpdated = true;
                    status = "Plano de fundo inicializado";
                }
                else
                {
                    status = "A aguardar cena parada para definir o fundo";
                }
            }
        }
        else
        {
            motionRatio = ComputeMotionRatio(backgroundGray, currentGray, diffGray, motionMaskRaw);
            if (motionRatio <= kStaticRatioThreshold)
            {
                backgroundGray = currentGray.clone();
                backgroundColor = frame.clone();
                backgroundUpdated = true;
                status = "Sem movimento: fundo atualizado";
            }
            else
            {
                status = "Movimento detetado: fundo mantido";
            }
        }

        if (!motionMaskRaw.empty())
        {
            cv::morphologyEx(motionMaskRaw, motionMask, cv::MORPH_OPEN, kernel);
            cv::morphologyEx(motionMask, motionMask, cv::MORPH_CLOSE, kernel);
            frame.copyTo(foreground, motionMask);
        }

        cv::Mat frameDisplay = frame.clone();
        DrawText(frameDisplay,
                 status,
                 cv::Point(20, 30),
                 backgroundUpdated ? cv::Scalar(0, 255, 255)
                                   : (motionRatio > kStaticRatioThreshold ? cv::Scalar(0, 0, 255)
                                                                          : cv::Scalar(0, 255, 0)));
        DrawText(frameDisplay, "Fonte: " + sourceLabel, cv::Point(20, 60), cv::Scalar(255, 255, 255));
        DrawText(frameDisplay, "Frame: " + std::to_string(frameNumber), cv::Point(20, 90), cv::Scalar(255, 255, 255));
        DrawText(frameDisplay,
                 "Pixeis > 16: " + std::to_string(motionRatio * 100.0).substr(0, 5) + "%",
                 cv::Point(20, 120),
                 cv::Scalar(255, 255, 255));
        DrawText(frameDisplay, "Sem movimento se <= 1.00%", cv::Point(20, 150), cv::Scalar(255, 255, 255));

        cv::Mat backgroundDisplay = backgroundColor.empty() ? frame.clone() : backgroundColor.clone();
        if (backgroundColor.empty())
        {
            DrawText(backgroundDisplay, "Fundo ainda nao definido", cv::Point(20, 30), cv::Scalar(0, 255, 255));
        }

        cv::imshow("VC - Frame Atual", frameDisplay);
        cv::imshow("VC - Plano de Fundo", backgroundDisplay);
        cv::imshow("VC - Mascara de Movimento", motionMask);
        cv::imshow("VC - Primeiro Plano", foreground);

        previousGray = currentGray.clone();

        const int key = cv::waitKey(waitTime);
        if (key == 'q' || key == 27) break;
    }

    capture.release();
    cv::destroyAllWindows();
    return 0;
}
