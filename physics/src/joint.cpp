#include "kx/joint.h"

namespace kx
{

    Joint::Joint(JointType type, Body *a, Body *b)
        : mType(type), mBodyA(a), mBodyB(b)
    {
    }

    Joint::~Joint()
    {
    }

} // namespace kx
