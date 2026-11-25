#include "render/ui_widgets.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace ui
{
    cv::Scalar playerColor(int p) { return p == 0 ? cv::Scalar(40, 150, 255) : cv::Scalar(80, 230, 100); }

    void fillTranslucent(cv::Mat &img, const cv::Rect &r, const cv::Scalar &col, double alpha)
    {
        const cv::Rect vis = r & cv::Rect(0, 0, img.cols, img.rows);
        if (vis.empty())
            return;
        cv::Mat roi = img(vis);
        if (alpha >= 1.0)
        {
            roi.setTo(col);
            return;
        }
        cv::Mat solid(roi.size(), roi.type(), col);
        cv::addWeighted(solid, alpha, roi, 1.0 - alpha, 0.0, roi);
    }

    void panel(cv::Mat &img, const cv::Rect &r, const cv::Scalar &accent, double fillAlpha, const cv::Scalar &fill,
               int border)
    {
        fillTranslucent(img, r, fill, fillAlpha);
        const cv::Scalar dark(12, 8, 16);
        cv::rectangle(img, r, dark, border + 2);
        cv::rectangle(img, r, accent * 0.75, border);
        const cv::Rect inner(r.x + border + 3, r.y + border + 3, std::max(1, r.width - 2 * (border + 3)),
                             std::max(1, r.height - 2 * (border + 3)));
        cv::rectangle(img, inner, accent * 0.35, 1);

        // cantoneiras em blocos
        const int b = std::max(4, border + 2);
        const cv::Scalar hi = accent * 0.4 + cv::Scalar(255, 255, 255) * 0.6;
        const cv::Point corners[4] = {r.tl(), {r.br().x - b, r.y}, {r.x, r.br().y - b}, {r.br().x - b, r.br().y - b}};
        for (const cv::Point &c : corners)
        {
            cv::rectangle(img, cv::Rect(c.x - 1, c.y - 1, b + 2, b + 2), dark, cv::FILLED);
            cv::rectangle(img, cv::Rect(c.x, c.y, b, b), hi, cv::FILLED);
        }
    }

    void panelTitle(cv::Mat &img, const cv::Rect &pr, const std::string &title, const cv::Scalar &accent, double scale)
    {
        int base = 0;
        const cv::Size ts = cv::getTextSize(title, cv::FONT_HERSHEY_DUPLEX, scale, 1, &base);
        const int h = ts.height + 14;
        const cv::Rect band(pr.x + pr.width / 2 - ts.width / 2 - 16, pr.y - h / 2, ts.width + 32, h);
        fillTranslucent(img, band, cv::Scalar(20, 14, 28), 1.0);
        cv::rectangle(img, band, accent, 2);
        const cv::Point org(band.x + 16, band.y + (h + ts.height) / 2);
        cv::putText(img, title, org + cv::Point(1, 1), cv::FONT_HERSHEY_DUPLEX, scale, cv::Scalar(10, 6, 14), 2,
                    cv::LINE_AA);
        cv::putText(img, title, org, cv::FONT_HERSHEY_DUPLEX, scale, accent * 0.3 + cv::Scalar(255, 255, 255) * 0.7, 1,
                    cv::LINE_AA);
    }

    std::vector<std::string> wrapText(const std::string &text, int font, double scale, int thick, int maxW)
    {
        std::vector<std::string> lines;
        std::istringstream in(text);
        std::string word, cur;
        int base = 0;
        while (in >> word)
        {
            const std::string test = cur.empty() ? word : cur + " " + word;
            if (!cur.empty() && cv::getTextSize(test, font, scale, thick, &base).width > maxW)
            {
                lines.push_back(cur);
                cur = word;
            }
            else
                cur = test;
        }
        if (!cur.empty())
            lines.push_back(cur);
        return lines;
    }

    int putWrapped(cv::Mat &img, const std::string &text, cv::Point org, int maxW, int font, double scale,
                   const cv::Scalar &col, int thick, int lineGap)
    {
        int base = 0;
        const int lh = cv::getTextSize("Ag", font, scale, thick, &base).height + lineGap;
        int y = org.y;
        for (const std::string &l : wrapText(text, font, scale, thick, maxW))
        {
            cv::putText(img, l, {org.x, y}, font, scale, col, thick, cv::LINE_AA);
            y += lh;
        }
        return y;
    }

    double fitScale(const std::string &text, int font, double maxScale, int thick, int maxW)
    {
        int base = 0;
        const int w = cv::getTextSize(text, font, maxScale, thick, &base).width;
        if (w <= maxW || w <= 0)
            return maxScale;
        return std::max(0.3, maxScale * double(maxW) / w);
    }

    void drawStar(cv::Mat &img, cv::Point c, int r, const cv::Scalar &col, bool filled)
    {
        cv::Point pts[10];
        for (int i = 0; i < 10; ++i)
        {
            const double a = -CV_PI / 2 + i * CV_PI / 5;
            const double rr = (i % 2) ? r * 0.45 : r;
            pts[i] = cv::Point(cvRound(c.x + std::cos(a) * rr), cvRound(c.y + std::sin(a) * rr));
        }
        const cv::Point *pp = pts;
        const int n = 10;
        if (filled)
            cv::fillPoly(img, &pp, &n, 1, col, cv::LINE_AA);
        cv::polylines(img, &pp, &n, 1, true, filled ? cv::Scalar(20, 12, 25) : col, filled ? 1 : 2, cv::LINE_AA);
    }

    void pausedOverlay(cv::Mat &img)
    {
        if (img.empty())
            return;
        const char *txt = "PAUSADO";
        const double scale = std::max(1.0, img.cols / 900.0);
        int baseline = 0;
        const cv::Size ts = cv::getTextSize(txt, cv::FONT_HERSHEY_DUPLEX, scale, 3, &baseline);
        const cv::Point org((img.cols - ts.width) / 2, (img.rows + ts.height) / 2);
        const cv::Rect box(org.x - 28, org.y - ts.height - 18, ts.width + 56, ts.height + 36);
        cv::rectangle(img, box, cv::Scalar(12, 10, 20), cv::FILLED);
        cv::rectangle(img, box, cv::Scalar(120, 230, 255), 2, cv::LINE_AA);
        cv::putText(img, txt, org, cv::FONT_HERSHEY_DUPLEX, scale, cv::Scalar(140, 245, 255), 3, cv::LINE_AA);
    }

    void drawShapeIcon(cv::Mat &img, Shape s, const cv::Rect &box, const cv::Scalar &col, int thick)
    {
        const int m = std::max(2, std::min(box.width, box.height) / 6);
        const cv::Rect b(box.x + m, box.y + m, std::max(1, box.width - 2 * m), std::max(1, box.height - 2 * m));
        const cv::Point c(b.x + b.width / 2, b.y + b.height / 2);
        const int r = std::min(b.width, b.height) / 2;
        switch (s)
        {
        case Shape::Linha:
            cv::line(img, {b.x, c.y}, {b.br().x, c.y}, col, thick, cv::LINE_AA);
            break;
        case Shape::Raio:
        {
            const cv::Point z[4] = {{b.x, b.y}, {b.br().x - b.width / 4, c.y - r / 4},
                                    {b.x + b.width / 4, c.y + r / 4}, {b.br().x, b.br().y}};
            for (int i = 0; i < 3; ++i)
                cv::line(img, z[i], z[i + 1], col, thick, cv::LINE_AA);
            break;
        }
        case Shape::Circulo:
            cv::circle(img, c, r, col, thick, cv::LINE_AA);
            break;
        case Shape::Triangulo:
        {
            const cv::Point t[3] = {{c.x, c.y - r}, {c.x + r, c.y + r * 3 / 4}, {c.x - r, c.y + r * 3 / 4}};
            for (int i = 0; i < 3; ++i)
                cv::line(img, t[i], t[(i + 1) % 3], col, thick, cv::LINE_AA);
            break;
        }
        case Shape::Estrela:
            drawStar(img, c, r, col, false);
            break;
        default:
            break;
        }
    }
} // namespace ui
