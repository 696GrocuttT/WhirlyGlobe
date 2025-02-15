/*  MapGestureHandler.java
 *  WhirlyGlobeLib
 *
 *  Created by Steve Gifford on 6/2/14.
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

package com.mousebird.maply;

import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;

/**
 * Implements the various gestures we need and handles conflict between them.
 * <p>
 * This is used by the MaplyController to deal with gestures on Android.  If
 * you want to mess with this, be sure to subclass the MaplyController and
 * create your own subclass of this. 
 *
 */
public class MapGestureHandler
{
	MapController mapControl = null;
	MapView mapView = null;

	public boolean allowRotate = false;
	public boolean allowZoom = true;
	public boolean allowPan = true;
	
	ScaleGestureDetector sgd = null;
	ScaleListener sl = null;
	GestureDetector gd = null;
	GestureListener gl = null;
	View view = null;
	double zoomLimitMin = 0.0;
	double zoomLimitMax = 1000.0;
	double startRot = Double.MAX_VALUE;
	double startViewRot = Double.MAX_VALUE;

	long lastZoomEnd = 0;

	public MapGestureHandler(MapController inControl,View inView)
	{
		mapControl = inControl;
		mapView = mapControl.mapView;
		view = inView;
		sl = new ScaleListener(mapControl);
		sgd = new ScaleGestureDetector(view.getContext(),sl);
		gl = new GestureListener(mapControl);
		gd = new GestureDetector(view.getContext(),gl);
		sl.gl = gl;		
	}

	public void shutdown() {
		this.sgd = null;
		if(this.sl != null) {
			this.sl.maplyControl = null;
		}

		this.sl = null;
		this.gd = null;
		if(this.gl != null) {
			this.gl.maplyControl = null;
		}

		this.gl = null;
		this.view = null;
	}

	public void setZoomLimits(double inMin,double inMax)
	{
		zoomLimitMin = inMin;
		zoomLimitMax = inMax;
	}

	/**
	 * Check that a given position will be within the given bounds.
	 * This is used by the various gestures for bounds checking.
	 * 
	 * @param newLocalPos Position we're to check.
	 * @param bounds Bounding box, probably from the maplyControl.
	 * @return true if the new point is within the valid area.
	 */
	public static boolean withinBounds(MapView mapView,Point2d frameSize,Point3d newLocalPos,Point2d[] bounds)
	{
		if (bounds == null)
			return true;

		// We make a copy of the map view so we can mess with it
		MapView thisMapView = mapView.clone();

	    // The corners of the view should be within the bounds
	    Point2d[] corners = new Point2d[4];
	    corners[0] = new Point2d(0,0);
	    corners[1] = new Point2d(frameSize.getX(), 0.0);
	    corners[2] = new Point2d(frameSize.getX(), frameSize.getY());
	    corners[3] = new Point2d(0.0, frameSize.getY());

		Point2d locOffset = new Point2d(0,0);
		for (int tests=0;tests<4;tests++) {
			Point3d testLoc = new Point3d(newLocalPos.getX()+locOffset.getX(),newLocalPos.getY()+locOffset.getY(),newLocalPos.getZ());
			thisMapView.setLoc(testLoc);
			Matrix4d fullMatrix = thisMapView.calcModelViewMatrix();

			boolean checkOkay = true;
			for (int ii = 0; ii < 4; ii++) {
				Point3d hit = thisMapView.pointOnPlaneFromScreen(corners[ii], fullMatrix, frameSize, false);
				Point2d hit2d = new Point2d(hit.getX(), hit.getY());
				if (!GeometryUtils.PointInPolygon(hit2d, bounds)) {
					Point2d closePt = new Point2d();
					GeometryUtils.ClosestPointToPolygon(bounds,hit2d,closePt);
					Point2d thisOffset = new Point2d(1.01*(closePt.getX()-hit2d.getX()),1.01*(closePt.getY()-hit2d.getY()));

					locOffset = locOffset.addTo(thisOffset);
					checkOkay = false;

					break;
				}
			}

			if (checkOkay)
			{
				newLocalPos.setValue(testLoc.getX(),testLoc.getY(),testLoc.getZ());
				return true;
			}
		}
	    
	    return false;
	}
	
	// Listening for a pinch scale event
	private class ScaleListener extends ScaleGestureDetector.SimpleOnScaleGestureListener
	{
		MapController maplyControl;
		double startZ;
		float startDist;
		GestureListener gl = null;
		public boolean isActive = false;
//		Point3d centerGeoCoord = null;
		
		ScaleListener(MapController inMaplyControl)
		{
			maplyControl = inMaplyControl;
		}
		
		@Override
		public boolean onScaleBegin(ScaleGestureDetector detector)
		{
			if (!allowZoom)
				return false;

			startZ = maplyControl.mapView.getLoc().getZ();
			startDist = detector.getCurrentSpan();
//			Log.d("Maply","Starting zoom");

			// Find the center and zoom around that
//			Point2d center = new Point2d(detector.getFocusX(),detector.getFocusY());
//			Matrix4d modelTransform = maplyControl.mapView.calcModelViewMatrix();
//			Point3d hit = maplyControl.mapView.pointOnPlaneFromScreen(center, modelTransform, maplyControl.renderWrapper.maplyRender.frameSize, false);
//			Point3d localPt = mapView.coordAdapter.displayToLocal(hit);
//			centerGeoCoord = mapView.coordAdapter.getCoordSystem().localToGeographic(localPt);

			// Cancel the panning
			if (gl != null)
				gl.isActive = false;
			isActive = true;
			return true;
		}
		
		@Override
		public boolean onScale(ScaleGestureDetector detector)
		{
			if (!allowZoom)
				return false;

			float curDist = detector.getCurrentSpan();
			if (curDist > 0.0 && startDist > 0.0)
			{				
				float scale = startDist/curDist;
				Point3d pos = maplyControl.mapView.getLoc();
				mapView.cancelAnimation();
				Point3d newPos = new Point3d(pos.getX(),pos.getY(),startZ*scale);
				if (withinBounds(mapView,maplyControl.getViewSize(),newPos,maplyControl.viewBounds) || scale < 1.0) {
					double newZ = newPos.getZ();
					newZ = Math.min(newZ,zoomLimitMax);
					newZ = Math.max(newZ,zoomLimitMin);
					maplyControl.mapView.setLoc(new Point3d(newPos.getX(),newPos.getY(),newZ));
				}
//				Log.d("Maply","Zoom: " + maplyControl.mapView.getLoc().getZ() + " Scale: " + scale);
				return true;
			}
			
			isActive = false;
			return false;
		}
		
		@Override
		public void onScaleEnd(ScaleGestureDetector detector)
		{
			if (!allowZoom)
				return;

//			Log.d("Maply","Ending scale");
			lastZoomEnd = System.currentTimeMillis();
			isActive = false;
		}
	}
	
	// Listening for the rest of the interesting events
	private class GestureListener implements GestureDetector.OnGestureListener,
				GestureDetector.OnDoubleTapListener
	{
		MapController maplyControl;
		MapView mapView;
		public boolean isActive = false;
		
		GestureListener(MapController inMaplyControl) {
			maplyControl = inMaplyControl;
			mapView = maplyControl.mapView;
		}
		
		Point2d startScreenPos = null;
		Point3d startLoc = null;
		Point3d startOnPlane = null;
		Matrix4d startTransform = null;
		@Override
		public boolean onDown(MotionEvent e) 
		{
//			Log.d("Maply","onDown");

			// Starting state for pan
			startScreenPos = new Point2d(e.getX(),e.getY());
			startTransform = mapView.calcModelViewMatrix();
			final Point2d viewSize = maplyControl.getViewSize();
			final Point2d viewCenter = viewSize.multiplyBy(0.5);
			startLoc = mapView.pointOnPlaneFromScreen(viewCenter, startTransform, viewSize, false);
			startOnPlane = mapView.pointOnPlaneFromScreen(startScreenPos, startTransform, viewSize, false);
			isActive = true;
			return true;
		}

		@Override
		public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX,
				float distanceY) 
		{
			if (!isActive)
				return false;

			if (!allowPan)
				return false;
			
			Point2d newScreenPos = new Point2d(e2.getX(),e2.getY());
			
			// New state for pan
			Point3d hit = mapView.pointOnPlaneFromScreen(newScreenPos, startTransform, maplyControl.getViewSize(), false);
			if (hit != null)
			{
				Point3d newPos = new Point3d(startOnPlane.getX()-hit.getX()+startLoc.getX(),
						startOnPlane.getY()-hit.getY()+startLoc.getY(),
						maplyControl.mapView.getLoc().getZ());
				mapView.cancelAnimation();
								
				// If the point is within bounds, set it
				Point3d locPos = mapView.coordAdapter.displayToLocal(newPos);
				if (withinBounds(mapView, maplyControl.getViewSize(), locPos, maplyControl.viewBounds)) {
					mapView.setLoc(locPos);
				}
			}
			
			return true;
		}
		
		// How long we'll animate the momentum 
		static final double AnimMomentumTime = 1.0;
		
		@Override
		public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX,
				float velocityY) 
		{
//			Log.d("Maply","Fling: (x,y) = " + velocityX + " " + velocityY);

			if (!allowPan)
				return false;

			if (System.currentTimeMillis() - lastZoomEnd < 175)
			{
				isActive = false;
				return true;
			}

			// Project the points into map space
			final Matrix4d mapTransform = maplyControl.mapView.calcModelViewMatrix();
			final Point2d touch0 = new Point2d(e1.getX(),e1.getY());
			final Point2d touch1 = touch0.addTo(new Point2d(velocityX,velocityY));
			final Point2d viewSize = maplyControl.getViewSize();
			final Point3d dpt0 = mapView.pointOnPlaneFromScreen(touch0, mapTransform, viewSize, false);
			final Point3d dpt1 = mapView.pointOnPlaneFromScreen(touch1, mapTransform, viewSize, false);
			final Point3d pt0 = mapView.coordAdapter.displayToLocal(dpt0);
			final Point3d pt1 = mapView.coordAdapter.displayToLocal(dpt1);

			// That gives us a direction in map space
			Point3d dir = pt0.subtract(pt1);
			dir.multiplyBy(-1.0);
			final double len = dir.length();
			dir = dir.multiplyBy((len == 0.0) ? 0.0 : 1.0/len);
			final double modelVel = len / AnimMomentumTime;
			
			// Acceleration based on how far we want this to go
			final double accel = - modelVel / (AnimMomentumTime * AnimMomentumTime);
			
			// Now kick off the animation
			final RendererWrapper renderWrap = maplyControl.renderWrapper;
			if (renderWrap !=  null) {
				final RenderController render = renderWrap.maplyRender.get();
				if (render != null) {
					mapView.setAnimationDelegate(new MapAnimateTranslateMomentum(mapView, render, modelVel, accel, dir, maplyControl.viewBounds));
				}
			}
		
			isActive = false;
			
			return true;
		}
		
		@Override
		public void onLongPress(MotionEvent e) 
		{
//			Log.d("Maply","Long Press");
			if (sl == null || gl == null)
				return;
			if (!sl.isActive && gl.isActive && e.getPointerCount() == 1)
				maplyControl.processLongPress(new Point2d(e.getX(),e.getY()));
		}


		@Override
		public void onShowPress(MotionEvent e) 
		{
//			Log.d("Maply","ShowPress");
		}

		@Override
		public boolean onSingleTapUp(MotionEvent e) 
		{
//			Log.d("Maply","Single Tap Up");
			return false;
		}

		@Override
		public boolean onDoubleTapEvent(MotionEvent e) 
		{
//			Log.d("Maply","Double tap update");
			return false;
		}

		@Override
		public boolean onSingleTapConfirmed(MotionEvent e) 
		{
			if (System.currentTimeMillis() - lastZoomEnd < 175) {
				return true;
			}

			final MapController theControl = maplyControl;
			if (theControl != null) {
				maplyControl.processTap(new Point2d(e.getX(), e.getY()));
			}

			return true;
		}

		// Zoom in on double tap
		@Override
		public boolean onDoubleTap(MotionEvent e) 
		{
			if (!allowZoom || maplyControl == null)
				return false;

			// Figure out where they tapped
			Point2d touch = new Point2d(e.getX(),e.getY());
			Matrix4d mapTransform = maplyControl.mapView.calcModelViewMatrix();
			Point3d pt = mapView.pointOnPlaneFromScreen(touch, mapTransform, maplyControl.getViewSize(), false);
			if (pt == null)
				return false;
			Point3d locPt = mapView.getCoordAdapter().displayToLocal(pt);
			if (locPt == null)
				return false;

			// Zoom in where they tapped
			Point3d loc = mapView.getLoc();
			double newZ = loc.getZ()/2.0;
			newZ = Math.min(newZ,zoomLimitMax);
			newZ = Math.max(newZ,zoomLimitMin);
			loc.setValue(locPt.getX(), locPt.getY(), newZ);
			
			// Now kick off the animation
			final RendererWrapper renderWrap = maplyControl.renderWrapper;
			if (renderWrap !=  null) {
				final RenderController render = renderWrap.maplyRender.get();
				if (render != null) {
					// Start after the stop/end events for the current tap
					maplyControl.addPostSurfaceRunnable(() -> {
						mapView.setAnimationDelegate(
								new MapAnimateTranslate(mapView, render, loc, (float) 0.1, maplyControl.viewBounds));
					});
				}
			}

			isActive = false;
			
			return true;
		}		
	}

	// Update rotation when there are two fingers working
	void handleRotation(MotionEvent event)
	{
		if (allowRotate && event.getPointerCount() > 1)
		{
			MotionEvent.PointerCoords p0 = new MotionEvent.PointerCoords();
			MotionEvent.PointerCoords p1 = new MotionEvent.PointerCoords();
			event.getPointerCoords(0,p0);
			event.getPointerCoords(1,p1);
			double cX = (p0.x+p1.x)/2.0;
			double cY = (p0.y+p1.y)/2.0;
			double dx = p0.x-cX;
			double dy = p0.y-cY;

			// Calculate a starting rotation
			if (startRot == Double.MAX_VALUE)
			{
				startRot = Math.atan2(dy, dx);
				startViewRot = mapView.getRot();
			} else {
				// Update an existing rotation
				double curRot = Math.atan2(dy, dx);
				double diffRot = curRot-startRot;
				mapView.setRot(startViewRot+diffRot);
			}
		}
	}

	// Cancel an outstanding rotation
	void cancelRotation()
	{
		startRot = Double.MAX_VALUE;
	}
	
	// Where we receive events from the gl view
	public boolean onTouch(View v, MotionEvent event) 
	{
		boolean slWasActive = this.sl.isActive;
		boolean glWasActive = this.gl.isActive;
		boolean rotWasActive = startRot != Double.MAX_VALUE;

		// If they're using two fingers, cancel any outstanding pan
		if (event.getPointerCount() == 2)
			gl.isActive = false;
		
		// Try for a pinch or another gesture
		if (sl.isActive || event.getPointerCount() == 2)
		{
			sgd.onTouchEvent(event);
			handleRotation(event);
		}
		if (!sl.isActive && event.getPointerCount() == 1) {
			gd.onTouchEvent(event);
			if (event.getAction() == MotionEvent.ACTION_UP) {
				gl.isActive = false;
			}
			cancelRotation();
		}
		
		if (!sl.isActive && !gl.isActive && !slWasActive && allowZoom)
		{
			if (System.currentTimeMillis() - lastZoomEnd >= 175) {
				if (event.getPointerCount() == 2 && (event.getActionMasked() == MotionEvent.ACTION_POINTER_UP)) {
					Point3d loc = mapView.getLoc();
					double newZ = loc.getZ() * 2.0;
					newZ = Math.min(newZ, zoomLimitMax);
					newZ = Math.max(newZ, zoomLimitMin);
					loc.setValue(loc.getX(), loc.getY(), newZ);

					// Now kick off the animation
					final RendererWrapper renderWrap = mapControl.renderWrapper;
					if (renderWrap !=  null) {
						final RenderController render = renderWrap.maplyRender.get();
						if (render != null) {
							mapView.setAnimationDelegate(new MapAnimateTranslate(mapView, render, loc, (float) 0.1, mapControl.viewBounds));
						}
					}

					sl.isActive = false;
					gl.isActive = false;
				}
			}
		}

		if(!glWasActive && this.gl.isActive) {
			this.mapControl.panDidStart(true);
		}

		if(glWasActive && !this.gl.isActive) {
			this.mapControl.panDidEnd(true);
		}

		if(!slWasActive && this.sl.isActive) {
			this.mapControl.zoomDidStart(true);
		}

		if(slWasActive && !this.sl.isActive) {
			this.mapControl.zoomDidEnd(true);
		}

		if (!rotWasActive && startRot != Double.MAX_VALUE)
		{
			this.mapControl.rotateDidStart(true);
		}

		if (rotWasActive && startRot == Double.MAX_VALUE)
		{
			this.mapControl.rotateDidEnd(true);
		}


		return true;
	}      
}
