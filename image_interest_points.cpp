#include "image_interest_points.h"

#include "filesys.h"
#include <fstream>

void CImageComponentsDescriptorBase::detectRegions(int mode1, int mode2)
{
    Mat src = original_image.clone();
    // Convert image to grayscale
    Mat gray;

    if (isGrayScaleImage(src) == false)
    {
        cvtColor(src, gray, COLOR_BGR2GRAY);
    }
    else
    {
        gray = src.clone();
    }

    // Convert image to binary
    Mat bw;
    threshold(gray, bw, 50, 255, THRESH_BINARY | THRESH_OTSU);
    // Find all the contours in the thresholded image
    findContours(bw, raw_contourns, mode1, mode2);
}

std::pair<int, int> CImageComponentsDescriptorBase::getCentroid(cv::Moments& momInertia) const
{
    int cx = momInertia.m10 / momInertia.m00;
    int cy = momInertia.m01 / momInertia.m00;
    std::pair<int, int> p(cx, cy);
    return p;
}

double CImageComponentsDescriptorBase::getArea(std::vector<cv::Point>& region) const
{
    return contourArea(region);
}

double CImageComponentsDescriptorBase::getPerimeter(std::vector<cv::Point>& region, bool closed) const
{
    return arcLength(region, closed);
}


double CImageComponentsDescriptorBase::getRoundNess(std::vector<cv::Point>& region)
{
    double Area = getArea(region);
    double Perimeter = getPerimeter(region);

    return 4 * CV_PI * Area / pow(Perimeter,2);
}

double CImageComponentsDescriptorBase::getOrientation(cv::Moments& momInertia) const
{
    double u11 = momInertia.m11;
    double u20 = momInertia.m20;
    double u02 = momInertia.m02;

    double factor = ( 2 * u11 )/ (u20 - u02);
    double angle =  0.5 * atan(factor);

    double degrees = angle * (180.0 / CV_PI);
    return degrees;
}


void CImageComponentsDescriptorNormal::getObjectsInfo()
{
    for (const auto& c : raw_contourns)
    {
        // declare the region
        std::vector<Point> original = c;
        cv::Moments momInertia = cv::moments(cv::Mat(original));
        ImageComponentsDescriptor.region = original;
        ImageComponentsDescriptor.momInertia = momInertia;
        ImageComponentsDescriptor.convex = isContourConvex(original);
        Objects.push_back(ImageComponentsDescriptor);
    }
}

void CImageComponentsDescriptorHull::getObjectsInfo()
{
    for (const auto& c : raw_contourns)
    {
        // declare the region
        std::vector<Point> hull;
        convexHull(c, hull);
        Moments momInertiaHull = cv::moments(cv::Mat(hull));
        ImageComponentsDescriptor.region = hull;
        ImageComponentsDescriptor.momInertia = momInertiaHull;
        ImageComponentsDescriptor.convex = isContourConvex(hull);
        Objects.push_back(ImageComponentsDescriptor);
    }
}


void CImageComponentsDescriptorAprox::getObjectsInfo()
{
    for (const auto& c : raw_contourns)
    {
        // declare the region
        std::vector<Point> Aprox;
        double epsilon = 0.1 * arcLength(c, true);
        approxPolyDP(c, Aprox, epsilon, true);
        cv::Moments momInertiaAprox = cv::moments(cv::Mat(Aprox));
        ImageComponentsDescriptor.region = Aprox;
        ImageComponentsDescriptor.momInertia = momInertiaAprox;
        ImageComponentsDescriptor.convex = isContourConvex(Aprox);
        Objects.push_back(ImageComponentsDescriptor);
    }
}

template<typename T>
void CCompare<T>::calculateDescriptors()
{
    std::unique_ptr<T> descriptor1{ new T{img1} };
    std::unique_ptr<T> descriptor2{ new T{img2} };

    descriptor1->detectRegions();
    descriptor1->getObjectsInfo();
    _imag1_descriptions = descriptor1->getImageFullInformation();

    descriptor2->detectRegions();
    descriptor2->getObjectsInfo();
    _imag2_descriptions = descriptor2->getImageFullInformation();
}

namespace image_info
{

    Descriptors getImageDescriptors(const Mat& img)
    {

        Descriptors out;

        CImageComponentsDescriptorHull hull(img);

        hull.detectRegions(CHAIN_APPROX_SIMPLE);
        hull.getObjectsInfo();
        ObjectsCollection Information = hull.getImageFullInformation();
        int objectsIndex = 0;
        for (auto& object : Information)
        {

            double Area = hull.getArea(object.region);
            double perimeter = arcLength(object.region, true);
            double r_factor = hull.getRoundNess(object.region);
            double orientation = hull.getOrientation(object.momInertia);

            std::pair<int, int> centroid = hull.getCentroid(object.momInertia);

            if (Area < 1e2 || 1e5 < Area) continue;

            ImageDescriptors d;

            d.Area = Area;
            d.perimeter = perimeter;
            d.r_factor = r_factor;
            d.orientation = orientation;
            d.convex = object.convex;
            d.centroid = centroid;

            out.emplace_back(d);
        }

        createCSV(out);
        return out;
    }

    std::stringstream getImageInfoMoments(const Mat& img, int opt)
    {

        Mat clone = img.clone();

        if (opt == 0)
        {
            std::vector<wxString> choices = {
                                                "Normal algorithm",
                                                "Hull",
                                                "Aproximation"
            };
            wxSingleChoiceDialog dialog(
                NULL,
                "Algorithm for print info",
                "Algorithm for print info",
                static_cast<int>(choices.size()), choices.data());

            dialog.ShowModal();

            CImageComponentsDescriptorBase* base = nullptr;

            wxString algo = dialog.GetStringSelection();
            if (algo == "Normal algorithm")
            {
                base = new CImageComponentsDescriptorNormal(img);
            }
            else
            if (algo == "Hull")
            {
                base = new CImageComponentsDescriptorHull(img);
            }
            else
            if (algo == "Aproximation")
            {
                base = new CImageComponentsDescriptorAprox(img);
            }
            else
            {
                base = new CImageComponentsDescriptorNormal(img);
            }

            return Apply(base, clone);
        }
        else
        {
            CImageComponentsDescriptorNormal* desc =  new CImageComponentsDescriptorNormal(img);
            return Apply(desc, clone);
        }

    }

    std::stringstream Apply(CImageComponentsDescriptorBase* base, Mat& img)
    {

        std::stringstream os;

        base->detectRegions(CHAIN_APPROX_SIMPLE);
        base->getObjectsInfo();
        ObjectsCollection Information = base->getImageFullInformation();
        int objectsIndex = 0;
        for (auto& object : Information)
        {

            double Area = base->getArea(object.region);
            double perimeter = arcLength(object.region, true);
            double r_factor = base->getRoundNess(object.region);
            double orientation = base->getOrientation(object.momInertia);

            std::pair<int, int> centroid = base->getCentroid(object.momInertia);

            if (Area < 1e2 || 1e5 < Area) continue;

            ImageDescriptors d;

            d.Area = Area;
            d.perimeter = perimeter;
            d.r_factor = r_factor;
            d.orientation = orientation;
            d.convex = object.convex;
            d.centroid = centroid;

            os << "Region " << objectsIndex << std::endl;
            os << d;
   
            objectsIndex++;
        }

        if (base != nullptr)
        {
            delete base;
        }

        return os;
    }

    void createCSV(Descriptors& descriptors, std::string fname)
    {
        std::ofstream myfile(fname);
        int i = 0;
        if (myfile.is_open())
        {
            for (const auto& descriptor : descriptors)
            {
                if (i == 0)
                {
                    myfile << "Area,Perimeter,roundness,orientation" << std::endl;
                    i++;
                    continue;
                }
                std::stringstream s;
                s <<    descriptor.Area << "," << 
                        descriptor.perimeter  << "," <<
                        descriptor.r_factor << "," <<
                        descriptor.orientation << std::endl;

                myfile << s.str();

            }
            myfile.close();
        }
    }
}


