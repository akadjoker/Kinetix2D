#include "kx/joints/joint.h"

namespace kx
{

    Joint::Joint(JointType type, Body *a, Body *b)
        : mType(type), mBodyA(a), mBodyB(b), mCollideConnected(false)
    {
    }

    Joint::~Joint()
    {
    }

} // namespace kx
