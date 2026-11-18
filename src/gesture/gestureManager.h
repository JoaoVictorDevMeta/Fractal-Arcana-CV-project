#include "vision/air_pen.h"
#include "gesture/shape_classif.h"

class gestureManager
{
private:
    AirPen pen;
    ShapeClassifier classifier;
public:
    gestureManager(/* args */);
    ~gestureManager();
};