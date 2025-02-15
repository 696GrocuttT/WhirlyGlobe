/*  GlobeView.cpp
 *  WhirlyGlobeLib
 *
 *  Created by Steve Gifford on 1/14/11.
 *  Copyright 2011-2021 mousebird consulting
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#import "Platform.h"
#import "WhirlyVector.h"
#import "GlobeView.h"
#import "WhirlyGeometry.h"
#import "GlobeMath.h"
#import "WhirlyKitLog.h"

using namespace WhirlyKit;
using namespace Eigen;

namespace WhirlyGlobe
{

GlobeView::GlobeView(WhirlyKit::CoordSystemDisplayAdapter *inCoordAdapter)
{
    coordAdapter = inCoordAdapter;
    //rotQuat = Eigen::AngleAxisd(-M_PI_4,Vector3d(1,0,0));     // North pole at heading=0
    rotQuat = Eigen::Quaterniond(-0.5, 0.5, 0.5, 0.5);  // lat=0 / lon=0 / heading=0
    coordAdapter = &fakeGeoC;
    defaultNearPlane = nearPlane;
    defaultFarPlane = farPlane;
    // This will get you down to r17 in the usual tile sets
    absoluteMinNearPlane = 0.000001;
    absoluteMinFarPlane = 0.001;
    absoluteMinHeight = 0.000002;
    heightInflection = 0.011;
    heightAboveGlobe = 1.1;
    tilt = 0.0;
    roll = 0.0;
}

GlobeView::GlobeView(const GlobeView &that)
: View(that), absoluteMinHeight(that.absoluteMinHeight), heightInflection(that.heightInflection), defaultNearPlane(that.defaultNearPlane),
    absoluteMinNearPlane(that.absoluteMinNearPlane), defaultFarPlane(that.defaultFarPlane), absoluteMinFarPlane(that.absoluteMinFarPlane),
    heightAboveGlobe(that.heightAboveGlobe), rotQuat(that.rotQuat), tilt(that.tilt), roll(that.roll)
{
}
    
GlobeView::~GlobeView()
{
}
    
void GlobeView::setFarClippingPlane(double newFarPlane)
{
    farPlane = newFarPlane;
}

void GlobeView::setRotQuat(Eigen::Quaterniond newRotQuat)
{
    setRotQuat(newRotQuat,true);
}

// Set the new rotation, but also keep track of when we did it
void GlobeView::setRotQuat(Eigen::Quaterniond newRotQuat,bool updateWatchers)
{
    double w, x, y, z;
    w = newRotQuat.coeffs().w();
    x = newRotQuat.coeffs().x();
    y = newRotQuat.coeffs().y();
    z = newRotQuat.coeffs().z();
    if (isnan(w) || isnan(x) || isnan(y) || isnan(z))
        return;

    lastChangedTime = TimeGetCurrent();
    rotQuat = newRotQuat;
    if (updateWatchers)
        runViewUpdates();
}

void GlobeView::setTilt(double newTilt)
{
    if (isnan(newTilt))
        return;

    tilt = newTilt;
}

void GlobeView::setRoll(double newRoll,bool updateWatchers)
{
    if (isnan(newRoll))
        return;
    
    roll = newRoll;

    lastChangedTime = TimeGetCurrent();
    roll = newRoll;
    if (updateWatchers)
        runViewUpdates();
}

double GlobeView::minHeightAboveGlobe() const
{
    if (continuousZoom)
        return absoluteMinHeight;
    else
        return 1.01*nearPlane;
}

double GlobeView::heightAboveSurface() const
{
    return heightAboveGlobe;
}
	
double GlobeView::maxHeightAboveGlobe() const
{
    return (farPlane - 1.0);
}
	
double GlobeView::calcEarthZOffset() const
{
	float minH = minHeightAboveGlobe();
	if (heightAboveGlobe < minH)
		return 1.0+minH;
	
	float maxH = maxHeightAboveGlobe();
	if (heightAboveGlobe > maxH)
		return 1.0+maxH;
	
	return 1.0 + heightAboveGlobe;
}

void GlobeView::setHeightAboveGlobe(double newH)
{
    privateSetHeightAboveGlobe(newH,true);
}

void GlobeView::setHeightAboveGlobe(double newH,bool updateWatchers)
{
    privateSetHeightAboveGlobe(newH,updateWatchers);
}

void GlobeView::setHeightAboveGlobeNoLimits(double newH,bool updateWatchers)
{
    if (isnan(newH))
        return;

    heightAboveGlobe = newH;
    
    // If we get down below the inflection point we'll start messing
    //  with the field of view.  Not ideal, but simple.
    if (continuousZoom)
    {
        if (heightAboveGlobe < heightInflection)
        {
            double t = 1.0 - (heightInflection - heightAboveGlobe) / (heightInflection - absoluteMinHeight);
            nearPlane = t * (defaultNearPlane-absoluteMinNearPlane) + absoluteMinNearPlane;
            farPlane = t * (defaultFarPlane-absoluteMinFarPlane) + absoluteMinFarPlane;
        } else {
            nearPlane = defaultNearPlane;
            farPlane = defaultFarPlane;
        }
		imagePlaneSize = nearPlane * tan(fieldOfView / 2.0);
    }
    
    lastChangedTime = TimeGetCurrent();
    
    if (updateWatchers)
        runViewUpdates();
}

// Set the height above the globe, but constrain it
// Also keep track of when we did it
void GlobeView::privateSetHeightAboveGlobe(double newH,bool updateWatchers)
{
    if (isnan(newH))
        return;

    double minH = minHeightAboveGlobe();
	heightAboveGlobe = std::max(newH,minH);
    
	double maxH = maxHeightAboveGlobe();
	heightAboveGlobe = std::min(heightAboveGlobe,maxH);

    // If we get down below the inflection point we'll start messing
    //  with the field of view.  Not ideal, but simple.
    if (continuousZoom)
    {
        if (heightAboveGlobe < heightInflection)
        {
            double t = 1.0 - (heightInflection - heightAboveGlobe) / (heightInflection - absoluteMinHeight);
            nearPlane = t * (defaultNearPlane-absoluteMinNearPlane) + absoluteMinNearPlane;
//            farPlane = t * (defaultFarPlane-absoluteMinFarPlane) + absoluteMinFarPlane;
        } else {
            nearPlane = defaultNearPlane;
//            farPlane = defaultFarPlane;
        }
		imagePlaneSize = nearPlane * tan(fieldOfView / 2.0);
    }
    
    lastChangedTime = TimeGetCurrent();
    
    if (updateWatchers)
       runViewUpdates();
}

void GlobeView::setCenterOffset(double offX,double offY,bool updateWatchers)
{
    centerOffset = Point2d(offX,offY);

    if (updateWatchers)
        runViewUpdates();
}
	
Eigen::Matrix4d GlobeView::calcModelMatrix() const
{
    // Move the model around to move the center around
    Point2d modelOff(0.0,0.0);
    if (centerOffset.x() != 0.0 || centerOffset.y() != 0.0) {
        // imagePlaneSize is actually half the image plane size in the horizontal
        modelOff = Point2d(centerOffset.x() * imagePlaneSize, centerOffset.y() * imagePlaneSize) * (heightAboveGlobe+1.0)/nearPlane;
    }
    
    Eigen::Affine3d trans(Eigen::Translation3d(modelOff.x(),modelOff.y(),-calcEarthZOffset()));
	Eigen::Affine3d rot(rotQuat);
	
	return (trans * rot).matrix();
}

Eigen::Matrix4d GlobeView::calcViewMatrix() const
{
    Eigen::Quaterniond selfRotPitch(AngleAxisd(tilt, Vector3d::UnitX()));
    Eigen::Quaterniond selfRoll(AngleAxisd(roll, Vector3d::UnitZ()));
    Eigen::Quaterniond combo = selfRoll * selfRotPitch;
    
    return ((Affine3d)combo).matrix();
}

Vector3d GlobeView::currentUp() const
{
	Eigen::Matrix4d modelMat = calcModelMatrix().inverse();
	
	Vector4d newUp = modelMat * Vector4d(0,0,1,0);
	return Vector3d(newUp.x(),newUp.y(),newUp.z());
}

Vector3d GlobeView::prospectiveUp(Eigen::Quaterniond &prospectiveRot)
{
    Eigen::Affine3d rot(prospectiveRot);
    Eigen::Matrix4d modelMat = rot.inverse().matrix();
    Vector4d newUp = modelMat *Vector4d(0,0,1,0);
    return Vector3d(newUp.x(),newUp.y(),newUp.z());
}
    
bool GlobeView::pointOnSphereFromScreen(const Point2f &pt,const Eigen::Matrix4d &modelTrans,const Point2f &frameSize,Point3d &hit,bool normalized,double radius)
{
    // Back project the point from screen space into model space
    const Point3d screenPt = pointUnproject(Point2f(pt.x(),pt.y()),frameSize.x(),frameSize.y(),true);
    
    // Run the screen point and the eye point (origin) back through
    //  the model matrix to get a direction and origin in model space
    const Matrix4d invModelMat = modelTrans.inverse();
    const Point3d eyePt(0,0,0);
    const Vector4d modelEye = invModelMat * Vector4d(eyePt.x(),eyePt.y(),eyePt.z(),1.0);
    const Vector4d modelScreenPt = invModelMat * Vector4d(screenPt.x(),screenPt.y(),screenPt.z(),1.0);
    
    // Now intersect that with a unit sphere to see where we hit
    const Vector4d dir4 = modelScreenPt - modelEye;
    Vector3d dir(dir4.x(),dir4.y(),dir4.z());
    double t;
    if (IntersectSphereRadius(Vector3d(modelEye.x(),modelEye.y(),modelEye.z()), dir, radius, hit, &t) && t > 0.0)
        return true;
    
    // We need the closest pass, if that didn't work out
    if (normalized)
    {
        Vector3d orgDir(-modelEye.x(),-modelEye.y(),-modelEye.z());
        orgDir.normalize();
        dir.normalize();
        const Vector3d tmpDir = orgDir.cross(dir);
        const Vector3d resVec = dir.cross(tmpDir);
        hit = -resVec.normalized();
    } else {
        const double len2 = dir.squaredNorm();
        const double top = dir.dot(Vector3d(modelScreenPt.x(),modelScreenPt.y(),modelScreenPt.z()));
        t = (len2 > 0.0) ? top/len2 : 0.0;
        hit = Vector3d(modelEye.x(),modelEye.y(),modelEye.z()) + dir*t;
    }

    return false;
}
	
bool GlobeView::pointOnSphereFromScreen(const Point2f &pt,const Eigen::Matrix4d &modelTrans,const Point2f &frameSize,Point3d &hit,bool normalized)
{
	// Back project the point from screen space into model space
    const Point3d screenPt = pointUnproject(Point2f(pt.x(),pt.y()),frameSize.x(),frameSize.y(),true);
	
	// Run the screen point and the eye point (origin) back through
	//  the model matrix to get a direction and origin in model space
    const Matrix4d invModelMat = modelTrans.inverse();
    const Point3d eyePt(0,0,0);
    const Vector4d modelEye = invModelMat * Vector4d(eyePt.x(),eyePt.y(),eyePt.z(),1.0);
    const Vector4d modelScreenPt = invModelMat * Vector4d(screenPt.x(),screenPt.y(),screenPt.z(),1.0);
	
	// Now intersect that with a unit sphere to see where we hit
    const Vector4d dir4 = modelScreenPt - modelEye;
	Vector3d dir(dir4.x(),dir4.y(),dir4.z());
    double t;
	if (IntersectUnitSphere(Vector3d(modelEye.x(),modelEye.y(),modelEye.z()), dir, hit, &t) && t > 0.0)
		return true;
	
	// We need the closest pass, if that didn't work out
    if (normalized)
    {
        Vector3d orgDir(-modelEye.x(),-modelEye.y(),-modelEye.z());
        orgDir.normalize();
        dir.normalize();
        const Vector3d tmpDir = orgDir.cross(dir);
        const Vector3d resVec = dir.cross(tmpDir);
        hit = -resVec.normalized();
    } else {
        const double len2 = dir.squaredNorm();
        const double top = dir.dot(Vector3d(modelScreenPt.x(),modelScreenPt.y(),modelScreenPt.z()));
        t = (len2 > 0.0) ? top/len2 : 0.0;
        hit = Vector3d(modelEye.x(),modelEye.y(),modelEye.z()) + dir*t;
    }
	
	return false;
}

Point2f GlobeView::pointOnScreenFromSphere(const Point3d &worldLoc,const Eigen::Matrix4d *transform,const Point2f &frameSize)
{
    // Run the model point through the model transform (presumably what they passed in)
    Vector4d screenPt = *transform * Vector4d(worldLoc.x(),worldLoc.y(),worldLoc.z(),1.0);
    if (screenPt.w() != 0)
    {
        screenPt /= screenPt.w();
    }

    // Intersection with near gives us the same plane as the screen 
    const Vector4d ray = screenPt * -nearPlane/screenPt.z();

    // Now we need to scale that to the frame
    Point2d ll,ur;
    double near,far;
    calcFrustumWidth(frameSize.x(),frameSize.y(),ll,ur,near,far);
    const double u = (ray.x() - ll.x()) / (ur.x() - ll.x());
    const double v = (ray.y() - ll.y()) / (ur.y() - ll.y());

    return { u * frameSize.x(), (1.0 - v) * frameSize.y() };
}

// Calculate the Z buffer resolution
float GlobeView::calcZbufferRes()
{
    float delta;
//    int numBits = 16;
//    float testZ = 1.5;  // Should only need up to 1.0 away, actually
//    delta = testZ * testZ / ( nearPlane * (1<<numBits - 1));
    
    // Note: Not right
    delta = 0.0001;
    
    return delta;
}

// Construct a rotation to the given location
Eigen::Quaterniond GlobeView::makeRotationToGeoCoord(const WhirlyKit::Point2d &worldCoord,bool northUp)
{
    const Point3d worldLoc = coordAdapter->localToDisplay(coordAdapter->getCoordSystem()->geographicToLocal(worldCoord));
    
    // Let's rotate to where they tapped over a 1sec period
    const Vector3d curUp = currentUp();
    
    // The rotation from where we are to where we tapped
    const Eigen::Quaterniond endRot = QuatFromTwoVectors(worldLoc,curUp);
    const Eigen::Quaterniond curRotQuat = rotQuat;
    const Eigen::Quaterniond newRotQuat = curRotQuat * endRot;
    
    if (northUp)
    {
        // We'd like to keep the north pole pointed up
        // So we look at where the north pole is going
        const Vector3d northPole = (newRotQuat * Vector3d(0,0,1)).normalized();
        if (northPole.y() != 0.0)
        {
            // Then rotate it back on to the YZ axis, this will keep it upward.
            // However, the pole might be down now.  If so, rotate it back up.
            // todo: would `atan2` be simpler?
            const float ang = atan(northPole.x()/northPole.y()) + ((northPole.y() < 0.0) ? M_PI : 0.0);
            return newRotQuat * Eigen::AngleAxisd(ang,worldLoc);
        }
    }
    
    return newRotQuat;
}
    
Eigen::Quaterniond GlobeView::makeRotationToGeoCoord(const WhirlyKit::GeoCoord &worldCoord,bool northUp)
{
    Point2d coord(worldCoord.x(),worldCoord.y());
    return makeRotationToGeoCoord(coord, northUp);
}

Eigen::Vector3d GlobeView::eyePos() const
{
	const Eigen::Matrix4d modelMat = calcModelMatrix().inverse();
	const Vector4d newUp = modelMat * Vector4d(0,0,1,1);
	return Vector3d(newUp.x(),newUp.y(),newUp.z());
}

/// Set the change delegate
void GlobeView::setDelegate(GlobeViewAnimationDelegateRef inDelegate)
{
    delegate = inDelegate;
}

/// Called to cancel a running animation
void GlobeView::cancelAnimation()
{
    delegate.reset();
}

/// Renderer calls this every update.
void GlobeView::animate()
{
    // Have to hold on to the delegate because it can call cancelAnimation.... which frees the delegate
    auto theDelegate = delegate;
    if (theDelegate)
        theDelegate->updateView(this);
}
    
ViewStateRef GlobeView::makeViewState(SceneRenderer *renderer)
{
    return std::make_shared<GlobeViewState>(this,renderer);
}

GlobeViewState::GlobeViewState(WhirlyGlobe::GlobeView *globeView,WhirlyKit::SceneRenderer *renderer)
: ViewState(globeView,renderer)
{
    heightAboveGlobe = globeView->heightAboveSurface();
    rotQuat = globeView->getRotQuat();
}

GlobeViewState::~GlobeViewState()
{
}

Eigen::Vector3d GlobeViewState::currentUp()
{
    Eigen::Matrix4d modelMat = invModelMatrix;
    
    Vector4d newUp = modelMat * Vector4d(0,0,1,0);
    return Vector3d(newUp.x(),newUp.y(),newUp.z());
}


bool GlobeViewState::pointOnSphereFromScreen(const Point2f &pt,const Eigen::Matrix4d &modelTrans,const Point2f &frameSize,Point3d &hit,bool clip)
{
    // Back project the point from screen space into model space
    Point3d screenPt = pointUnproject(Point2d(pt.x(),pt.y()),frameSize.x(),frameSize.y(),clip);
    
    // Run the screen point and the eye point (origin) back through
    //  the model matrix to get a direction and origin in model space
    Matrix4d invModelMat = modelTrans.inverse();
    Point3d eyePt(0,0,0);
    Vector4d modelEye = invModelMat * Vector4d(eyePt.x(),eyePt.y(),eyePt.z(),1.0);
    Vector4d modelScreenPt = invModelMat * Vector4d(screenPt.x(),screenPt.y(),screenPt.z(),1.0);
    
    // Now intersect that with a unit sphere to see where we hit
    Vector4d dir4 = modelScreenPt - modelEye;
    Vector3d dir(dir4.x(),dir4.y(),dir4.z());
    if (IntersectUnitSphere(Vector3d(modelEye.x(),modelEye.y(),modelEye.z()), dir, hit))
        return true;
    
    // We need the closest pass, if that didn't work out
    Vector3d orgDir(-modelEye.x(),-modelEye.y(),-modelEye.z());
    orgDir.normalize();
    dir.normalize();
    Vector3d tmpDir = orgDir.cross(dir);
    Vector3d resVec = dir.cross(tmpDir);
    hit = -resVec.normalized();
    
    return false;
}
    
}
