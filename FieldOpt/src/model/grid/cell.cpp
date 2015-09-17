#include "cell.h"

namespace Model {
namespace Grid {

Cell::Cell(int global_index, IJKCoordinate* ijk_index, double volume, XYZCoordinate* center, QList<XYZCoordinate *> corners)
{
    global_index_ = global_index;
    ijk_index_ = ijk_index;
    volume_ = volume;
    center_ = center;
    corners_ = corners;
}

}
}
