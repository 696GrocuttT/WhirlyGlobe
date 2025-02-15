/*
 *  ViewPlacementGenerator.h
 *  WhirlyGlobeLib
 *
 *  Created by Steve Gifford on 7/25/12.
 *  Copyright 2011-2019 mousebird consulting
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
 *
 */

#import <Foundation/Foundation.h>
#import <math.h>
#import "WhirlyVector.h"
#import "Scene.h"
#import "DataLayer.h"
#import "LayerThread.h"
#import "GlobeMath.h"
#import "SceneRenderer.h"

namespace WhirlyKit
{

#define kViewPlacementGeneratorShared "SharedViewPlacementGenerator"
  
/** The View Placement Generator moves UIViews around accordingly to locations in geographic coordinates.
    You'll need to both add the UIView here and add it underneath the OpenGL view.
 */
class ViewPlacementManager
{
public:
    ViewPlacementManager();
    virtual ~ViewPlacementManager();
    
    /// Used to track a UIView and location we want to put it at
    class ViewInstance
    {
    public:
        ViewInstance() { }
        ViewInstance(UIView *view) : view(view),offset2(0,0) { }
        ViewInstance(WhirlyKit::GeoCoord loc,UIView *view) : loc(loc), offset2(0,0), view(view), active(true) { offset.x() = view.frame.origin.x;  offset.y() = view.frame.origin.y; }
        ~ViewInstance() { }
        
        /// Sort by UIView
        bool operator < (const ViewInstance &that) const { return view < that.view; }

        /// Set if we're actively moving this around
        bool active;
        /// Where to put the view
        WhirlyKit::GeoCoord loc;
        /// An offset taken from the view origin when it's passed to us
        Point2d offset;
        /// An additional offset the user can pass in
        Point2d offset2;
        /// The view we're going to move around
        UIView *view;
        /// Minimum visibility above globe
        float minVis;
        /// Maximum visibility above globe
        float maxVis;
    };
    
    /// Add a view to be tracked.
    /// You should call this from the main thread.
    void addView(GeoCoord loc,const WhirlyKit::Point2d &offset,UIView *view,float minVis,float maxVis);

    /// Move an existing tracked view to a new location
    void moveView(GeoCoord loc,const WhirlyKit::Point2d &offset,UIView *view,float minVis,float maxVis);
    
    /// Stop moving a view around (but keep it)
    void freezeView(UIView *view);
    
    /// Start moving a view around again
    void unfreezeView(UIView *view);

    /// Remove a view being tracked.
    /// Call this in the main thread
    void removeView(UIView *view);

    /// Rather than generate drawables here, we update our locations
    void updateLocations(RendererFrameInfo *frameInfo);
    
    /// Print out stats for debugging
    void dumpStats();
    
    bool getChangedSinceUpdate() const { return changedSinceUpdate; }
            
protected:
    bool changedSinceUpdate;
    std::mutex viewInstanceLock;
    std::set<ViewInstance> viewInstanceSet;
};

}

namespace WhirlyKit
{
// Active Model runs right before the scene updates
class ViewPlacementActiveModel : public ActiveModel
{
public:
    ViewPlacementActiveModel() = default;
    
    ViewPlacementManager *getManager() { return &manager; }
    
    /// Create the stuff you need to manipulate in the scene
    virtual void startWithScene(Scene *scene) override;
    
    /// Return true if you have an update that needs to be processed.
    /// Return false if you don't, otherwise we'll be constantly rendering.
    virtual bool hasUpdate() const override;
    
    /// Update your stuff for display, but be quick!
    virtual void updateForFrame(RendererFrameInfo *frameInfo) override;
    
    /// Time to clean up your toys
    virtual void teardown(PlatformThreadInfo *) override;

protected:
    ViewPlacementManager manager;
};
    
typedef std::shared_ptr<ViewPlacementActiveModel> ViewPlacementActiveModelRef;
    
}
