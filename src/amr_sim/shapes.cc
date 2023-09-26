#include "amr_sim/shapes.h"

/* ------------- Triangle ------------- */

Triangle::Triangle(Params params):params_(params)
{}

Triangle::Triangle()
{
    // default triangle vertices
    params_.p1 = olc::vf2d{0,0};
    params_.p2 = olc::vf2d{0,0};
    params_.p3 = olc::vf2d{0,0};
}

void Triangle::GetParams(Params &params)
{
    params = params_;
}

void Triangle::SetParams(const Params &params)
{
    params_ = params;
}

std::string Triangle::GetShape()
{
    return "triangle";
}


bool Triangle::isInside(const olc::vf2d &p)
{
    olc::vf2d p1 = params_.p1;
    olc::vf2d p2 = params_.p2;
    olc::vf2d p3 = params_.p3;

    // Calculate the vectors
    olc::vf2d v0 = p2 - p1;
    olc::vf2d v1 = p3 - p1;
    olc::vf2d v2 = p  - p1;

    // Calculate dot products
    float dot00 = v0.dot(v0);
    float dot01 = v0.dot(v1);
    float dot02 = v0.dot(v2);
    float dot11 = v1.dot(v1);
    float dot12 = v1.dot(v2);

    // Calculate barycentric coordinates
    float invDenom = 1 / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    float w = 1 - u - v;

    return (u >= 0 && u <= 1 && v >= 0 && v <= 1 && w >= 0 && w <= 1);
}

/* ------------- Rectangle ------------- */

Rectangle::Rectangle(Params params):params_(params)
{}

Rectangle::Rectangle()
{
    // default rectangle properties
    params_.pCenter = olc::vf2d{0.,0.};
    params_.W = 10.;
    params_.H = 10.;
    params_.theta = 0.;
}

void Rectangle::UpdateVertices()
{
    float W = params_.W;
    float H = params_.H;
    float theta = params_.theta;
    olc::vf2d pCenter = params_.pCenter;

    // find diagonal length
    float D = std::sqrt(std::pow(W,2) + std::pow(H,2));
    
    // find angles of 4 vertices
    float theta2 = std::asin(H/D);
    float theta1 = theta2 + 2*std::asin(W/D);
    float theta3 = -theta2;
    float theta4 = -theta1;

    // add theta offset
    theta1 += theta;
    theta2 += theta;
    theta3 += theta;
    theta4 += theta;

    // update the 4 vertices
    olc::vf2d v = (D/2.)*olc::vf2d(1,0);
    olc::vf2d v1,v2,v3,v4;

    Rotate(v,theta1,v1);
    Rotate(v,theta2,v2);
    Rotate(v,theta3,v3);
    Rotate(v,theta4,v4);

    olc::vf2d p1,p2,p3,p4;
    p1 = pCenter + v1;
    p2 = pCenter + v2;
    p3 = pCenter + v3;
    p4 = pCenter + v4;

    // update internal triangles
    Triangle::Params triangleParams;

    triangleParams.p1 = p1;
    triangleParams.p2 = p2;
    triangleParams.p3 = p3;
    upperTriangle_.SetParams(triangleParams);

    triangleParams.p1 = p1;
    triangleParams.p2 = p3;
    triangleParams.p3 = p4;
    lowerTriangle_.SetParams(triangleParams);
}

void Rectangle::GetParams(Params &params)
{
    params = params_;
}

void Rectangle::SetParams(const Params &params)
{
    params_ = params;
    UpdateVertices();
}

void Rectangle::GetInternalTriangles(Triangle &upperTriangle, Triangle &lowerTriangle)
{
    upperTriangle = upperTriangle_;
    lowerTriangle = lowerTriangle_;
}

bool Rectangle::isInside(const olc::vf2d &p)
{
    return (upperTriangle_.isInside(p) || lowerTriangle_.isInside(p));
}

std::string Rectangle::GetShape()
{
    return "rectangle";
}

/* ------------ Spline ----------- */
Spline::Spline()
{
    isLoop_ = false;
}

Spline::Spline(bool isLoop):isLoop_(isLoop)
{}

void Spline::SetLoop(const bool &isLoop)
{
    isLoop_ = isLoop;
}

bool Spline::IsLoop()
{
    return isLoop_;
}

int Spline::GetMaxT()
{
    int tMax;
    if (isLoop_)
    {
        tMax = controlPoints.size();
    }
    else
    {
        tMax = ((controlPoints.size()-3) < 0)? 0 : (controlPoints.size()-3);
    }

    return tMax;
}

void Spline::Select4Points(float &t, olc::vf2d &p0, olc::vf2d &p1, olc::vf2d &p2, olc::vf2d &p3)
{
    // get 4 control points based on mode
    int index = static_cast<int>(t);
    if (isLoop_)
    {
        p0 = controlPoints[(((index-1) < 0) ? (controlPoints.size()-1) : (index-1))];
        p1 = controlPoints[index];
        p2 = controlPoints[(index+1)%controlPoints.size()];
        p3 = controlPoints[(index+2)%controlPoints.size()];
    }
    else
    {
        p0 = controlPoints[index];
        p1 = controlPoints[index+1];
        p2 = controlPoints[index+2];
        p3 = controlPoints[index+3];
    }

    // offset t
    t -= static_cast<float>(index);
}

bool Spline::IsValid(const float &t)
{
    if (t < 0.)
    {
        std::cerr << "Error: Spline parameter t must be non-negative\n";
        return false;
    }
    if (controlPoints.size() < 4)
    {
        std::cerr << "Error: Catmull-Rom spline needs at least 4 control points\n";
        return false;
    }
    if (t > GetMaxT())
    {
        std::cerr << "Error: t is out-of-range\n";
        return false;
    }

    return true;
}

bool Spline::Interpolate(float t, olc::vf2d &p)
{
    // Check if t is valid and if spline is properly set up
    if (!IsValid(t))
    {
        return false;
    }

    // get 4 control points of Catmull-Rom spline
    olc::vf2d p0,p1,p2,p3;
    Select4Points(t,p0,p1,p2,p3);

    // interpolate point
    p = 0.5*((2*p1) +
             (-p0+p2)*t + 
             (2*p0-5*p1+4*p2-p3)*std::pow(t,2) + 
             (-p0+3*p1-3*p2+p3)*std::pow(t,3));

    return true;
}

bool Spline::GetGradient(float t , olc::vf2d &p)
{
    // Check if t is valid and if spline is properly set up
    if (!IsValid(t))
    {
        return false;
    }

    // get 4 control points of Catmull-Rom spline
    olc::vf2d p0,p1,p2,p3;
    Select4Points(t,p0,p1,p2,p3);

    // interpolate point
    p = 0.5*((-p0+p2) + 
             2.*(2*p0-5*p1+4*p2-p3)*t + 
             3.*(-p0+3*p1-3*p2+p3)*std::pow(t,2));

    // convert p into unit normal vector
    p = p.norm();
    p = p.perp();

    return true;   
}

/* ------------ Road ------------- */


/* --------- Utilities --------- */
void Rotate(const olc::vf2d &pIn, const float &theta, olc::vf2d &pOut)
{
    pOut.x = std::cos(theta)*pIn.x - std::sin(theta)*pIn.y;
    pOut.y = std::sin(theta)*pIn.x + std::cos(theta)*pIn.y;
}